#!/usr/bin/env python3
"""Siemens SCALANCE XC208G mirroring discovery via Net-SNMP.

This helper is the XC208G sibling of scalance_xr328_mirror_check.py. It keeps
the same JSON contract and the same read-only Net-SNMP command flow while
using XC208G defaults and an 8-port topology export.

Common use cases:

1. Validate SNMPv3 credentials without running the full discovery:
     python Tools/snmp/scalance_xc208g_mirror_check.py SWITCH_IP --version 3 --check-access-only --pretty

2. Run full SNMPv3 discovery after the access probe succeeds:
     python Tools/snmp/scalance_xc208g_mirror_check.py SWITCH_IP --version 3 --pretty

3. Run a lab-only SNMPv2c discovery:
     python Tools/snmp/scalance_xc208g_mirror_check.py SWITCH_IP --version 2c --community public --pretty

   If no SNMPv1/v2c community is given, the helper tries the usual defaults
   private and then public. The helper remains read-only; it does not call
   snmpset.

4. Use explicit Net-SNMP tools from a staged package root:
     python Tools/snmp/scalance_xc208g_mirror_check.py SWITCH_IP --version 2c --community COMMUNITY --snmpget snmpget.exe --snmpwalk snmpwalk.exe --pretty

5. Use an optional Hosts3D switch config file:
     python Tools/snmp/scalance_xc208g_mirror_check.py --config-file hsd-data/switches.txt --switch swxc208g --write-json hsd-data/scalance_xc208g_mirror_check.json --write-topology hsd-data/switch-topology.txt --pretty

Example switches.txt line:
     switch name=swxc208g type=scalance_xc208g host=SWITCH_IP version=2c community=COMMUNITY enabled=1 auto_refresh=1 refresh_seconds=60

Credentials are never printed as values. SNMPv1/v2c community values are
passed directly with --community or community=... in switches.txt. Use
environment variables only for SNMPv3 login/password values when they should
not live in the local switch config file.
"""

from __future__ import annotations

import argparse
import importlib.util
import os
import shutil
import sys
from pathlib import Path
from typing import Any


XR328_SCRIPT = Path(__file__).with_name("scalance_xr328_mirror_check.py")
SPEC = importlib.util.spec_from_file_location("scalance_xr328_mirror_check_base", XR328_SCRIPT)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"Unable to load {XR328_SCRIPT}")
base = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = base
SPEC.loader.exec_module(base)
_base_topology_lines = base.topology_lines


DEFAULT_HOSTS3D_SWITCH_CONFIG = {
    "name": "swxc208g",
    "type": "scalance_xc208g",
    "host": "",
    "version": "2c",
}


def find_local_tool(name: str) -> str:
    if shutil.which(name):
        return name

    script = Path(__file__).resolve()
    repo_or_package_root = script.parents[2]
    candidates = [
        repo_or_package_root / name,
        repo_or_package_root / "Debug" / "windows" / "x64" / name,
        repo_or_package_root / "Release" / "windows" / "x64" / name,
        repo_or_package_root / "Release" / "windows" / "x86" / name,
    ]
    for candidate in candidates:
        if candidate.is_file():
            return str(candidate)
    return name


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="SCALANCE XC208G mirroring discovery using snmpget/snmpwalk.",
    )
    parser.add_argument("host", nargs="?", help="Switch hostname or IP address")
    parser.add_argument("--config-file", dest="config_file", help="Read switch connection data from a Hosts3D switches.txt file")
    parser.add_argument("--profile-file", dest="config_file", help=argparse.SUPPRESS)
    parser.add_argument("--switch", dest="switch_name", help="Switch name from --config-file; defaults to the first enabled switch")
    parser.add_argument("--profile", dest="switch_name", help=argparse.SUPPRESS)
    parser.add_argument(
        "--hosts3d-default",
        action="store_true",
        help="Use the built-in SCALANCE XC208G profile when no config file is selected; host must still be provided",
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
    parser.add_argument("--snmpget", default=find_local_tool("snmpget.exe" if os.name == "nt" else "snmpget"))
    parser.add_argument("--snmpwalk", default=find_local_tool("snmpwalk.exe" if os.name == "nt" else "snmpwalk"))
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


def apply_hosts3d_default(args: argparse.Namespace) -> dict[str, str] | None:
    if not args.hosts3d_default:
        return None
    config = dict(DEFAULT_HOSTS3D_SWITCH_CONFIG)
    base.apply_config_values(args, config)
    return config


def topology_lines(data: dict[str, Any], config: dict[str, str] | None = None) -> list[str]:
    lines = _base_topology_lines(data, config)
    if lines:
        lines[0] = "# Generated by scalance_xc208g_mirror_check.py. Edit with Hosts3D closed if you need manual corrections."
    for idx, line in enumerate(lines):
        if line.startswith("switch "):
            parts = [part for part in line.split() if not part.startswith("ports=")]
            parts.append("ports=8")
            lines[idx] = " ".join(parts)
            break
    return lines


base.DEFAULT_HOSTS3D_SWITCH_CONFIG = DEFAULT_HOSTS3D_SWITCH_CONFIG
base.parse_args = parse_args
base.apply_hosts3d_default = apply_hosts3d_default
base.topology_lines = topology_lines


def main() -> int:
    return base.main()


if __name__ == "__main__":
    raise SystemExit(main())
