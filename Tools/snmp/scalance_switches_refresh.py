#!/usr/bin/env python3
"""Serial SCALANCE switch refresh for Hosts3D."""

from __future__ import annotations

import argparse
import os
import shlex
import subprocess
import sys
from pathlib import Path


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
    parser.add_argument("--scriptcommands-path", required=True, help="Existing shared ScriptCommands checkout")
    parser.add_argument("--application-dir", default=str(Path.cwd()), help="Base directory for relative checkout paths")
    parser.add_argument("--snmpget", default="")
    parser.add_argument("--snmpwalk", default="")
    parser.add_argument("--pretty", action="store_true")
    return parser.parse_args()


def resolve_commands_path(configured: str, application_dir: str) -> Path:
    if not configured.strip():
        raise ValueError("Set scriptcommands_path in settings.ini to the existing shared checkout")
    path = Path(configured.strip())
    if path.anchor and not path.is_absolute():
        raise ValueError("Use an absolute or application-relative scriptcommands_path")
    return (path if path.is_absolute() else Path(application_dir) / path).resolve()


def connection_arguments(config: dict[str, str]) -> list[str]:
    if not config.get("host"):
        raise ValueError("An enabled switch is missing host=... in switches.txt")
    arguments = [config["host"]]
    # Hosts3D owns this file format. Shared commands receive only method parameters.
    for key in ("name", "version", "port", "timeout", "retries", "community", "community_fallback", "user", "level",
                "auth_proto", "auth_pass", "priv_proto", "priv_pass"):
        value = config.get(key, "")
        if key in ("user", "auth_pass", "priv_pass") and config.get(key + "_env"):
            value = os.getenv(config[key + "_env"], value)
        if value:
            arguments.extend(["--" + key.replace("_", "-"), value])
    return arguments


def main() -> int:
    args = parse_args()
    script_dir = resolve_commands_path(args.scriptcommands_path, args.application_dir)
    config_file = Path(args.config_file)
    json_dir = Path(args.json_dir)
    topology_path = Path(args.write_topology)
    switches = load_switches(config_file)

    if not switches:
        print("no enabled switches found", file=sys.stderr)
        return 1

    json_dir.mkdir(parents=True, exist_ok=True)
    topology_path.parent.mkdir(parents=True, exist_ok=True)
    temp_topology_path = topology_path.with_name(f"{topology_path.name}.{os.getpid()}.refreshing")
    if temp_topology_path.exists():
        temp_topology_path.unlink()

    failures = 0
    wrote_topology = False
    try:
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
            cmd = [sys.executable, str(helper), *connection_arguments(config)]
            cmd += ["--write-json", str(json_path), "--write-topology", str(temp_topology_path)]
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

        if wrote_topology and failures == 0:
            temp_topology_path.replace(topology_path)
    finally:
        if temp_topology_path.exists():
            temp_topology_path.unlink()

    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
