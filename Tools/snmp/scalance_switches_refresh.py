#!/usr/bin/env python3
"""Serial SCALANCE switch refresh for Hosts3D."""

from __future__ import annotations

import argparse
import shlex
import subprocess
import sys
from pathlib import Path


DEFAULT_SWITCH = {
    "name": "sw6248xr328",
    "type": "scalance_xr328",
    "host": "192.168.6.248",
    "version": "2c",
}


def parse_bool(value: str | None, default: bool = False) -> bool:
    if value is None:
        return default
    return value.strip().lower() in ("1", "true", "yes", "on", "enabled")


def parse_switch_line(line: str) -> dict[str, str] | None:
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
    return config or None


def load_switches(path: Path) -> list[dict[str, str]]:
    switches: list[dict[str, str]] = []
    if not path.is_file():
        return switches
    for line in path.read_text(encoding="utf-8").splitlines():
        config = parse_switch_line(line)
        if config and parse_bool(config.get("enabled"), True):
            switches.append(config)
    return switches


def safe_name(config: dict[str, str], index: int) -> str:
    raw = config.get("name") or config.get("host") or f"switch{index + 1}"
    chars = []
    for ch in raw:
        chars.append(ch if (ch.isalnum() or ch in ("-", "_", ".")) else "_")
    return "".join(chars).strip("._") or f"switch{index + 1}"


def helper_for_type(switch_type: str) -> str | None:
    normalized = switch_type.strip().lower()
    if normalized in ("", "scalance_xr328", "xr328"):
        return "scalance_xr328_mirror_check.py"
    if normalized in ("scalance_xc208g", "xc208g"):
        return "scalance_xc208g_mirror_check.py"
    return None


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Refresh all enabled SCALANCE switches serially.")
    parser.add_argument("--config-file", default="hsd-data/switches.txt")
    parser.add_argument("--json-dir", default="hsd-data/snmp")
    parser.add_argument("--write-topology", default="hsd-data/switch-topology.txt")
    parser.add_argument("--snmpget", default="")
    parser.add_argument("--snmpwalk", default="")
    parser.add_argument("--pretty", action="store_true")
    parser.add_argument("--use-default-if-empty", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    script_dir = Path(__file__).resolve().parent
    config_file = Path(args.config_file)
    json_dir = Path(args.json_dir)
    topology_path = Path(args.write_topology)
    switches = load_switches(config_file)

    using_default = False
    if not switches and args.use_default_if_empty:
        switches = [dict(DEFAULT_SWITCH)]
        using_default = True

    if not switches:
        print("no enabled switches found", file=sys.stderr)
        return 1

    json_dir.mkdir(parents=True, exist_ok=True)
    topology_path.parent.mkdir(parents=True, exist_ok=True)
    if topology_path.exists():
        topology_path.unlink()

    failures = 0
    wrote_topology = False
    for index, config in enumerate(switches):
        helper_name = helper_for_type(config.get("type", ""))
        if not helper_name:
            print(f"unsupported switch type for {config.get('name') or config.get('host')}: {config.get('type')}", file=sys.stderr)
            failures += 1
            continue
        helper = script_dir / helper_name
        if not helper.is_file():
            print(f"missing helper: {helper}", file=sys.stderr)
            failures += 1
            continue

        json_path = json_dir / f"{safe_name(config, index)}.json"
        cmd = [sys.executable, str(helper)]
        if using_default:
            cmd.append("--hosts3d-default")
        else:
            cmd += ["--config-file", str(config_file)]
            if config.get("name"):
                cmd += ["--switch", config["name"]]
            elif config.get("host"):
                cmd.append(config["host"])
        cmd += ["--write-json", str(json_path), "--write-topology", str(topology_path)]
        if wrote_topology:
            cmd.append("--append-topology")
        if args.snmpget:
            cmd += ["--snmpget", args.snmpget]
        if args.snmpwalk:
            cmd += ["--snmpwalk", args.snmpwalk]
        if args.pretty:
            cmd.append("--pretty")

        proc = subprocess.run(cmd)
        if proc.returncode:
            failures += 1
        else:
            wrote_topology = True

    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
