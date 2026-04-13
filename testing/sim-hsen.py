#!/usr/bin/env python3
"""
Modernized from the historical 2009 sim-hsen.py helper.

This is intentionally small and keeps the original idea:
send synthetic hsen-compatible UDP packet metadata directly to Hosts3D.
"""

import argparse
import random
import socket
import struct
import time


def ip_value(prefix: str, host: int) -> int:
    a, b, c = [int(part) for part in prefix.split(".")]
    return (host << 24) + (c << 16) + (b << 8) + a


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=10111)
    parser.add_argument("--count", type=int, default=0, help="0 means infinite")
    parser.add_argument("--delay", type=float, default=0.1, help="delay between packets in seconds")
    parser.add_argument("--sensor", type=int, default=1)
    parser.add_argument("--prefix", default="192.168.0")
    args = parser.parse_args()

    hadr = (args.host, args.port)
    hsock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    proto = [1, 6, 17, 249]  # ICMP, TCP, UDP, ARP

    print("Ctrl+C to stop" if args.count == 0 else f"Sending {args.count} burst(s)")
    sent = 0
    while args.count == 0 or sent < args.count:
        pr = random.randint(0, 3)
        srcip = ip_value(args.prefix, random.randint(1, 254))
        dstip = ip_value(args.prefix, random.randint(1, 254))
        srcpt = random.randint(0, 65535)
        dstpt = random.randint(0, 65535)
        pks = random.randint(1, 200)
        if pks > 50:
            pks = 1
        for _ in range(pks):
            pkex = struct.pack(
                "=ccIBBBBBBHHIIBB",
                b"U",
                b"\x00",
                48,
                0, 0, 0, 0, 0, 0,
                srcpt,
                dstpt,
                srcip,
                dstip,
                args.sensor,
                proto[pr],
            )
            hsock.sendto(pkex, hadr)
            pkex = struct.pack(
                "=ccIBBBBBBHHIIBB",
                b"U",
                b"\x00",
                48,
                0, 0, 0, 0, 0, 0,
                dstpt,
                srcpt,
                dstip,
                srcip,
                args.sensor,
                proto[pr],
            )
            hsock.sendto(pkex, hadr)
            time.sleep(args.delay)
        sent += 1


if __name__ == "__main__":
    main()
