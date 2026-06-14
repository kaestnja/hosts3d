#!/usr/bin/env python3
"""Siemens SCALANCE XR328 mirroring discovery via Net-SNMP.

This helper intentionally shells out to snmpget/snmpwalk instead of carrying an
SNMP library dependency. The current prototype only reads switch state; later
management tools may add controlled write operations when explicitly requested.

Common use cases:

1. Validate SNMPv3 credentials without running the full discovery:
   PowerShell:
     $env:SNMP_USER = "USER"
     $env:SNMP_AUTH_PASS = "AUTHPASS"
     $env:SNMP_PRIV_PASS = "PRIVPASS"
     python Tools/snmp/scalance_xr328_mirror_check.py 192.168.6.248 --version 3 --check-access-only --pretty

   POSIX shell:
     export SNMP_USER=USER
     export SNMP_AUTH_PASS=AUTHPASS
     export SNMP_PRIV_PASS=PRIVPASS
     python3 Tools/snmp/scalance_xr328_mirror_check.py 192.168.6.248 --version 3 --check-access-only --pretty

2. Run full SNMPv3 discovery after the access probe succeeds:
     python Tools/snmp/scalance_xr328_mirror_check.py 192.168.6.248 --version 3 --pretty

3. Run a lab-only SNMPv2c discovery:
     python Tools/snmp/scalance_xr328_mirror_check.py 192.168.6.248 --version 2c --community public --pretty

   If no SNMPv1/v2c community is given, the helper tries the usual defaults
   private and then public. The helper remains read-only; it does not call
   snmpset.

4. Use explicit Net-SNMP tools from a staged package root:
     python Tools/snmp/scalance_xr328_mirror_check.py SWITCH_IP --version 2c --community COMMUNITY --snmpget snmpget.exe --snmpwalk snmpwalk.exe --pretty

   The Hosts3D F9 integration passes the package-root snmpget/snmpwalk paths
   automatically when those tools are present.

5. Keep the first probe small when Bridge-FDB or LLDP is slow/noisy:
     python Tools/snmp/scalance_xr328_mirror_check.py 192.168.6.248 --version 2c --community public --skip-fdb --skip-lldp --pretty

6. Use the built-in Hosts3D F9 SCALANCE default and write both runtime outputs:
     python Tools/snmp/scalance_xr328_mirror_check.py --hosts3d-default --write-json hsd-data/snmp/sw6248xr328.json --write-topology hsd-data/switch-topology.txt --pretty

7. Use an optional Hosts3D switch config file instead of the built-in default:
     python Tools/snmp/scalance_xr328_mirror_check.py --config-file hsd-data/switches.txt --write-json hsd-data/snmp/sw6248xr328.json --write-topology hsd-data/switch-topology.txt --pretty

Example switches.txt line:
     switch name=sw6248xr328 type=scalance_xr328 host=SWITCH_IP version=2c community=COMMUNITY enabled=1 auto_refresh=1 refresh_seconds=60

Credentials are never printed as values. SNMPv1/v2c community values are
passed directly with --community or community=... in switches.txt. Use
environment variables only for SNMPv3 login/password values when they should
not live in the local switch config file.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shlex
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any


OID_SYS_DESCR = "1.3.6.1.2.1.1.1.0"
OID_SYS_OBJECT_ID = "1.3.6.1.2.1.1.2.0"
OID_SYS_NAME = "1.3.6.1.2.1.1.5.0"

OID_IF_DESCR = "1.3.6.1.2.1.2.2.1.2"
OID_IF_ADMIN_STATUS = "1.3.6.1.2.1.2.2.1.7"
OID_IF_OPER_STATUS = "1.3.6.1.2.1.2.2.1.8"
OID_IF_NAME = "1.3.6.1.2.1.31.1.1.1.1"
OID_IF_ALIAS = "1.3.6.1.2.1.31.1.1.1.18"

OID_MIRROR_BASE = "1.3.6.1.4.1.4329.20.1.1.1.1.1.6"
OID_MIRROR_STATUS = OID_MIRROR_BASE + ".1.0"
OID_MIRROR_TO_PORT = OID_MIRROR_BASE + ".2.0"
OID_MIRROR_INGRESS = OID_MIRROR_BASE + ".3.1.2"
OID_MIRROR_EGRESS = OID_MIRROR_BASE + ".3.1.3"
OID_MIRROR_CTRL_STATUS = OID_MIRROR_BASE + ".3.1.4"
OID_MIRROR_EXT_SESSION = OID_MIRROR_BASE + ".6"
OID_MIRROR_EXT_SRC = OID_MIRROR_BASE + ".7"
OID_MIRROR_EXT_DEST = OID_MIRROR_BASE + ".9"

OID_FDB_ADDRESS = "1.3.6.1.2.1.17.4.3.1.1"
OID_FDB_PORT = "1.3.6.1.2.1.17.4.3.1.2"
OID_BASE_PORT_IFINDEX = "1.3.6.1.2.1.17.1.4.1.2"
OID_Q_FDB_TABLE = "1.3.6.1.2.1.17.7.1.2.2"
OID_LLDP_LOC_PORT_ID = "1.0.8802.1.1.2.1.3.7.1.3"
OID_LLDP_LOC_PORT_DESC = "1.0.8802.1.1.2.1.3.7.1.4"
OID_LLDP_REM_TABLE = "1.0.8802.1.1.2.1.4.1"
OID_LLDP_REM_MAN_ADDR_TABLE = "1.0.8802.1.1.2.1.4.2"

OID_LLDP_REM_CHASSIS_ID_SUBTYPE = OID_LLDP_REM_TABLE + ".1.4"
OID_LLDP_REM_CHASSIS_ID = OID_LLDP_REM_TABLE + ".1.5"
OID_LLDP_REM_PORT_ID_SUBTYPE = OID_LLDP_REM_TABLE + ".1.6"
OID_LLDP_REM_PORT_ID = OID_LLDP_REM_TABLE + ".1.7"
OID_LLDP_REM_PORT_DESC = OID_LLDP_REM_TABLE + ".1.8"
OID_LLDP_REM_SYS_NAME = OID_LLDP_REM_TABLE + ".1.9"
OID_LLDP_REM_SYS_DESC = OID_LLDP_REM_TABLE + ".1.10"

STATUS_OK = "ok"
STATUS_PARTIAL = "partial_data"
STATUS_SNMP_UNREACHABLE = "snmp_unreachable"
STATUS_AUTH_FAILED = "auth_failed"
STATUS_OID_NOT_SUPPORTED = "oid_not_supported"
STATUS_EMPTY_TABLE = "empty_table"
STATUS_PARSE_ERROR = "parse_error"
STATUS_NOT_IMPLEMENTED = "not_implemented"
STATUS_UNKNOWN = "unknown"

DEFAULT_HOSTS3D_SWITCH_CONFIG = {
    "name": "sw6248xr328",
    "type": "scalance_xr328",
    "host": "192.168.6.248",
    "version": "2c",
}


@dataclass
class SnmpResult:
    status: str
    values: dict[str, str]
    error: str | None = None


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="SCALANCE XR328 mirroring discovery using snmpget/snmpwalk.",
    )
    parser.add_argument("host", nargs="?", help="Switch hostname or IP address")
    parser.add_argument("--config-file", dest="config_file", help="Read switch connection data from a Hosts3D switches.txt file")
    parser.add_argument("--profile-file", dest="config_file", help=argparse.SUPPRESS)
    parser.add_argument("--switch", dest="switch_name", help="Switch name from --config-file; defaults to the first enabled switch")
    parser.add_argument("--profile", dest="switch_name", help=argparse.SUPPRESS)
    parser.add_argument(
        "--hosts3d-default",
        action="store_true",
        help="Use the built-in Hosts3D SCALANCE XR328 lab default when no config file is selected",
    )
    parser.add_argument("--version", choices=["1", "2c", "3"], default="3")
    parser.add_argument("--port", type=int, default=161)
    parser.add_argument("--timeout", type=int, default=3)
    parser.add_argument("--retries", type=int, default=1)
    parser.add_argument("--community", default="")
    parser.add_argument("--user", default=os.getenv("SNMP_USER", ""))
    parser.add_argument("--level", choices=["noAuthNoPriv", "authNoPriv", "authPriv"], default="authPriv")
    parser.add_argument("--auth-proto", default=os.getenv("SNMP_AUTH_PROTO", "SHA"))
    parser.add_argument("--auth-pass", default=os.getenv("SNMP_AUTH_PASS", ""))
    parser.add_argument("--priv-proto", default=os.getenv("SNMP_PRIV_PROTO", "AES"))
    parser.add_argument("--priv-pass", default=os.getenv("SNMP_PRIV_PASS", ""))
    parser.add_argument("--snmpget", default="snmpget")
    parser.add_argument("--snmpwalk", default="snmpwalk")
    parser.add_argument("--skip-fdb", action="store_true", help="Skip Bridge-MIB FDB reads")
    parser.add_argument("--skip-lldp", action="store_true", help="Skip LLDP table read")
    parser.add_argument(
        "--check-access-only",
        action="store_true",
        help="Only query basic device identity; useful for validating SNMP credentials",
    )
    parser.add_argument("--write-json", help="Write the raw JSON result to this path")
    parser.add_argument("--write-topology", help="Write Hosts3D switch-topology.txt output to this path")
    parser.add_argument("--append-topology", action="store_true", help="Append to --write-topology instead of replacing it")
    parser.add_argument("--pretty", action="store_true", help="Pretty-print JSON")
    return parser.parse_args()


def parse_bool_text(value: str | None, default: bool = False) -> bool:
    if value is None:
        return default
    return value.strip().lower() in ("1", "true", "yes", "on", "enabled")


def parse_switch_config_line(line: str) -> dict[str, str] | None:
    line = line.strip()
    if not line or line[0] in "#;":
        return None
    try:
        parts = shlex.split(line, comments=True)
    except ValueError:
        return None
    if not parts or parts[0].lower() != "switch":
        return None
    config: dict[str, str] = {}
    for item in parts[1:]:
        if "=" not in item:
            continue
        key, value = item.split("=", 1)
        config[key.strip().lower()] = value.strip()
    return config if config else None


def parse_switch_profile_line(line: str) -> dict[str, str] | None:
    return parse_switch_config_line(line)


def load_switch_configs(path: str) -> list[dict[str, str]]:
    configs: list[dict[str, str]] = []
    try:
        with open(path, encoding="utf-8") as handle:
            for line in handle:
                config = parse_switch_config_line(line)
                if config:
                    configs.append(config)
    except OSError:
        return []
    return configs


def load_switch_profiles(path: str) -> list[dict[str, str]]:
    return load_switch_configs(path)


def env_or_value(config: dict[str, str], key: str, default: str = "") -> str:
    env_name = config.get(key + "_env", "")
    if env_name and os.getenv(env_name):
        return os.getenv(env_name, "")
    return config.get(key, default)


def apply_config_values(args: argparse.Namespace, config: dict[str, str]) -> None:
    args.host = args.host or config.get("host", "")
    args.version = config.get("version", args.version)
    args.port = int(config.get("port", args.port))
    args.timeout = int(config.get("timeout", args.timeout))
    args.retries = int(config.get("retries", args.retries))
    args.community = args.community or config.get("community", "")
    args.user = args.user or env_or_value(config, "user")
    args.level = config.get("level", args.level)
    args.auth_proto = config.get("auth_proto", args.auth_proto)
    args.auth_pass = args.auth_pass or env_or_value(config, "auth_pass")
    args.priv_proto = config.get("priv_proto", args.priv_proto)
    args.priv_pass = args.priv_pass or env_or_value(config, "priv_pass")


def apply_switch_config(args: argparse.Namespace) -> dict[str, str] | None:
    if not args.config_file:
        return None
    configs = load_switch_configs(args.config_file)
    selected: dict[str, str] | None = None
    for config in configs:
        if args.switch_name and config.get("name") != args.switch_name:
            continue
        if not args.switch_name and not parse_bool_text(config.get("enabled"), True):
            continue
        selected = config
        break
    if selected is None:
        return None
    apply_config_values(args, selected)
    return selected


def apply_profile(args: argparse.Namespace) -> dict[str, str] | None:
    return apply_switch_config(args)


def apply_hosts3d_default(args: argparse.Namespace) -> dict[str, str] | None:
    if not args.hosts3d_default:
        return None
    config = dict(DEFAULT_HOSTS3D_SWITCH_CONFIG)
    apply_config_values(args, config)
    return config


def command_exists(exe: str) -> bool:
    return shutil.which(exe) is not None


def snmp_base_args(args: argparse.Namespace, community: str | None = None) -> list[str]:
    cmd = ["-On", "-t", str(args.timeout), "-r", str(args.retries), "-v", args.version]
    if args.version in ("1", "2c"):
        cmd += ["-c", community if community is not None else args.community]
    else:
        cmd += ["-l", args.level, "-u", args.user]
        if args.level in ("authNoPriv", "authPriv"):
            cmd += ["-a", args.auth_proto, "-A", args.auth_pass]
        if args.level == "authPriv":
            cmd += ["-x", args.priv_proto, "-X", args.priv_pass]
    cmd += [f"{args.host}:{args.port}"]
    return cmd


def snmp_community_candidates(args: argparse.Namespace) -> list[tuple[str, str]]:
    if args.version not in ("1", "2c"):
        return [("", "not_applicable")]
    if args.community:
        return [(args.community, "provided")]
    return [("private", "default_private"), ("public", "default_public")]


def credential_state(args: argparse.Namespace) -> dict[str, str]:
    if args.version in ("1", "2c"):
        return {"community": "provided" if args.community else "default_probe_private_public"}
    state = {"user": "provided" if args.user else "missing"}
    if args.level in ("authNoPriv", "authPriv"):
        state["auth_pass"] = "provided" if args.auth_pass else "missing"
    else:
        state["auth_pass"] = "not_required"
    if args.level == "authPriv":
        state["priv_pass"] = "provided" if args.priv_pass else "missing"
    else:
        state["priv_pass"] = "not_required"
    return state


def missing_credentials(args: argparse.Namespace) -> list[str]:
    if args.version in ("1", "2c"):
        return []
    missing = []
    state = credential_state(args)
    for key, value in state.items():
        if value == "missing":
            missing.append(key)
    return missing


def classify_error(text: str) -> str:
    low = text.lower()
    if "authentication failure" in low or "authorizationerror" in low or "wrong digest" in low:
        return STATUS_AUTH_FAILED
    if "timeout" in low or "no response" in low:
        return STATUS_SNMP_UNREACHABLE
    if "no such object" in low or "no such instance" in low or "unknown object identifier" in low:
        return STATUS_OID_NOT_SUPPORTED
    return STATUS_UNKNOWN


def run_snmp(args: argparse.Namespace, tool: str, oid: str) -> SnmpResult:
    exe = args.snmpget if tool == "get" else args.snmpwalk
    if not command_exists(exe):
        return SnmpResult(STATUS_NOT_IMPLEMENTED, {}, f"{exe} not found as a command or explicit executable path")
    last: SnmpResult | None = None
    for community, community_source in snmp_community_candidates(args):
        cmd = [exe] + snmp_base_args(args, community if args.version in ("1", "2c") else None) + [oid]
        try:
            proc = subprocess.run(cmd, text=True, capture_output=True, timeout=args.timeout * (args.retries + 2) + 5)
        except subprocess.TimeoutExpired:
            last = SnmpResult(STATUS_SNMP_UNREACHABLE, {}, "SNMP command timed out")
            continue
        if proc.returncode != 0:
            err = (proc.stderr or proc.stdout or "").strip()
            last = SnmpResult(classify_error(err), {}, err)
            continue
        values = parse_snmp_lines(proc.stdout)
        if not values:
            last = SnmpResult(STATUS_EMPTY_TABLE if tool == "walk" else STATUS_PARSE_ERROR, {}, proc.stdout.strip())
            continue
        bad = "\n".join(values.values())
        bad_status = classify_error(bad)
        if bad_status != STATUS_UNKNOWN:
            last = SnmpResult(bad_status, values, bad)
            continue
        if args.version in ("1", "2c"):
            args.community = community
            args.community_source = community_source
        return SnmpResult(STATUS_OK, values)
    return last or SnmpResult(STATUS_UNKNOWN, {}, "SNMP command failed")


def parse_snmp_lines(text: str) -> dict[str, str]:
    values: dict[str, str] = {}
    for raw in text.splitlines():
        line = raw.strip()
        if not line:
            continue
        if " = " in line:
            oid, value = line.split(" = ", 1)
        else:
            parts = line.split(None, 1)
            if len(parts) != 2:
                continue
            oid, value = parts
        values[oid.strip().lstrip(".")] = value.strip()
    return values


def suffix(oid: str, base: str) -> str:
    oid = oid.lstrip(".")
    if oid == base:
        return ""
    return oid[len(base) + 1:] if oid.startswith(base + ".") else oid


def parse_int(value: str) -> int | None:
    match = re.search(r"(-?\d+)", value)
    return int(match.group(1)) if match else None


def parse_text(value: str) -> str | None:
    if "No Such" in value:
        return None
    if ":" in value:
        _, value = value.split(":", 1)
    value = value.strip()
    if len(value) >= 2 and value[0] == '"' and value[-1] == '"':
        value = value[1:-1]
    return value or None


def decode_enabled_disabled(value: str) -> bool | None:
    """Decode per-port Siemens mirror flags where enabled(1), disabled(2)."""
    low = value.lower()
    num = parse_int(value)
    if "enabled" in low or num == 1:
        return True
    if "disabled" in low or num == 2:
        return False
    return None


def decode_mirror_status(value: str) -> str:
    """Decode global Siemens mirror status where disabled(1), enabled(2)."""
    low = value.lower()
    num = parse_int(value)
    if "enabled" in low or num == 2:
        return "enabled"
    if "disabled" in low or num == 1:
        return "disabled"
    return STATUS_UNKNOWN


def decode_if_status(value: str) -> str:
    low = value.lower()
    num = parse_int(value)
    if "up" in low or num == 1:
        return "up"
    if "down" in low or num == 2:
        return "down"
    if "testing" in low or num == 3:
        return "testing"
    return STATUS_UNKNOWN


def mac_from_fdb_oid(oid: str) -> str | None:
    parts = oid.split(".")
    if len(parts) < 6:
        return None
    try:
        octets = [int(x) for x in parts[-6:]]
    except ValueError:
        return None
    if any(x < 0 or x > 255 for x in octets):
        return None
    return ":".join(f"{x:02X}" for x in octets)


def mac_from_value(value: str) -> str | None:
    hex_part = value.split(":", 1)[1] if ":" in value else value
    octets = re.findall(r"\b[0-9A-Fa-f]{2}\b", hex_part)
    if len(octets) >= 6:
        return ":".join(x.upper() for x in octets[:6])
    return None


def add_detail(
    result: dict[str, Any],
    component: str,
    status: str,
    message: str | None = None,
    optional: bool = False,
) -> None:
    item: dict[str, Any] = {"component": component, "status": status}
    if optional:
        item["optional"] = True
    if message:
        item["message"] = message
    result["details"].append(item)


def walk(args: argparse.Namespace, component: str, oid: str, result: dict[str, Any], optional: bool = False) -> dict[str, str]:
    got = walk_result(args, component, oid, result, optional)
    return got.values if got.status == STATUS_OK else {}


def walk_result(
    args: argparse.Namespace,
    component: str,
    oid: str,
    result: dict[str, Any],
    optional: bool = False,
) -> SnmpResult:
    got = run_snmp(args, "walk", oid)
    add_detail(result, component, got.status, got.error, optional)
    return got


def get(args: argparse.Namespace, component: str, oid: str, result: dict[str, Any]) -> str | None:
    got = get_result(args, component, oid, result)
    if got.status != STATUS_OK:
        return None
    return next(iter(got.values.values()), None)


def get_result(args: argparse.Namespace, component: str, oid: str, result: dict[str, Any]) -> SnmpResult:
    got = run_snmp(args, "get", oid)
    add_detail(result, component, got.status, got.error)
    return got


def build_interface_map(args: argparse.Namespace, result: dict[str, Any]) -> dict[int, dict[str, Any]]:
    interfaces: dict[int, dict[str, Any]] = {}
    fields = [
        ("if_descr", OID_IF_DESCR, "ifDescr"),
        ("if_name", OID_IF_NAME, "ifName"),
        ("if_alias", OID_IF_ALIAS, "ifAlias"),
        ("admin_status", OID_IF_ADMIN_STATUS, "ifAdminStatus"),
        ("oper_status", OID_IF_OPER_STATUS, "ifOperStatus"),
    ]
    for field, oid, component in fields:
        rows = walk(args, component, oid, result)
        for row_oid, value in rows.items():
            idx = parse_int(suffix(row_oid, oid))
            if idx is None:
                continue
            iface = interfaces.setdefault(idx, {"if_index": idx})
            iface[field] = decode_if_status(value) if field.endswith("_status") else parse_text(value)
    return interfaces


def interface_snapshot(interfaces: dict[int, dict[str, Any]], raw_id: int | None) -> dict[str, Any] | None:
    if raw_id is None:
        return None
    iface = dict(interfaces.get(raw_id, {}))
    iface.setdefault("if_index", raw_id)
    iface["raw_id"] = raw_id
    return iface


def apply_fdb(args: argparse.Namespace, result: dict[str, Any], interfaces: dict[int, dict[str, Any]]) -> None:
    if args.skip_fdb:
        add_detail(result, "bridge_fdb", STATUS_NOT_IMPLEMENTED, "skipped by argument")
        return
    fdb_addresses = walk(args, "dot1dTpFdbAddress", OID_FDB_ADDRESS, result)
    fdb_ports = walk(args, "dot1dTpFdbPort", OID_FDB_PORT, result)
    bridge_to_if = walk(args, "dot1dBasePortIfIndex", OID_BASE_PORT_IFINDEX, result)
    mac_by_suffix: dict[str, str] = {}
    for row_oid, value in fdb_addresses.items():
        row_suffix = suffix(row_oid, OID_FDB_ADDRESS)
        mac = mac_from_fdb_oid(row_oid) or mac_from_value(value)
        if mac:
            mac_by_suffix[row_suffix] = mac
    bridge_map: dict[int, int] = {}
    for row_oid, value in bridge_to_if.items():
        bridge_port = parse_int(suffix(row_oid, OID_BASE_PORT_IFINDEX))
        if_index = parse_int(value)
        if bridge_port is not None and if_index is not None:
            bridge_map[bridge_port] = if_index
    for row_oid, value in fdb_ports.items():
        row_suffix = suffix(row_oid, OID_FDB_PORT)
        mac = mac_by_suffix.get(row_suffix) or mac_from_fdb_oid(row_oid)
        bridge_port = parse_int(value)
        if mac is None or bridge_port is None:
            continue
        if_index = bridge_map.get(bridge_port)
        if if_index is None:
            continue
        iface = interfaces.setdefault(if_index, {"if_index": if_index})
        iface.setdefault("learned_macs", []).append(
            {"mac": mac, "source": "bridge_mib", "confidence": "learned_fdb"},
        )
    q_bridge = walk_result(args, "dot1qTpFdbTable", OID_Q_FDB_TABLE, result, optional=True)
    result["q_bridge_fdb"] = {
        "status": q_bridge.status,
        "raw_count": len(q_bridge.values) if q_bridge.status == STATUS_OK else 0,
        "entries": [],
    }


def raw_column_rows(rows: dict[str, str], base: str, suffix_names: list[str]) -> list[dict[str, Any]]:
    grouped: dict[tuple[int, ...], dict[str, Any]] = {}
    for row_oid, value in rows.items():
        parts = [parse_int(part) for part in suffix(row_oid, base).split(".") if part]
        if len(parts) < len(suffix_names) + 1 or any(part is None for part in parts):
            continue
        numeric_parts = [part for part in parts if part is not None]
        column = numeric_parts[0]
        index_parts = tuple(numeric_parts[1 : len(suffix_names) + 1])
        row = grouped.setdefault(index_parts, {name: part for name, part in zip(suffix_names, index_parts)})
        row.setdefault("raw_columns", {})[str(column)] = parse_text(value)
    return list(grouped.values())


def build_extended_mirroring_raw(
    ext_session: dict[str, str],
    ext_src: dict[str, str],
    ext_dest: dict[str, str],
    interfaces: dict[int, dict[str, Any]],
) -> dict[str, Any]:
    sessions = raw_column_rows(ext_session, OID_MIRROR_EXT_SESSION + ".1", ["session_id"])
    sources = raw_column_rows(ext_src, OID_MIRROR_EXT_SRC + ".1", ["session_id", "source_id"])
    destinations = raw_column_rows(ext_dest, OID_MIRROR_EXT_DEST + ".1", ["session_id", "destination_id"])
    for row in sources:
        iface = interface_snapshot(interfaces, row.get("source_id"))
        if iface:
            row["interface"] = iface
    for row in destinations:
        iface = interface_snapshot(interfaces, row.get("destination_id"))
        if iface:
            row["interface"] = iface
    return {
        "interpretation": "raw_grouped_by_observed_indices",
        "sessions": sessions,
        "sources": sources,
        "destinations": destinations,
    }


def lldp_index_from_suffix(row_suffix: str) -> tuple[int, int, int] | None:
    parts = [parse_int(part) for part in row_suffix.split(".") if part]
    if len(parts) < 3 or any(part is None for part in parts[:3]):
        return None
    numeric_parts = [part for part in parts if part is not None]
    return numeric_parts[0], numeric_parts[1], numeric_parts[2]


def lldp_man_addr_from_suffix(row_suffix: str) -> tuple[tuple[int, int, int], str | None] | None:
    parts = [parse_int(part) for part in row_suffix.split(".") if part]
    if len(parts) < 5 or any(part is None for part in parts[:5]):
        return None
    numeric_parts = [part for part in parts if part is not None]
    key = (numeric_parts[0], numeric_parts[1], numeric_parts[2])
    addr_subtype = numeric_parts[3]
    addr_len = numeric_parts[4]
    addr_parts = numeric_parts[5 : 5 + addr_len]
    if len(addr_parts) != addr_len:
        return key, None
    if addr_subtype == 1 and addr_len == 4:
        return key, ".".join(str(part) for part in addr_parts)
    return key, ".".join(str(part) for part in addr_parts)


def lldp_field(rows: dict[str, str], base: str, key: tuple[int, int, int]) -> str | None:
    wanted = ".".join(str(part) for part in key)
    for row_oid, value in rows.items():
        if suffix(row_oid, base) == wanted:
            return parse_text(value)
    return None


def build_lldp_neighbors(
    rem_rows: dict[str, str],
    man_addr_rows: dict[str, str],
    loc_port_ids: dict[str, str],
    loc_port_descs: dict[str, str],
) -> dict[int, list[dict[str, Any]]]:
    keys = set()
    for row_oid in rem_rows:
        for base in (
            OID_LLDP_REM_CHASSIS_ID_SUBTYPE,
            OID_LLDP_REM_CHASSIS_ID,
            OID_LLDP_REM_PORT_ID_SUBTYPE,
            OID_LLDP_REM_PORT_ID,
            OID_LLDP_REM_PORT_DESC,
            OID_LLDP_REM_SYS_NAME,
            OID_LLDP_REM_SYS_DESC,
        ):
            if row_oid.startswith(base + "."):
                key = lldp_index_from_suffix(suffix(row_oid, base))
                if key:
                    keys.add(key)
                break
    management_addresses: dict[tuple[int, int, int], list[str]] = {}
    man_addr_base = OID_LLDP_REM_MAN_ADDR_TABLE + ".1.3"
    for row_oid in man_addr_rows:
        if not row_oid.startswith(man_addr_base + "."):
            continue
        parsed = lldp_man_addr_from_suffix(suffix(row_oid, man_addr_base))
        if parsed is None:
            continue
        key, address = parsed
        if address and address != "0.0.0.0":
            management_addresses.setdefault(key, []).append(address)
    by_port: dict[int, list[dict[str, Any]]] = {}
    for key in sorted(keys):
        _time_mark, local_port_num, remote_index = key
        loc_port_id = parse_text(loc_port_ids.get(OID_LLDP_LOC_PORT_ID + "." + str(local_port_num), ""))
        loc_port_desc = parse_text(loc_port_descs.get(OID_LLDP_LOC_PORT_DESC + "." + str(local_port_num), ""))
        addresses = sorted(set(management_addresses.get(key, [])))
        neighbor = {
            "local_port_num": local_port_num,
            "local_port_id": loc_port_id,
            "local_port_description": loc_port_desc,
            "remote_index": remote_index,
            "chassis_id_subtype": parse_int(lldp_field(rem_rows, OID_LLDP_REM_CHASSIS_ID_SUBTYPE, key) or ""),
            "chassis_id": lldp_field(rem_rows, OID_LLDP_REM_CHASSIS_ID, key),
            "port_id_subtype": parse_int(lldp_field(rem_rows, OID_LLDP_REM_PORT_ID_SUBTYPE, key) or ""),
            "port_id": lldp_field(rem_rows, OID_LLDP_REM_PORT_ID, key),
            "port_description": lldp_field(rem_rows, OID_LLDP_REM_PORT_DESC, key),
            "system_name": lldp_field(rem_rows, OID_LLDP_REM_SYS_NAME, key),
            "system_description": lldp_field(rem_rows, OID_LLDP_REM_SYS_DESC, key),
            "management_address": addresses[0] if addresses else None,
            "management_addresses": addresses,
        }
        by_port.setdefault(local_port_num, []).append(neighbor)
    return by_port


def topology_safe_text(value: Any, fallback: str = "") -> str:
    text = str(value or fallback).strip()
    if not text:
        return fallback
    return re.sub(r"\s+", "_", text)


def topology_port_name(port: dict[str, Any]) -> str:
    return topology_safe_text(port.get("if_name") or port.get("if_descr") or port.get("if_alias"), f"P0.{port.get('if_index', 0)}")


def topology_is_switch_port(port: dict[str, Any]) -> bool:
    name = topology_safe_text(port.get("if_name"))
    if re.fullmatch(r"P\d+\.\d+", name):
        return True
    descr = topology_safe_text(port.get("if_descr")).lower()
    return "ethernet_port" in descr and "vlan" not in descr and "loopback" not in descr


def topology_host_mac(value: Any) -> str:
    text = str(value or "").strip()
    if re.fullmatch(r"[0-9A-Fa-f]{2}(:[0-9A-Fa-f]{2}){5}", text):
        return text.upper()
    return ""


def topology_append_host_line(lines: list[str], port_id: int, ip: str, mac: str, label: str, seen_hosts: set[tuple[int, str, str]]) -> None:
    key = (port_id, mac.upper(), ip)
    if key in seen_hosts:
        return
    seen_hosts.add(key)
    parts = ["host"]
    if ip:
        parts.append(f"ip={ip}")
    if mac:
        parts.append(f"mac={mac}")
    parts.append(f"port={port_id}")
    parts.append(f"label={topology_safe_text(label or ip or mac, 'host')}")
    lines.append(" ".join(parts))


def topology_lines(data: dict[str, Any], config: dict[str, str] | None = None) -> list[str]:
    device = data.get("device", {})
    mirroring = data.get("mirroring", {})
    interfaces = data.get("interfaces", [])
    name = topology_safe_text(
        (config or {}).get("name") or device.get("sys_name") or device.get("host"),
        "switch",
    )
    ports_by_id: dict[int, dict[str, Any]] = {}
    for iface in interfaces:
        idx = iface.get("if_index")
        if isinstance(idx, int) and idx > 0 and topology_is_switch_port(iface):
            ports_by_id[idx] = iface
    dest = mirroring.get("destination_port") or {}
    dest_id = dest.get("raw_id") or dest.get("if_index")
    source_by_id: dict[int, dict[str, Any]] = {}
    for port in mirroring.get("source_ports", []):
        raw_id = port.get("raw_id") or port.get("if_index")
        if isinstance(raw_id, int):
            source_by_id[raw_id] = port
            ports_by_id.setdefault(raw_id, port)
    if isinstance(dest_id, int):
        ports_by_id.setdefault(dest_id, dest)
    max_port = max([28] + [idx for idx in ports_by_id if idx <= 256])
    lines = [
        "# Generated by scalance_xr328_mirror_check.py. Edit with Hosts3D closed if you need manual corrections.",
        "# Raw SNMP JSON should be kept beside this file for diagnostics.",
        f"switch name={name} ports={max_port}",
    ]
    for port_id in sorted(idx for idx in ports_by_id if idx <= max_port):
        port = ports_by_id[port_id]
        role = "normal"
        if isinstance(dest_id, int) and port_id == dest_id:
            role = "destination"
        if port_id in source_by_id:
            src = source_by_id[port_id]
            ingress = src.get("ingress_mirroring") is True
            egress = src.get("egress_mirroring") is True
            role = "both" if ingress and egress else "ingress" if ingress else "egress"
        line = f"port id={port_id} name={topology_port_name(port)} role={role}"
        if isinstance(dest_id, int) and port_id in source_by_id:
            line += f" dest={dest_id}"
        if port.get("oper_status"):
            line += f" up={1 if port.get('oper_status') == 'up' else 0}"
        lines.append(line)
    seen_hosts: set[tuple[int, str, str]] = set()
    for port_id in sorted(ports_by_id):
        if port_id > max_port:
            continue
        port = ports_by_id[port_id]
        macs = []
        for mac_item in port.get("learned_macs", []):
            mac = topology_host_mac(mac_item.get("mac"))
            if mac and mac not in macs:
                macs.append(mac)
        neighbors = list(port.get("lldp_neighbors", []))
        used_macs: set[str] = set()
        for neighbor in neighbors:
            ip = topology_safe_text(neighbor.get("management_address"))
            label = topology_safe_text(neighbor.get("system_name") or neighbor.get("port_id") or neighbor.get("chassis_id"), "lldp_neighbor")
            mac = topology_host_mac(neighbor.get("chassis_id"))
            if not mac and len(macs) == 1 and len(neighbors) == 1:
                mac = macs[0]
            if mac:
                used_macs.add(mac)
            topology_append_host_line(lines, port_id, ip, mac, label, seen_hosts)
        for mac in macs:
            if mac in used_macs:
                continue
            topology_append_host_line(lines, port_id, "", mac, mac, seen_hosts)
    return lines


def write_text_atomic(path: str, content: str) -> None:
    target = Path(path)
    target.parent.mkdir(parents=True, exist_ok=True)
    tmp = target.with_name(target.name + ".tmp")
    tmp.write_text(content, encoding="utf-8", newline="\n")
    tmp.replace(target)


def append_text(path: str, content: str) -> None:
    target = Path(path)
    target.parent.mkdir(parents=True, exist_ok=True)
    with target.open("a", encoding="utf-8", newline="\n") as handle:
        handle.write(content)


def apply_lldp(args: argparse.Namespace, result: dict[str, Any]) -> dict[int, list[dict[str, Any]]]:
    if args.skip_lldp:
        add_detail(result, "lldp_mib", STATUS_NOT_IMPLEMENTED, "skipped by argument")
        return {}
    loc_port_ids = walk(args, "lldpLocPortId", OID_LLDP_LOC_PORT_ID, result, optional=True)
    loc_port_descs = walk(args, "lldpLocPortDesc", OID_LLDP_LOC_PORT_DESC, result, optional=True)
    rows = walk(args, "lldpRemTable", OID_LLDP_REM_TABLE, result)
    man_addr_rows = walk(args, "lldpRemManAddrTable", OID_LLDP_REM_MAN_ADDR_TABLE, result, optional=True)
    result["lldp_raw_count"] = len(rows)
    neighbors = build_lldp_neighbors(rows, man_addr_rows, loc_port_ids, loc_port_descs)
    result["lldp_neighbor_count"] = sum(len(port_neighbors) for port_neighbors in neighbors.values())
    return neighbors


def finalize_status(result: dict[str, Any]) -> None:
    statuses = {item["status"] for item in result["details"]}
    if STATUS_AUTH_FAILED in statuses:
        result["status"] = STATUS_AUTH_FAILED
    elif STATUS_SNMP_UNREACHABLE in statuses:
        result["status"] = STATUS_SNMP_UNREACHABLE
    effective_details = [
        item
        for item in result["details"]
        if not (item.get("optional") and item["status"] in (STATUS_EMPTY_TABLE, STATUS_OID_NOT_SUPPORTED))
    ]
    effective_statuses = {item["status"] for item in effective_details}
    if result["status"] != STATUS_OK:
        return
    if effective_statuses - {STATUS_OK, STATUS_EMPTY_TABLE, STATUS_OID_NOT_SUPPORTED}:
        result["status"] = STATUS_PARTIAL
    elif any(item["status"] != STATUS_OK for item in effective_details):
        result["status"] = STATUS_PARTIAL


def build_result(args: argparse.Namespace) -> dict[str, Any]:
    result: dict[str, Any] = {
        "status": STATUS_OK,
        "details": [],
        "device": {"host": args.host, "sys_name": None, "sys_descr": None, "sys_object_id": None},
        "snmp": {
            "version": "v" + args.version,
            "security_level": args.level if args.version == "3" else None,
            "auth_protocol": args.auth_proto if args.version == "3" and args.level in ("authNoPriv", "authPriv") else None,
            "privacy_protocol": args.priv_proto if args.version == "3" and args.level == "authPriv" else None,
            "credential_state": credential_state(args),
            "community_source": "provided" if args.version in ("1", "2c") and args.community else "default_probe_private_public" if args.version in ("1", "2c") else None,
            "access_probe": STATUS_UNKNOWN,
        },
        "mirroring": {
            "global_status": STATUS_UNKNOWN,
            "destination_port": None,
            "source_ports": [],
            "extended_mirroring": STATUS_UNKNOWN,
            "extended_mirroring_raw": {"interpretation": "not_checked", "sessions": [], "sources": [], "destinations": []},
        },
        "q_bridge_fdb": {"status": STATUS_UNKNOWN, "raw_count": 0, "entries": []},
        "notes": [
            "This prototype currently reads state only; controlled write operations belong in a later explicit management step.",
            "MAC-to-port values are learned FDB data and may age out.",
            "IP and hostname data are indirect correlations unless provided by LLDP or another explicit source.",
        ],
    }
    sys_descr = get_result(args, "sysDescr", OID_SYS_DESCR, result)
    result["snmp"]["credential_state"] = credential_state(args)
    if args.version in ("1", "2c"):
        result["snmp"]["community_source"] = getattr(args, "community_source", "provided" if args.community else "default_probe_private_public")
    result["snmp"]["access_probe"] = sys_descr.status
    result["device"]["sys_descr"] = parse_text(next(iter(sys_descr.values.values()), "") if sys_descr.status == STATUS_OK else "")
    result["device"]["sys_object_id"] = parse_text(get(args, "sysObjectID", OID_SYS_OBJECT_ID, result) or "")
    result["device"]["sys_name"] = parse_text(get(args, "sysName", OID_SYS_NAME, result) or "")

    if args.check_access_only:
        finalize_status(result)
        return result

    interfaces = build_interface_map(args, result)
    mirror_status = get(args, "snMspsConfigMirrorStatus", OID_MIRROR_STATUS, result)
    if mirror_status is not None:
        result["mirroring"]["global_status"] = decode_mirror_status(mirror_status)
    dest_raw = parse_int(get(args, "snMspsConfigMirrorToPort", OID_MIRROR_TO_PORT, result) or "")
    result["mirroring"]["destination_port"] = interface_snapshot(interfaces, dest_raw)

    ingress = walk(args, "snMspsConfigMirrorCtrlIngressMirroring", OID_MIRROR_INGRESS, result)
    egress = walk(args, "snMspsConfigMirrorCtrlEgressMirroring", OID_MIRROR_EGRESS, result)
    ctrl_status = walk(args, "snMspsConfigMirrorCtrlStatus", OID_MIRROR_CTRL_STATUS, result)

    ext_session = walk(args, "snMspsConfigMirrorCtrlExtnTable", OID_MIRROR_EXT_SESSION, result)
    ext_src = walk(args, "snMspsConfigMirrorCtrlExtnSrcTable", OID_MIRROR_EXT_SRC, result)
    ext_dest = walk(args, "snMspsConfigMirrorCtrlExtnDestinationTable", OID_MIRROR_EXT_DEST, result)
    result["mirroring"]["extended_mirroring"] = "supported" if (ext_session or ext_src or ext_dest) else STATUS_EMPTY_TABLE
    result["mirroring"]["extended_mirroring_raw"] = build_extended_mirroring_raw(
        ext_session,
        ext_src,
        ext_dest,
        interfaces,
    )

    apply_fdb(args, result, interfaces)
    lldp_neighbors = apply_lldp(args, result)
    for raw_id, neighbors in lldp_neighbors.items():
        iface = interfaces.setdefault(raw_id, {"if_index": raw_id})
        iface["lldp_neighbors"] = neighbors

    source_ids = set()
    for rows in (ingress, egress):
        for row_oid in rows:
            idx = parse_int(suffix(row_oid, OID_MIRROR_INGRESS if rows is ingress else OID_MIRROR_EGRESS))
            if idx is not None:
                source_ids.add(idx)
    for raw_id in sorted(source_ids):
        in_val = next((v for k, v in ingress.items() if parse_int(suffix(k, OID_MIRROR_INGRESS)) == raw_id), "")
        eg_val = next((v for k, v in egress.items() if parse_int(suffix(k, OID_MIRROR_EGRESS)) == raw_id), "")
        in_flag = decode_enabled_disabled(in_val)
        eg_flag = decode_enabled_disabled(eg_val)
        if in_flag is not True and eg_flag is not True:
            continue
        port = interface_snapshot(interfaces, raw_id) or {"raw_id": raw_id, "if_index": raw_id}
        port["ingress_mirroring"] = in_flag
        port["egress_mirroring"] = eg_flag
        status_val = next((v for k, v in ctrl_status.items() if parse_int(suffix(k, OID_MIRROR_CTRL_STATUS)) == raw_id), "")
        port["mirror_ctrl_status_raw"] = parse_text(status_val)
        port["lldp_neighbors"] = lldp_neighbors.get(raw_id, [])
        port.setdefault("ip_correlations", [])
        result["mirroring"]["source_ports"].append(port)

    result["interfaces"] = [interfaces[key] for key in sorted(interfaces)]
    finalize_status(result)
    return result


def main() -> int:
    args = parse_args()
    switch_config = apply_switch_config(args) or apply_hosts3d_default(args)
    if not args.host:
        data = {
            "status": STATUS_AUTH_FAILED,
            "details": [
                {
                    "component": "switch_config",
                    "status": STATUS_AUTH_FAILED,
                    "message": "Missing switch host. Provide HOST, --hosts3d-default, or a switch line with host=... in --config-file.",
                },
            ],
        }
        text = json.dumps(data, indent=2 if args.pretty else None, sort_keys=False)
        if args.write_json:
            write_text_atomic(args.write_json, text + "\n")
        print(text)
        return 2
    missing = missing_credentials(args)
    if missing:
        data = {
            "status": STATUS_AUTH_FAILED,
            "details": [
                {
                    "component": "snmp_credentials",
                    "status": STATUS_AUTH_FAILED,
                    "message": "Missing SNMP credential fields: " + ", ".join(missing),
                },
            ],
            "snmp": {
                "version": "v" + args.version,
                "security_level": args.level if args.version == "3" else None,
                "auth_protocol": args.auth_proto if args.version == "3" and args.level in ("authNoPriv", "authPriv") else None,
                "privacy_protocol": args.priv_proto if args.version == "3" and args.level == "authPriv" else None,
                "credential_state": credential_state(args),
                "access_probe": "not_run",
            },
        }
        text = json.dumps(data, indent=2 if args.pretty else None, sort_keys=False)
        if args.write_json:
            write_text_atomic(args.write_json, text + "\n")
        print(text)
        return 2
    data = build_result(args)
    text = json.dumps(data, indent=2 if args.pretty else None, sort_keys=False)
    if args.write_json:
        write_text_atomic(args.write_json, text + "\n")
    if args.write_topology and data["status"] in (STATUS_OK, STATUS_PARTIAL):
        topology_text = "\n".join(topology_lines(data, switch_config)) + "\n"
        if args.append_topology:
            append_text(args.write_topology, topology_text)
        else:
            write_text_atomic(args.write_topology, topology_text)
    print(text)
    return 0 if data["status"] in (STATUS_OK, STATUS_PARTIAL) else 1


if __name__ == "__main__":
    sys.exit(main())
