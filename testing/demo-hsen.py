#!/usr/bin/env python3
"""
Short synthetic visualization demo for Hosts3D.

Runs a deterministic sequence of synthetic hsen-compatible bursts and reports
estimated versus actual runtime per step and overall.
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
import time


def write_line(text: str, log_path: str) -> None:
    print(text)
    if log_path:
        log_dir = os.path.dirname(log_path)
        if log_dir:
            os.makedirs(log_dir, exist_ok=True)
        with open(log_path, "a", encoding="utf-8") as handle:
            handle.write(text + "\n")


def mode_sleep_factor(mode: str, burst_packets: int = 1) -> float:
    return {
        "tcphandshake": 3.0,
        "tcpack": 2.0,
        "tcppayload": 2.0,
        "tcpreset": 3.0,
        "udppayload": 2.0,
        "icmpecho": 2.0,
        "icmpburst": 2.0 * burst_packets,
        "arp": 1.0,
        "discovery": 3.0,
    }.get(mode, 2.0)


def stage_estimate_seconds(stage: dict[str, object]) -> float:
    burst_packets = int(stage.get("burst_packets", 1))
    factor = mode_sleep_factor(str(stage["mode"]), burst_packets)
    return (int(stage["count"]) * int(stage["delay_ms"]) * factor) / 1000.0 + 0.20


def run_stage(stage: dict[str, object], args: argparse.Namespace, sender_path: str, log_path: str) -> tuple[float, float]:
    estimate = stage_estimate_seconds(stage)
    cmd = [
        sys.executable,
        sender_path,
        "--mode",
        str(stage["mode"]),
        "--host",
        args.host,
        "--port",
        str(args.port),
        "--sensor",
        str(args.sensor),
        "--count",
        str(stage["count"]),
        "--delay",
        str(int(stage["delay_ms"]) / 1000.0),
    ]
    if args.center_ip:
        cmd.extend(["--center-ip", args.center_ip, "--center-role", args.center_role])
    if stage.get("tcp_port") is not None:
        cmd.extend(["--tcp-port", str(stage["tcp_port"])])
    if stage.get("udp_port") is not None:
        cmd.extend(["--udp-port", str(stage["udp_port"])])
    if stage.get("burst_packets") is not None:
        cmd.extend(["--burst-packets", str(stage["burst_packets"])])
    if stage.get("discovery_kind") is not None:
        cmd.extend(["--discovery-kind", str(stage["discovery_kind"])])
    if stage.get("discovery_name") is not None:
        cmd.extend(["--discovery-name", str(stage["discovery_name"])])
    if stage.get("wide_host_spread"):
        cmd.append("--wide-host-spread")

    write_line(f"[{stage['name']}] {stage['mode']}  est. {estimate:.1f}s", log_path)
    started = time.perf_counter()
    subprocess.run(cmd, check=True)
    actual = time.perf_counter() - started
    write_line(f"[{stage['name']}] done in {actual:.1f}s", log_path)
    time.sleep(args.pause_between_steps_ms / 1000.0)
    return estimate, actual


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=10111)
    parser.add_argument("--sensor", type=int, default=1)
    parser.add_argument("--center-ip", default="")
    parser.add_argument("--center-role", choices=("mixed", "source", "destination"), default="mixed")
    parser.add_argument("--pause-between-steps-ms", type=int, default=150)
    parser.add_argument("--log", default="")
    parser.add_argument("--state", default="")
    args = parser.parse_args()

    script_dir = os.path.dirname(os.path.abspath(__file__))
    sender_candidates = (
        os.path.join(script_dir, "sim-hsen.py"),
        os.path.join(script_dir, "testing", "sim-hsen.py"),
    )
    sender_path = next((path for path in sender_candidates if os.path.isfile(path)), "")
    if not sender_path:
        raise SystemExit(
            "Missing sender script near demo-hsen.py "
            "(expected sim-hsen.py beside it or under ./testing/)."
        )

    if args.log:
        with open(args.log, "w", encoding="utf-8") as handle:
            handle.write("")
    if args.state:
        state_dir = os.path.dirname(args.state)
        if state_dir:
            os.makedirs(state_dir, exist_ok=True)
        with open(args.state, "w", encoding="ascii") as handle:
            handle.write(f"pid={os.getpid()}\n")
            handle.write(f"started={time.time():.3f}\n")

    stages: list[dict[str, object]] = [
        {"name": "TCP setup/finish", "mode": "tcphandshake", "count": 6, "delay_ms": 120, "tcp_port": 443},
        {"name": "TCP ACK", "mode": "tcpack", "count": 6, "delay_ms": 100, "tcp_port": 443},
        {"name": "TCP payload", "mode": "tcppayload", "count": 6, "delay_ms": 100, "tcp_port": 443},
        {"name": "TCP reset", "mode": "tcpreset", "count": 5, "delay_ms": 120, "tcp_port": 443},
        {"name": "UDP payload", "mode": "udppayload", "count": 6, "delay_ms": 100, "udp_port": 5353},
        {"name": "ICMP echo", "mode": "icmpecho", "count": 5, "delay_ms": 80},
        {"name": "ICMP burst", "mode": "icmpburst", "count": 4, "delay_ms": 60, "burst_packets": 4},
        {"name": "ARP", "mode": "arp", "count": 6, "delay_ms": 100},
        {"name": "DNS discovery", "mode": "discovery", "count": 4, "delay_ms": 120, "discovery_kind": "dns", "discovery_name": "gateway.example.net"},
        {"name": "mDNS discovery", "mode": "discovery", "count": 5, "delay_ms": 120, "discovery_kind": "mdns", "discovery_name": "printer.office.local"},
        {"name": "LLMNR discovery", "mode": "discovery", "count": 4, "delay_ms": 120, "discovery_kind": "llmnr", "discovery_name": "workstation.local"},
        {"name": "NetBIOS discovery", "mode": "discovery", "count": 5, "delay_ms": 120, "discovery_kind": "netbios", "discovery_name": "NAS-BOX"},
        {"name": "Random mix", "mode": "random", "count": 8, "delay_ms": 80},
        {"name": "Wide host spread", "mode": "tcpack", "count": 10, "delay_ms": 60, "tcp_port": 80, "wide_host_spread": True},
    ]

    try:
        estimated_total = sum(stage_estimate_seconds(stage) for stage in stages)
        write_line("Synthetic Hosts3D quick demo (Python)", args.log)
        write_line(f"Destination: {args.host}:{args.port}  Sensor: {args.sensor}", args.log)
        if args.center_ip:
            write_line(f"Center host: {args.center_ip}", args.log)
        write_line(f"Estimated total runtime: {estimated_total:.1f}s", args.log)

        total_started = time.perf_counter()
        for stage in stages:
            run_stage(stage, args, sender_path, args.log)
        total_actual = time.perf_counter() - total_started

        write_line(f"Estimated total runtime: {estimated_total:.1f}s", args.log)
        write_line(f"Actual total runtime: {total_actual:.1f}s", args.log)
        write_line("Python demo finished.", args.log)
    finally:
        if args.state:
            try:
                os.remove(args.state)
            except FileNotFoundError:
                pass


if __name__ == "__main__":
    main()
