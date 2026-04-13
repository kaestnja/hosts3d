# Synthetic Hosts3D Traffic Tools

This folder contains lightweight test senders for `Hosts3D`.

They do not capture real traffic.
Instead, they send synthetic `hsen`-compatible UDP packets directly to `Hosts3D` on port `10111`.

The commands below assume your shell is already in the folder that contains this README and the four demo/test scripts.
In the repository that folder is `testing/`; in flat release packages it is the runtime folder itself.

## Recommended Tool on Windows

Use:

```powershell
powershell -ExecutionPolicy Bypass -File .\sim-hsen.ps1
```

or with PowerShell 7:

```powershell
pwsh -File .\sim-hsen.ps1
```

With no extra arguments, this starts the synthetic sender in `Random` mode and keeps running until you stop it with `Ctrl+C`.
Use that as the quickest broad smoke test when you just want visible synthetic activity without tuning a specific packet family first.

## Useful Examples

The examples below cover every currently supported sender mode once.
`Discovery` is shown once for each supported `DiscoveryKind`, because `Dns`, `mDns`, `Llmnr`, and `NetBIOS` intentionally generate different resolver and name-publication patterns.
The remaining parameters such as `Count`, `DelayMs`, `BurstPackets`, `CenterIp`, `CenterRole`, `SensorId`, and `WideHostSpread` tune pacing or topology rather than introducing separate traffic families.

Random mixed traffic:

```powershell
pwsh -File .\sim-hsen.ps1 -Mode Random -Count 100
```

Generates: a rotating mix of TCP handshake, ACK-only TCP, TCP payload, TCP reset, UDP payload, ICMP echo, ARP, and discovery traffic.
Look for: a quick overall "is the scene alive?" check with varied packet shapes and several host-to-host relationships on screen at once.

TCP handshake demo:

```powershell
pwsh -File .\sim-hsen.ps1 -Mode TcpHandshake -Count 20
```

Generates: clean `SYN -> SYN/ACK -> ACK` sequences without payload.
Look for: short-lived connection setup visuals and whether pure TCP control packets remain easy to distinguish from data traffic.

TCP control/payload demo:

```powershell
pwsh -File .\sim-hsen.ps1 -Mode TcpPayload -Count 40
```

Generates: application-like TCP payload packets followed by ACK replies.
Look for: directional request/response flow and larger payload packet rendering instead of handshake-only activity.

TCP ACK-only control traffic:

```powershell
pwsh -File .\sim-hsen.ps1 -Mode TcpAck -Count 20
```

Generates: tiny ACK reply pairs with almost no payload.
Look for: sparse low-noise control traffic and whether small TCP packets still animate clearly.

TCP reset demo:

```powershell
pwsh -File .\sim-hsen.ps1 -Mode TcpReset -Count 20
```

Generates: failed TCP setup that ends in a reset instead of a completed session.
Look for: aborted-session traffic and a visibly different TCP control pattern from the normal handshake case.

UDP payload demo:

```powershell
pwsh -File .\sim-hsen.ps1 -Mode UdpPayload -UdpPort 5353 -Count 30
```

Generates: connectionless UDP request/reply exchanges on the chosen UDP port.
Look for: service-style chatter without TCP state, useful for protocol-shape checks that should feel lighter than TCP.

ICMP echo demo:

```powershell
pwsh -File .\sim-hsen.ps1 -Mode IcmpEcho -Count 12
```

Generates: single ping-style echo request/reply pairs.
Look for: clean two-host reachability activity with no TCP or UDP ports involved.

ICMP echo burst demo:

```powershell
pwsh -File .\sim-hsen.ps1 -Mode IcmpBurst -BurstPackets 8 -Count 10
```

Generates: several ping-style exchanges back-to-back for the same host pair per burst.
Look for: denser repeated activity and packet trails that cluster around one pair instead of spreading out immediately.

ARP request/reply/gratuitous demo:

```powershell
pwsh -File .\sim-hsen.ps1 -Mode Arp -Count 30
```

Generates: randomized ARP request, reply, and gratuitous variants.
Look for: LAN-local neighbor/discovery chatter and confirmation that non-TCP/UDP packet families still render sensibly.

DNS discovery/name demo:

```powershell
pwsh -File .\sim-hsen.ps1 -Mode Discovery -DiscoveryKind Dns -DiscoveryName gateway.example.net -Count 12
```

Generates: DNS-style resolver traffic toward a `.53` infrastructure host plus a synthetic name publication for `gateway.example.net`.
Look for: classic resolver-centric discovery behavior with one visible infrastructure endpoint.

mDNS discovery/name demo:

```powershell
pwsh -File .\sim-hsen.ps1 -Mode Discovery -DiscoveryKind mDns -DiscoveryName printer.office.local -Count 20
```

Generates: multicast-DNS-style discovery traffic and a synthetic local service announcement for `printer.office.local`.
Look for: zero-config discovery scenes where one local service name becomes visually associated with a nearby host.

LLMNR discovery/name demo:

```powershell
pwsh -File .\sim-hsen.ps1 -Mode Discovery -DiscoveryKind Llmnr -DiscoveryName workstation.local -Count 12
```

Generates: LLMNR-style local name-resolution exchanges and a matching host-name publication.
Look for: Windows-style flat-LAN resolver traffic that should feel more local than classic DNS.

NetBIOS discovery/name demo:

```powershell
pwsh -File .\sim-hsen.ps1 -Mode Discovery -DiscoveryKind NetBIOS -DiscoveryName NAS-BOX -Count 12
```

Generates: NetBIOS-style legacy name traffic and a short synthetic host announcement for `NAS-BOX`.
Look for: older SMB/LAN discovery visuals and legacy local-network naming behavior.

Wide host spread across many more addresses:

```powershell
pwsh -File .\sim-hsen.ps1 -Mode Random -WideHostSpread -Count 100
```

Generates: the chosen traffic family with the third and fourth octets spread across many more addresses instead of staying tightly clustered.
Look for: a scene that fills with many distinct hosts faster, useful when you want to stress-test spacing and clutter handling.

## Short Demo Wrappers

For a short deterministic visualization showcase with per-stage runtime estimates and measured totals:

```powershell
pwsh -File .\demo-hsen.ps1
```

Generates: a fixed multi-stage walkthrough that steps through the focused traffic families in a predictable order.
Look for: repeatable before/after comparison runs where you want roughly the same visual sequence every time.

```powershell
python .\demo-hsen.py
```

Generates: the same deterministic quick demo as the PowerShell wrapper, but via Python 3.
Look for: the same visual coverage when Python is the easier runtime to use on that machine.

Both demo wrappers currently stay well below one minute and usually finish in about 23-25 seconds.

If you want one visible central host to stay anchored to a specific adapter or test IP:

```powershell
pwsh -File .\demo-hsen.ps1 -CenterIp 192.168.172.42
```

Generates: the normal staged demo, but keeps one fixed visible host in the scene while peers rotate around it.
Look for: recordings or regression checks where one adapter-like host should remain visually recognizable across several test phases.

```powershell
python .\demo-hsen.py --center-ip 192.168.172.42
```

Generates: the same anchored-host demo layout through the Python wrapper.
Look for: a stable central host when you want to compare host placement or host aging across runs.

To keep a timing log:

```powershell
pwsh -File .\demo-hsen.ps1 -LogPath .\demo-ps-last.txt
```

Generates: the usual staged demo plus a plain-text summary file with estimated and measured runtime per stage.
Look for: quick regression notes when you want to compare timing drift or confirm which stages actually ran.

```powershell
python .\demo-hsen.py --log .\demo-py-last.txt
```

Generates: the same per-stage timing log from the Python wrapper.
Look for: an easy artifact to attach to debugging notes without reading the console scrollback.

## Expected Artifact Lifetime

- flying packet objects and activity alerts usually disappear again within seconds through the normal animation lifecycle
- dynamic hosts created by the demo remain until the normal inactivity cleanup removes them
- that timeout is controlled by `dynamic_host_ttl_seconds` and currently defaults to `300` seconds (`5` minutes)
- selected, locked, or freshly refreshed hosts can remain visible longer, just like during normal live capture

## Notes

- `Count` means synthetic conversations/bursts, not individual packets.
- The default destination is `127.0.0.1:10111`.
- All currently supported sender modes are documented above: `Random`, `TcpHandshake`, `TcpAck`, `TcpPayload`, `TcpReset`, `UdpPayload`, `IcmpEcho`, `IcmpBurst`, `Arp`, and `Discovery`.
- `Discovery` is additionally shown once for each supported `DiscoveryKind`: `Dns`, `mDns`, `Llmnr`, and `NetBIOS`.
- Both senders also support a central demo host via `CenterIp` / `--center-ip`, with mixed/source/destination behavior.
- The short demo wrappers run a deterministic sequence of these modes and report estimated and actual runtime per stage.
- `-WideHostSpread` helps create many more distinct hosts without changing the general prefix family.
- These scripts are useful for visualization checks, menu/OSD checks, packet-shape verification, and replay-free packet demos.

## Python Variant

`sim-hsen.py` is a Python 3 variant of the same synthetic sender and supports the same focused test modes plus the same central-host options.
For brevity, the isolated sender examples above are shown in PowerShell form, but every one of them has a direct Python equivalent with lowercase `--mode` / `--discovery-kind` values and long option names such as `--center-ip` and `--wide-host-spread`.

Example translation:

```powershell
python .\sim-hsen.py --mode tcphandshake --count 20
```

Generates: the same handshake-only test as the PowerShell `TcpHandshake` example.
Look for: identical connection-setup visuals, just launched through Python instead of PowerShell.

On normal Windows test systems, `sim-hsen.ps1` is the preferred tool because it does not require Python.
