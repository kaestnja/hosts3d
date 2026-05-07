#!/usr/bin/env python3
"""Siemens SCALANCE XR328 mirroring discovery via Net-SNMP.

This helper intentionally shells out to snmpget/snmpwalk instead of carrying an
SNMP library dependency. The current prototype only reads switch state; later
management tools may add controlled write operations when explicitly requested.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
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
OID_LLDP_REM_TABLE = "1.0.8802.1.1.2.1.4.1"

STATUS_OK = "ok"
STATUS_PARTIAL = "partial_data"
STATUS_SNMP_UNREACHABLE = "snmp_unreachable"
STATUS_AUTH_FAILED = "auth_failed"
STATUS_OID_NOT_SUPPORTED = "oid_not_supported"
STATUS_EMPTY_TABLE = "empty_table"
STATUS_PARSE_ERROR = "parse_error"
STATUS_NOT_IMPLEMENTED = "not_implemented"
STATUS_UNKNOWN = "unknown"


@dataclass
class SnmpResult:
    status: str
    values: dict[str, str]
    error: str | None = None


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="SCALANCE XR328 mirroring discovery using snmpget/snmpwalk.",
    )
    parser.add_argument("host", help="Switch hostname or IP address")
    parser.add_argument("--version", choices=["1", "2c", "3"], default="3")
    parser.add_argument("--port", type=int, default=161)
    parser.add_argument("--timeout", type=int, default=3)
    parser.add_argument("--retries", type=int, default=1)
    parser.add_argument("--community", default=os.getenv("SNMP_COMMUNITY", ""))
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
    parser.add_argument("--pretty", action="store_true", help="Pretty-print JSON")
    return parser.parse_args()


def command_exists(exe: str) -> bool:
    return shutil.which(exe) is not None


def snmp_base_args(args: argparse.Namespace) -> list[str]:
    cmd = ["-On", "-t", str(args.timeout), "-r", str(args.retries), "-v", args.version]
    if args.version in ("1", "2c"):
        cmd += ["-c", args.community]
    else:
        cmd += ["-l", args.level, "-u", args.user]
        if args.level in ("authNoPriv", "authPriv"):
            cmd += ["-a", args.auth_proto, "-A", args.auth_pass]
        if args.level == "authPriv":
            cmd += ["-x", args.priv_proto, "-X", args.priv_pass]
    cmd += [f"{args.host}:{args.port}"]
    return cmd


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
    cmd = [exe] + snmp_base_args(args) + [oid]
    try:
        proc = subprocess.run(cmd, text=True, capture_output=True, timeout=args.timeout * (args.retries + 2) + 5)
    except subprocess.TimeoutExpired:
        return SnmpResult(STATUS_SNMP_UNREACHABLE, {}, "SNMP command timed out")
    if proc.returncode != 0:
        err = (proc.stderr or proc.stdout or "").strip()
        return SnmpResult(classify_error(err), {}, err)
    values = parse_snmp_lines(proc.stdout)
    if not values:
        return SnmpResult(STATUS_EMPTY_TABLE if tool == "walk" else STATUS_PARSE_ERROR, {}, proc.stdout.strip())
    bad = "\n".join(values.values())
    bad_status = classify_error(bad)
    if bad_status != STATUS_UNKNOWN:
        return SnmpResult(bad_status, values, bad)
    return SnmpResult(STATUS_OK, values)


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
    low = value.lower()
    num = parse_int(value)
    if "enabled" in low or num == 1:
        return True
    if "disabled" in low or num == 2:
        return False
    return None


def decode_mirror_status(value: str) -> str:
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


def add_detail(result: dict[str, Any], component: str, status: str, message: str | None = None) -> None:
    item: dict[str, Any] = {"component": component, "status": status}
    if message:
        item["message"] = message
    result["details"].append(item)


def walk(args: argparse.Namespace, component: str, oid: str, result: dict[str, Any]) -> dict[str, str]:
    got = run_snmp(args, "walk", oid)
    add_detail(result, component, got.status, got.error)
    return got.values if got.status == STATUS_OK else {}


def get(args: argparse.Namespace, component: str, oid: str, result: dict[str, Any]) -> str | None:
    got = run_snmp(args, "get", oid)
    add_detail(result, component, got.status, got.error)
    if got.status != STATUS_OK:
        return None
    return next(iter(got.values.values()), None)


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


def apply_lldp(args: argparse.Namespace, result: dict[str, Any]) -> None:
    if args.skip_lldp:
        add_detail(result, "lldp_mib", STATUS_NOT_IMPLEMENTED, "skipped by argument")
        return
    rows = walk(args, "lldpRemTable", OID_LLDP_REM_TABLE, result)
    result["lldp_raw_count"] = len(rows)


def build_result(args: argparse.Namespace) -> dict[str, Any]:
    result: dict[str, Any] = {
        "status": STATUS_OK,
        "details": [],
        "device": {"host": args.host, "sys_name": None, "sys_descr": None, "sys_object_id": None},
        "snmp": {"version": "v" + args.version, "security_level": args.level if args.version == "3" else None},
        "mirroring": {
            "global_status": STATUS_UNKNOWN,
            "destination_port": None,
            "source_ports": [],
            "extended_mirroring": STATUS_UNKNOWN,
        },
        "notes": [
            "This prototype currently reads state only; controlled write operations belong in a later explicit management step.",
            "MAC-to-port values are learned FDB data and may age out.",
            "IP and hostname data are indirect correlations unless provided by LLDP or another explicit source.",
        ],
    }
    result["device"]["sys_descr"] = parse_text(get(args, "sysDescr", OID_SYS_DESCR, result) or "")
    result["device"]["sys_object_id"] = parse_text(get(args, "sysObjectID", OID_SYS_OBJECT_ID, result) or "")
    result["device"]["sys_name"] = parse_text(get(args, "sysName", OID_SYS_NAME, result) or "")

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

    apply_fdb(args, result, interfaces)
    apply_lldp(args, result)

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
        port.setdefault("lldp_neighbors", [])
        port.setdefault("ip_correlations", [])
        result["mirroring"]["source_ports"].append(port)

    statuses = {item["status"] for item in result["details"]}
    if STATUS_AUTH_FAILED in statuses:
        result["status"] = STATUS_AUTH_FAILED
    elif STATUS_SNMP_UNREACHABLE in statuses:
        result["status"] = STATUS_SNMP_UNREACHABLE
    elif statuses - {STATUS_OK, STATUS_EMPTY_TABLE, STATUS_OID_NOT_SUPPORTED}:
        result["status"] = STATUS_PARTIAL
    elif any(item["status"] != STATUS_OK for item in result["details"]):
        result["status"] = STATUS_PARTIAL
    return result


def main() -> int:
    args = parse_args()
    if args.version in ("1", "2c") and not args.community:
        print(json.dumps({"status": STATUS_AUTH_FAILED, "details": [{"component": "snmp", "status": STATUS_AUTH_FAILED, "message": "SNMP community missing"}]}))
        return 2
    if args.version == "3" and not args.user:
        print(json.dumps({"status": STATUS_AUTH_FAILED, "details": [{"component": "snmp", "status": STATUS_AUTH_FAILED, "message": "SNMPv3 user missing"}]}))
        return 2
    data = build_result(args)
    print(json.dumps(data, indent=2 if args.pretty else None, sort_keys=False))
    return 0 if data["status"] in (STATUS_OK, STATUS_PARTIAL) else 1


if __name__ == "__main__":
    sys.exit(main())
