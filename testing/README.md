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

ARP request/reply/gratuitous demo:

```powershell
pwsh -File .\testing\sim-hsen.ps1 -Mode Arp -Count 30
```

Discovery/name demo:

```powershell
pwsh -File .\testing\sim-hsen.ps1 -Mode Discovery -DiscoveryName printer.office.local -Count 20
```

## Notes

- `Count` means synthetic conversations/bursts, not individual packets.
- The default destination is `127.0.0.1:10111`.
- These scripts are useful for visualization checks, menu/OSD checks, and replay-free packet demos.

## Python Variant

`sim-hsen.py` is a small Python 3 version derived from the original historical script.

On normal Windows test systems, `sim-hsen.ps1` is the preferred tool because it does not require Python.
