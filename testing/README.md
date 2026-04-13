# Synthetic Hosts3D Traffic Tools

This folder contains lightweight test senders for `Hosts3D`.

They do not capture real traffic.
Instead, they send synthetic `hsen`-compatible UDP packets directly to `Hosts3D` on port `10111`.

## Recommended Tool on Windows

Use:

```powershell
powershell -ExecutionPolicy Bypass -File .\testing\sim-hsen.ps1
```

or with PowerShell 7:

```powershell
pwsh -File .\testing\sim-hsen.ps1
```

## Useful Examples

Random mixed traffic:

```powershell
pwsh -File .\testing\sim-hsen.ps1 -Mode Random -Count 100
```

TCP control/payload demo:

```powershell
pwsh -File .\testing\sim-hsen.ps1 -Mode TcpPayload -Count 40
```

TCP ACK-only control traffic:

```powershell
pwsh -File .\testing\sim-hsen.ps1 -Mode TcpAck -Count 20
```

TCP reset demo:

```powershell
pwsh -File .\testing\sim-hsen.ps1 -Mode TcpReset -Count 20
```

ICMP echo burst demo:

```powershell
pwsh -File .\testing\sim-hsen.ps1 -Mode IcmpBurst -BurstPackets 8 -Count 10
```

ARP request/reply/gratuitous demo:

```powershell
pwsh -File .\testing\sim-hsen.ps1 -Mode Arp -Count 30
```

Discovery/name demo:

```powershell
pwsh -File .\testing\sim-hsen.ps1 -Mode Discovery -DiscoveryKind mDns -DiscoveryName printer.office.local -Count 20
```

Wide host spread across many more addresses:

```powershell
pwsh -File .\testing\sim-hsen.ps1 -Mode Random -WideHostSpread -Count 100
```

## Short Demo Wrappers

For a short deterministic visualization showcase with per-stage runtime estimates and measured totals:

```powershell
pwsh -File .\testing\demo-hsen.ps1
```

```powershell
python .\testing\demo-hsen.py
```

Both demo wrappers currently stay well below one minute and usually finish in about 23-25 seconds.

If you want one visible central host to stay anchored to a specific adapter or test IP:

```powershell
pwsh -File .\testing\demo-hsen.ps1 -CenterIp 192.168.172.42
```

```powershell
python .\testing\demo-hsen.py --center-ip 192.168.172.42
```

To keep a timing log:

```powershell
pwsh -File .\testing\demo-hsen.ps1 -LogPath .\testing\demo-ps-last.txt
```

```powershell
python .\testing\demo-hsen.py --log .\testing\demo-py-last.txt
```

## Expected Artifact Lifetime

- flying packet objects and activity alerts usually disappear again within seconds through the normal animation lifecycle
- dynamic hosts created by the demo remain until the normal inactivity cleanup removes them
- that timeout is controlled by `dynamic_host_ttl_seconds` and currently defaults to `300` seconds (`5` minutes)
- selected, locked, or freshly refreshed hosts can remain visible longer, just like during normal live capture

## Notes

- `Count` means synthetic conversations/bursts, not individual packets.
- The default destination is `127.0.0.1:10111`.
- The PowerShell sender now includes focused visualization modes for `TcpAck`, `TcpReset`, `IcmpEcho`, `IcmpBurst`, `Arp`, and several discovery styles (`Dns`, `mDns`, `Llmnr`, `NetBIOS`).
- Both senders also support a central demo host via `CenterIp` / `--center-ip`, with mixed/source/destination behavior.
- The short demo wrappers run a deterministic sequence of these modes and report estimated and actual runtime per stage.
- `-WideHostSpread` helps create many more distinct hosts without changing the general prefix family.
- These scripts are useful for visualization checks, menu/OSD checks, packet-shape verification, and replay-free packet demos.

## Python Variant

`sim-hsen.py` is a Python 3 variant of the same synthetic sender and supports the same focused test modes plus the same central-host options.

On normal Windows test systems, `sim-hsen.ps1` is the preferred tool because it does not require Python.
