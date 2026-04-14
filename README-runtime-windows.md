# Hosts3D 1.18 for Windows

This package contains the Windows runtime files for `Hosts3D` and `hsen`.

## Included Files
- `README.md`
- `README-runtime-windows.md`
- `README-testing.md`
- `Hosts3D.exe`
- `hsen.exe`
- `glfw3.dll`
- `libwinpthread-1.dll`
- `COPYING`
- `sim-hsen.ps1`
- `sim-hsen.py`
- `demo-hsen.ps1`
- `demo-hsen.py`

Private/local test packages may additionally include:
- `Packet.dll`
- `wpcap.dll`

## First Start
1. Start `Hosts3D.exe`
2. Right-click in the 3D view and choose `Configure Local Sensors (hsen)`
3. Select one or more capture interfaces
4. Click `Save`, then `Start`

`Hosts3D` creates its runtime folder and files on first start as needed:
- `hsd-data\settings.ini`
- `hsd-data\controls.txt`
- `hsd-data\netpos.txt`
- `hsd-data\0network.hnl`

You can also trigger the bundled synthetic visualization demos from the top-right OSD:
- `PS Demo`
- `Py Demo`

While a demo is active, the matching OSD button stays tinted until the demo finishes.
The same `RUNTIME` strip also includes a clickable `Dynamic Host TTL` row with presets from `Off` up to `1h`.

This package keeps the quick demo scripts beside `Hosts3D.exe`. The runtime still also accepts the older `testing\` layout and the usual development layout as fallbacks. The latest timing summary is written to:
- `hsd-data\demo-powershell-last.txt`
- `hsd-data\demo-python-last.txt`

Expected demo artifact lifetime:
- packet and alert objects usually disappear again within seconds
- dynamic demo hosts age out after normal inactivity cleanup, controlled by `dynamic_host_ttl_seconds`
- the default `dynamic_host_ttl_seconds` is `300` seconds (`5` minutes)
- `dynamic_host_ttl_seconds=0` means `Off`, so dynamic hosts are no longer removed automatically

## Notes
- `settings.ini` is plain text and can be edited with Hosts3D closed.
- `controls.txt` is generated from the active keybindings.
- `0network.hnl` is the auto-saved layout from the last session.
- Newly written `.hnl` files now use the versioned `HN2` layout format.
- If an older incompatible `0network.hnl` is found after replacing the EXE, Hosts3D now skips it with a warning and writes a new compatible layout on exit.
- `traffic.hpt` is Hosts3D's own record/replay format, not a Wireshark capture file.
- `Stop` and normal `Exit` re-scan for matching bundled `hsen.exe` processes from the same installation path, so stale local sensor processes can be cleaned up even if the saved PID state was lost or outdated.
- The quick demo scripts stay below one minute and currently run for roughly 23-25 seconds on a normal machine.
- `README-testing.md` documents the bundled synthetic sender/demo scripts and their example invocations.
- When a local sensor interface is selected, the demo prefers the first selected local adapter IPv4 as its central demo host and uses that interface's sensor ID.
- The PowerShell demo works with bundled Windows PowerShell or PowerShell 7.
- The Python demo requires Python 3 or the `py` launcher to be installed.
- Public release ZIPs may omit `wpcap.dll` and `Packet.dll`. In that case, install Npcap on the target machine before using `hsen` or `Configure Local Sensors (hsen)`.

## License
Hosts3D is distributed under the GNU General Public License. See `COPYING`.
This package is provided without warranty. Current project page: <https://github.com/kaestnja/hosts3d>.
