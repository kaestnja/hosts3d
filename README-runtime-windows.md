# Hosts3D 1.18 for Windows

This package contains the Windows runtime files for `Hosts3D` and `hsen`.

## Included Files
- `Hosts3D.exe`
- `hsen.exe`
- `glfw3.dll`
- `libwinpthread-1.dll`
- `COPYING`

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

## Notes
- `settings.ini` is plain text and can be edited with Hosts3D closed.
- `controls.txt` is generated from the active keybindings.
- `0network.hnl` is the auto-saved layout from the last session.
- Newly written `.hnl` files now use the versioned `HN2` layout format.
- If an older incompatible `0network.hnl` is found after replacing the EXE, Hosts3D now skips it with a warning and writes a new compatible layout on exit.
- `traffic.hpt` is Hosts3D's own record/replay format, not a Wireshark capture file.
- `Stop` and normal `Exit` re-scan for matching bundled `hsen.exe` processes from the same installation path, so stale local sensor processes can be cleaned up even if the saved PID state was lost or outdated.
- Public release ZIPs may omit `wpcap.dll` and `Packet.dll`. In that case, install Npcap on the target machine before using `hsen` or `Configure Local Sensors (hsen)`.

## License
Hosts3D is distributed under the GNU General Public License. See `COPYING`.
This package is provided without warranty. Current project page: <https://github.com/kaestnja/hosts3d>.
