#!/usr/bin/env python3
"""
Modernized from the historical upstream helper.

This sends synthetic hsen-compatible UDP packet metadata directly to Hosts3D.
It is intended for visualization and regression checks, not for generating
real raw network traffic.
"""

from __future__ import annotations

import argparse
import random
import socket
import struct
import time


PROTO_ICMP = 1
PROTO_TCP = 6
PROTO_UDP = 17
PROTO_ARP = 249


def parse_prefix(prefix: str) -> tuple[int, int, int]:
    parts = prefix.split(".")
    if len(parts) != 3:
      raise ValueError("prefix must look like '192.168.0'")
    values = tuple(int(part) for part in parts)
    for value in values:
        if value < 0 or value > 255:
            raise ValueError("prefix octets must be in the range 0-255")
    return values


def host_ip(prefix_parts: tuple[int, int, int], host_min: int, host_max: int, wide: bool,
            center_ip: str = "") -> str:
    while True:
        host_part = random.randint(host_min, host_max)
        if wide:
            candidate = f"{prefix_parts[0]}.{prefix_parts[1]}.{random.randint(0, 254)}.{host_part}"
        else:
            candidate = f"{prefix_parts[0]}.{prefix_parts[1]}.{prefix_parts[2]}.{host_part}"
        if not center_ip or candidate != center_ip:
            return candidate


def host_pair(prefix_parts: tuple[int, int, int], host_min: int, host_max: int, wide: bool,
              center_ip: str = "", center_role: str = "mixed") -> tuple[str, str]:
    if center_ip:
        peer_ip = host_ip(prefix_parts, host_min, host_max, wide, center_ip)
        if center_role == "source":
            return center_ip, peer_ip
        if center_role == "destination":
            return peer_ip, center_ip
        if random.randint(0, 1) == 0:
            return center_ip, peer_ip
        return peer_ip, center_ip
    src_ip = host_ip(prefix_parts, host_min, host_max, wide)
    dst_ip = host_ip(prefix_parts, host_min, host_max, wide)
    while dst_ip == src_ip:
        dst_ip = host_ip(prefix_parts, host_min, host_max, wide)
    return src_ip, dst_ip


def random_ephemeral_port() -> int:
    return random.randint(1024, 65535)


def discovery_port(kind: str) -> int:
    return {
        "dns": 53,
        "mdns": 5353,
        "llmnr": 5355,
        "netbios": 137,
    }[kind]


def discovery_responder_ip(prefix_parts: tuple[int, int, int], kind: str) -> str:
    last = {
        "dns": 53,
        "mdns": 251,
        "llmnr": 55,
        "netbios": 137,
    }[kind]
    return f"{prefix_parts[0]}.{prefix_parts[1]}.{prefix_parts[2]}.{last}"


def flags_byte(*, syn: bool = False, ack: bool = False, rst: bool = False, fin: bool = False,
               psh: bool = False, payload: bool = False, arp_subtype: int = 0) -> int:
    flags = 0
    if syn:
        flags |= 0x01
    if ack:
        flags |= 0x02
    if rst:
        flags |= 0x04
    if fin:
        flags |= 0x08
    if psh:
        flags |= 0x10
    if payload:
        flags |= 0x20
    flags |= (arp_subtype & 0x03) << 6
    return flags


def send_pkex(sock: socket.socket, dest: tuple[str, int], sensor: int, flags: int, size: int, mac: bytes,
              src_port: int, dst_port: int, src_ip: str, dst_ip: str, proto: int) -> None:
    packet = bytearray(26)
    packet[0] = 85
    packet[1] = flags & 0xFF
    struct.pack_into("=I", packet, 2, size)
    packet[6:12] = mac
    struct.pack_into("=H", packet, 12, src_port)
    struct.pack_into("=H", packet, 14, dst_port)
    packet[16:20] = socket.inet_aton(src_ip)
    packet[20:24] = socket.inet_aton(dst_ip)
    packet[24] = sensor & 0xFF
    packet[25] = proto & 0xFF
    sock.sendto(packet, dest)


def send_pdns(sock: socket.socket, dest: tuple[str, int], name: str, ip_text: str) -> None:
    packet = bytearray(261)
    packet[0] = 42
    name_bytes = name.encode("ascii", errors="ignore")[:255]
    packet[1:1 + len(name_bytes)] = name_bytes
    packet[257:261] = socket.inet_aton(ip_text)
    sock.sendto(packet, dest)


def random_mac() -> bytes:
    data = bytearray(random.getrandbits(8) for _ in range(6))
    data[0] = (data[0] & 0xFE) | 0x02
    return bytes(data)


def send_conversation(sock: socket.socket, dest: tuple[str, int], delay: float, sensor: int, src_ip: str, dst_ip: str,
                      src_port: int, dst_port: int, flags: int, size: int, proto: int, reply: bool = False) -> None:
    send_pkex(sock, dest, sensor, flags, size, random_mac(), src_port, dst_port, src_ip, dst_ip, proto)
    time.sleep(delay)
    if reply:
        reply_flags = flags
        if proto == PROTO_TCP and (flags & 0x01) and not (flags & 0x02):
            reply_flags = flags_byte(syn=True, ack=True)
        send_pkex(sock, dest, sensor, reply_flags, size, random_mac(), dst_port, src_port, dst_ip, src_ip, proto)
        time.sleep(delay)


def invoke_random(sock: socket.socket, dest: tuple[str, int], args: argparse.Namespace, prefix_parts: tuple[int, int, int]) -> None:
    choice = random.randint(0, 7)
    if choice == 0:
        invoke_tcp_handshake(sock, dest, args, prefix_parts)
    elif choice == 1:
        invoke_tcp_ack(sock, dest, args, prefix_parts)
    elif choice == 2:
        invoke_tcp_payload(sock, dest, args, prefix_parts)
    elif choice == 3:
        invoke_tcp_reset(sock, dest, args, prefix_parts)
    elif choice == 4:
        invoke_udp_payload(sock, dest, args, prefix_parts)
    elif choice == 5:
        invoke_icmp_echo(sock, dest, args, prefix_parts)
    elif choice == 6:
        invoke_arp(sock, dest, args, prefix_parts)
    else:
        invoke_discovery(sock, dest, args, prefix_parts)


def invoke_tcp_handshake(sock: socket.socket, dest: tuple[str, int], args: argparse.Namespace, prefix_parts: tuple[int, int, int]) -> None:
    src_ip, dst_ip = host_pair(prefix_parts, args.host_min, args.host_max, args.wide_host_spread, args.center_ip, args.center_role)
    src_port = random_ephemeral_port()
    send_pkex(sock, dest, args.sensor, flags_byte(syn=True), 60, random_mac(), src_port, args.tcp_port, src_ip, dst_ip, PROTO_TCP)
    time.sleep(args.delay)
    send_pkex(sock, dest, args.sensor, flags_byte(syn=True, ack=True), 60, random_mac(), args.tcp_port, src_port, dst_ip, src_ip, PROTO_TCP)
    time.sleep(args.delay)
    send_pkex(sock, dest, args.sensor, flags_byte(ack=True), 60, random_mac(), src_port, args.tcp_port, src_ip, dst_ip, PROTO_TCP)
    time.sleep(args.delay)


def invoke_tcp_ack(sock: socket.socket, dest: tuple[str, int], args: argparse.Namespace, prefix_parts: tuple[int, int, int]) -> None:
    src_ip, dst_ip = host_pair(prefix_parts, args.host_min, args.host_max, args.wide_host_spread, args.center_ip, args.center_role)
    send_conversation(sock, dest, args.delay, args.sensor, src_ip, dst_ip, random_ephemeral_port(), args.tcp_port,
                      flags_byte(ack=True), 60, PROTO_TCP, reply=True)


def invoke_tcp_payload(sock: socket.socket, dest: tuple[str, int], args: argparse.Namespace, prefix_parts: tuple[int, int, int]) -> None:
    src_ip, dst_ip = host_pair(prefix_parts, args.host_min, args.host_max, args.wide_host_spread, args.center_ip, args.center_role)
    src_port = random_ephemeral_port()
    send_pkex(sock, dest, args.sensor, flags_byte(ack=True, psh=True, payload=True), random.randint(200, 900),
              random_mac(), src_port, args.tcp_port, src_ip, dst_ip, PROTO_TCP)
    time.sleep(args.delay)
    send_pkex(sock, dest, args.sensor, flags_byte(ack=True), 60, random_mac(), args.tcp_port, src_port, dst_ip, src_ip, PROTO_TCP)
    time.sleep(args.delay)


def invoke_tcp_reset(sock: socket.socket, dest: tuple[str, int], args: argparse.Namespace, prefix_parts: tuple[int, int, int]) -> None:
    src_ip, dst_ip = host_pair(prefix_parts, args.host_min, args.host_max, args.wide_host_spread, args.center_ip, args.center_role)
    src_port = random_ephemeral_port()
    send_pkex(sock, dest, args.sensor, flags_byte(syn=True), 60, random_mac(), src_port, args.tcp_port, src_ip, dst_ip, PROTO_TCP)
    time.sleep(args.delay)
    send_pkex(sock, dest, args.sensor, flags_byte(syn=True, ack=True), 60, random_mac(), args.tcp_port, src_port, dst_ip, src_ip, PROTO_TCP)
    time.sleep(args.delay)
    send_pkex(sock, dest, args.sensor, flags_byte(rst=True, ack=True), 60, random_mac(), src_port, args.tcp_port, src_ip, dst_ip, PROTO_TCP)
    time.sleep(args.delay)


def invoke_udp_payload(sock: socket.socket, dest: tuple[str, int], args: argparse.Namespace, prefix_parts: tuple[int, int, int]) -> None:
    src_ip, dst_ip = host_pair(prefix_parts, args.host_min, args.host_max, args.wide_host_spread, args.center_ip, args.center_role)
    send_conversation(sock, dest, args.delay, args.sensor, src_ip, dst_ip, random_ephemeral_port(), args.udp_port,
                      flags_byte(payload=True), random.randint(80, 700), PROTO_UDP, reply=True)


def invoke_icmp_echo(sock: socket.socket, dest: tuple[str, int], args: argparse.Namespace, prefix_parts: tuple[int, int, int]) -> None:
    src_ip, dst_ip = host_pair(prefix_parts, args.host_min, args.host_max, args.wide_host_spread, args.center_ip, args.center_role)
    send_conversation(sock, dest, args.delay, args.sensor, src_ip, dst_ip, 0, 0, flags_byte(), 84, PROTO_ICMP, reply=True)


def invoke_icmp_burst(sock: socket.socket, dest: tuple[str, int], args: argparse.Namespace, prefix_parts: tuple[int, int, int]) -> None:
    src_ip, dst_ip = host_pair(prefix_parts, args.host_min, args.host_max, args.wide_host_spread, args.center_ip, args.center_role)
    for _ in range(args.burst_packets):
        send_conversation(sock, dest, args.delay, args.sensor, src_ip, dst_ip, 0, 0, flags_byte(), 84, PROTO_ICMP, reply=True)


def invoke_arp(sock: socket.socket, dest: tuple[str, int], args: argparse.Namespace, prefix_parts: tuple[int, int, int]) -> None:
    src_ip, dst_ip = host_pair(prefix_parts, args.host_min, args.host_max, args.wide_host_spread, args.center_ip, args.center_role)
    send_pkex(sock, dest, args.sensor, flags_byte(arp_subtype=random.choice((1, 2, 3))), 60, random_mac(), 0, 0, src_ip, dst_ip, PROTO_ARP)
    time.sleep(args.delay)


def invoke_discovery(sock: socket.socket, dest: tuple[str, int], args: argparse.Namespace, prefix_parts: tuple[int, int, int]) -> None:
    host_text = args.center_ip or host_ip(prefix_parts, args.host_min, args.host_max, args.wide_host_spread, args.center_ip)
    resolver_text = discovery_responder_ip(prefix_parts, args.discovery_kind)
    if resolver_text == host_text:
        host_text, resolver_text = host_pair(prefix_parts, args.host_min, args.host_max, args.wide_host_spread, args.center_ip, args.center_role)
    resolver_port = discovery_port(args.discovery_kind)
    client_port = random_ephemeral_port()
    send_pkex(sock, dest, args.sensor, flags_byte(payload=True), random.randint(140, 260), random_mac(),
              client_port, resolver_port, host_text, resolver_text, PROTO_UDP)
    time.sleep(args.delay)
    send_pkex(sock, dest, args.sensor, flags_byte(payload=True), random.randint(160, 280), random_mac(),
              resolver_port, client_port, resolver_text, host_text, PROTO_UDP)
    time.sleep(args.delay)
    send_pdns(sock, dest, args.discovery_name, host_text)
    time.sleep(args.delay)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=10111)
    parser.add_argument("--count", type=int, default=0, help="0 means infinite")
    parser.add_argument("--delay", type=float, default=0.1, help="delay between packets in seconds")
    parser.add_argument("--sensor", type=int, default=1)
    parser.add_argument("--prefix", default="192.168.0")
    parser.add_argument("--host-min", type=int, default=1)
    parser.add_argument("--host-max", type=int, default=64)
    parser.add_argument("--tcp-port", type=int, default=443)
    parser.add_argument("--udp-port", type=int, default=5353)
    parser.add_argument("--burst-packets", type=int, default=6)
    parser.add_argument("--discovery-name", default="demo-host.local")
    parser.add_argument("--discovery-kind", choices=("dns", "mdns", "llmnr", "netbios"), default="mdns")
    parser.add_argument("--wide-host-spread", action="store_true")
    parser.add_argument("--center-ip", default="")
    parser.add_argument("--center-role", choices=("mixed", "source", "destination"), default="mixed")
    parser.add_argument(
        "--mode",
        choices=("random", "tcphandshake", "tcpack", "tcppayload", "tcpreset", "udppayload", "icmpecho", "icmpburst", "arp", "discovery"),
        default="random",
    )
    args = parser.parse_args()

    if args.host_min > args.host_max:
        raise SystemExit("--host-min must be less than or equal to --host-max")
    if args.center_ip:
        socket.inet_aton(args.center_ip)
        prefix_parts = parse_prefix(".".join(args.center_ip.split(".")[:3]))
    else:
        prefix_parts = parse_prefix(args.prefix)
    dest = (args.host, args.port)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    print("Synthetic hsen sender")
    print(f"Mode: {args.mode}  Destination: {args.host}:{args.port}  Sensor: {args.sensor}")
    if args.center_ip:
        print(f"Center host: {args.center_ip}  Role: {args.center_role}")
    if args.mode == "discovery":
        print(f"Discovery kind: {args.discovery_kind}  Discovery name: {args.discovery_name}")
    if args.wide_host_spread:
        print("Host spread: wide")
    print("Count: infinite  Ctrl+C to stop" if args.count == 0 else f"Count: {args.count}")

    sent = 0
    while args.count == 0 or sent < args.count:
        if args.mode == "random":
            invoke_random(sock, dest, args, prefix_parts)
        elif args.mode == "tcphandshake":
            invoke_tcp_handshake(sock, dest, args, prefix_parts)
        elif args.mode == "tcpack":
            invoke_tcp_ack(sock, dest, args, prefix_parts)
        elif args.mode == "tcppayload":
            invoke_tcp_payload(sock, dest, args, prefix_parts)
        elif args.mode == "tcpreset":
            invoke_tcp_reset(sock, dest, args, prefix_parts)
        elif args.mode == "udppayload":
            invoke_udp_payload(sock, dest, args, prefix_parts)
        elif args.mode == "icmpecho":
            invoke_icmp_echo(sock, dest, args, prefix_parts)
        elif args.mode == "icmpburst":
            invoke_icmp_burst(sock, dest, args, prefix_parts)
        elif args.mode == "arp":
            invoke_arp(sock, dest, args, prefix_parts)
        elif args.mode == "discovery":
            invoke_discovery(sock, dest, args, prefix_parts)
        sent += 1

    sock.close()
    print(f"Finished after {sent} burst(s).")


if __name__ == "__main__":
    main()
