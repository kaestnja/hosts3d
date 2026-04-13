# Hosts3D 1.18

> 3D Real-Time Network Monitor (legacy project, modernized build workflow)  
> Original upstream release: 10 May 2011, Del Castle

## Overview
`Hosts3D` visualizes hosts and packet traffic in a 3D scene.  
`hsen` (Hosts3D Sensor) captures packet headers and sends compact packet metadata via UDP to `Hosts3D` (local or remote).

Core capabilities:
- Multiple sensors (`hsen`) feeding one or more `Hosts3D` instances
- Hostname/service discovery from observed traffic
- Configurable network layout and host grouping
- Packet recording/replay (`.hpt`)
- Filtering by host, protocol, and port

This README is organized for practical use:
1. get the build working
2. start `Hosts3D`
3. connect one or more local sensors
4. adjust settings and controls
5. use the later sections as reference

## Quick Start
If your main goal is to get the project running again, use this order:

1. Build `Hosts3D` and `hsen`
2. Start `Hosts3D`
3. Right-click in the 3D view, then choose `Configure Local Sensors (hsen)`
4. Select one or more capture interfaces
5. Click `Save`, then `Start`

Recommended paths:
- Windows 11:
  - install the MinGW/MSYS2 toolchain once
  - run `.\compile-hosts3d.bat Release x86`
  - run `.\compile-hsen.bat Release x86`
  - start `.\start-Hosts3DW.bat`
  - in Hosts3D, right-click in the 3D view, then choose `Configure Local Sensors (hsen)`
- Linux/macOS:
  - build with the platform commands below
  - start `Hosts3D`, right-click in the 3D view, then choose `Configure Local Sensors (hsen)`
  - click `Save` once in `Configure Local Sensors (hsen)` so Hosts3D can try the one-time local capture setup automatically
  - if automatic setup fails, use the exact command shown by Hosts3D once, then press `Start`

## Requirements
Use the platform-specific subsection that matches the machine you are building on.

### Hardware
- Scroll mouse
- OpenGL-capable graphics card with installed drivers

### Software By Platform
| Platform | Runtime | Build Toolchain |
|---|---|---|
| Linux | `glfw3`, `libpcap` | `g++`, `libglfw3-dev`, `libpcap-dev`, `pkg-config` |
| macOS | GLFW 3 | GCC/Clang + GLFW 3 + `pkg-config` |
| FreeBSD | `hsen` supported | standard C/C++ toolchain |
| Windows | Npcap/WinPcap-compatible runtime | MinGW (MSYS2) + GLFW 3 + Wpcap/Npcap SDK files |

## Build

### Linux (legacy upstream flow)
```bash
./configure
make
sudo make install
```

Uninstall:
```bash
sudo make uninstall
```

Alternative manual build commands:
```bash
./compile-hsen
./compile-hosts3d
```

### macOS (helper script present, but not recently verified)
The macOS helper script is still kept in the repository, but the current `1.18`
sources have not been recently verified on a real macOS machine.

Use this path as a best-effort starting point, not as a guaranteed tested build.

```bash
./compile-hsen
./compile-mac-hosts3d
```

The script itself now prints a warning and pauses for confirmation before it proceeds.

### FreeBSD (`hsen`)
```bash
./compile-hsen
```

### Windows 11 Build (MSYS2 + MinGW32)
> If `g++` is missing (`g++ is not recognized`), install toolchain first.
> Install MSYS2 first from the official project (`https://www.msys2.org/`) or via `winget`, then use the package commands below.

```powershell
winget install -e --id MSYS2.MSYS2
C:\msys64\usr\bin\bash -lc "pacman -Syu --noconfirm"
C:\msys64\usr\bin\bash -lc "pacman -S --needed --noconfirm mingw-w64-i686-gcc mingw-w64-i686-binutils mingw-w64-i686-glfw make"
```

```powershell
$env:Path = "C:\msys64\mingw32\bin;$env:Path"
g++ --version
g++ -dumpmachine
```

Expected target:
```text
i686-w64-mingw32
```

Build:
```powershell
$repo = Join-Path $env:USERPROFILE "source\\repos\\github.com\\<your-github-user>\\hosts3d"
Set-Location $repo
.\compile-hosts3d.bat Release x86
.\compile-hsen.bat Release x86
```

For a 64-bit build, request `x64` explicitly:
```powershell
.\compile-hosts3d.bat Release x64
.\compile-hsen.bat Release x64
```

Windows x64 also requires:
- an installed x64 MinGW toolchain (`mingw64` or `ucrt64`)
- the matching GLFW 3 package, for example:

```powershell
C:\msys64\usr\bin\bash -lc "pacman -S --needed --noconfirm mingw-w64-x86_64-gcc mingw-w64-x86_64-binutils mingw-w64-x86_64-glfw make"
```

The Windows `Hosts3D` build script now uses the GLFW 3 package installed under the selected MSYS2 MinGW toolchain root and copies `glfw3.dll` into the runtime output.

Verified current compiler path:
- `C:\msys64\mingw64\bin\g++.exe`

## Build Output Layout
Build scripts place binaries into config/os/arch folders:

| Binary | Release path | Debug path |
|---|---|---|
| Hosts3D | `Release/windows/x86/Hosts3D.exe` | `Debug/windows/x86/Hosts3D.exe` |
| hsen | `Release/windows/x86/hsen.exe` | `Debug/windows/x86/hsen.exe` |

With explicit arch selection, the same scripts also produce:
- `Release/windows/x64/Hosts3D.exe`
- `Release/windows/x64/hsen.exe`
- `Release/linux/arm64/hosts3d`
- `Release/linux/arm64/hsen`

Notes:
- Scripts overwrite known targets but do not wipe the whole `Release`/`Debug` tree.
- The Windows scripts now accept an explicit arch such as `x86` or `x64`.
- Windows builds now place intermediate object files under `build/windows/<target>/<arch>/<config>/`, so `x86`/`x64` and `Hosts3D`/`hsen` builds can run in parallel without mixing object files.
- Local dependency layout for build scripts: see `third_party/README.md`.
- Portable Wireshark helper location: `Tools/Wireshark/`.
- Runtime binaries under `Release/` and `Debug/` are local build outputs and are not intended to stay tracked in Git.
- Distributable binaries should be shipped as release packages/assets instead.

### Windows Packaging
Create a public release ZIP without bundled Npcap DLLs:
```powershell
.\package-release-windows.bat Release x64
```

Create a private/local test ZIP that also carries `wpcap.dll` and `Packet.dll`:
```powershell
.\package-release-windows.bat Release x64 with-npcap
```

### Linux Packaging
On Linux or Raspberry Pi OS, create a distributable tarball from the already-built runtime:
```bash
./package-release-linux Release arm64
```

This creates:
- `Release/dist/hosts3d-<version>-linux-arm64.tar.gz`
- `Release/dist/hosts3d-<version>-linux-arm64-SHA256.txt`

The Linux package includes:
- `hosts3d`
- `hsen`
- `COPYING`
- `README-runtime-linux.md`

## Manual Start Helpers (Windows, Optional)
```powershell
.\start-Hosts3DW.bat
.\start-Hosts3DW.bat fullscreen
.\start-hsenW.bat
```

Use these helpers when you want the old script-driven workflow or a manual/distributed setup.  
For normal local use, the built-in `Configure Local Sensors (hsen)` dialog is now the preferred path.

Related Windows helper notes:
- Quick copy/paste PowerShell snippets: `POWERSHELL_SNIPPETS.md`

If `start-hsenW.local.bat` is missing:
- `start-hsenW.bat` creates `hsen-interfaces-<COMPUTERNAME>.txt`
- it copies `start-hsenW.local.example.bat` to `start-hsenW.local.bat`
- set `HSEN_IFACE` in `start-hsenW.local.bat`

Example:
```bat
set "HSEN_IFACE=\Device\NPF_{E4ED794E-A66F-451C-851E-91226CD96BA4}"
```

## Runtime Dependencies and Common Errors

### Error: `libwinpthread-1.dll was not found`
- Re-run build scripts.
- They auto-copy `libwinpthread-1.dll` to the target output folder.

### `hsen.exe` packet-capture DLLs
- `wpcap.dll` and `Packet.dll` are copied from local `third_party` first.
- If missing there, scripts can copy from installed Npcap system paths.
- Public release ZIPs can be packaged without these DLLs; use a private/local `with-npcap` package only when you explicitly want to carry them for testing.

### Firewall
`hsen` communicates with `Hosts3D` over UDP port `10111`.

## Running and CLI
Start order does not matter.  
If `hsen` runs while `Hosts3D` is not listening, ICMP Port Unreachable (UDP 10111) can appear.

### Hosts3D
```text
hosts3d [-f]
  -f  full screen
```

### hsen
```text
hsen [-l] <id> <interface/file> [<destination>] [-p] [-d]
  -l          list capture interfaces
  id          sensor id (1-255)
  interface   interface name (or packet file)
  destination Hosts3D IP/broadcast (default: localhost)
  -p          promiscuous mode
  -d          daemon mode (Linux/macOS)
```

Interface listing:
```text
hsen -l
```

## Safe Stop (Windows)
Preferred for GUI-managed local sensors:
- use `Configure Local Sensors (hsen) > Stop` in Hosts3D

Fallback from the shell:

PowerShell:
```powershell
Get-Process Hosts3D,hsen -ErrorAction SilentlyContinue | Stop-Process
Get-Process Hosts3D,hsen -ErrorAction SilentlyContinue | Stop-Process -Force
```

cmd:
```cmd
taskkill /IM Hosts3D.exe /T
taskkill /IM hsen.exe /T
```

## Data Files
These are the files most users are most likely to encounter during normal operation.

Runtime data directory:
- Linux/macOS: `.hosts3d`
- Windows: `hsd-data`

Main files:

| File | Purpose |
|---|---|
| `controls.txt` | generated controls/help text |
| `settings.ini` | human-readable runtime settings |
| `0network.hnl` | network layout on exit; newly written layouts now use the versioned `HN2` format |
| `1network.hnl`..`4network.hnl` | saved layouts; newly written layouts now use the versioned `HN2` format |
| `netpos.txt` | CIDR-to-position/color mapping; exact `/32` entries also materialize known hosts at startup |
| `traffic.hpt` | Hosts3D packet traffic record/replay data (not PCAP) |
| `local-hsen.state` | machine-local state file for managed local `hsen` PIDs/process stamps on Linux/macOS |
| `local-hsen-windows.state` | machine-local state file for managed local `hsen.exe` PIDs/process stamps on Windows |
| `tmp-hinfo-hsd` | temporary host/selection info |
| `tmp-netpos-hsd` | temporary net positions |
| `tmp-flist-hsd` | temporary file list |

Notes:
- `settings.ini` is plain text and can be reviewed or edited with Hosts3D closed.
- If a legacy `settings-hsd` binary file is found, Hosts3D imports it once into `settings.ini`.
- `traffic.hpt` contains Hosts3D/hsen packet metadata for Record/Replay; it is not a Wireshark-compatible capture format.
- New recordings preserve packet-shape metadata; older `traffic.hpt` recordings remain readable.
- New packet recordings no longer overwrite the previous one: Hosts3D now auto-increments the name to the next free `traffic*.hpt` file in `hsd-data`.
- Newly written `.hnl` layout files use a versioned `HN2` format with explicit record sizes instead of raw in-memory structs.
- Older `HN1`/`HNL` layouts are still read when they pass basic size checks; obviously incompatible layout files are skipped cleanly with a warning instead of wedging startup.
- Runtime host lifetime/cleanup behavior is configured in the `[dynamic_hosts]` section of `settings.ini`.

### Keyboard Customization
- Keyboard bindings live in the `[keybindings]` section of `settings.ini`.
- `controls.txt` is regenerated from the current bindings when Hosts3D starts.
- Supported examples: `1`, `0`, `F7`, `F10`, `F11`, `F12`, `Ctrl + K`, `Ctrl + Shift + F1`, `Tab`, `Plus`, `Minus`, `Comma`, and `Period`.

### Dynamic and Static Hosts
- Automatically discovered traffic hosts start as `dynamic`.
- Dynamic hosts are automatically removed again after `dynamic_host_ttl_seconds` of inactivity when dynamic cleanup is enabled. This is meant to keep one-off Internet/test sources from staying in the scene indefinitely.
- Dynamic cleanup skips hosts that are currently selected or locked.
- Hosts loaded from a saved layout, created manually, manually edited (`Name`/`Remarks`), or named through `Selection > Resolve Hostnames` are treated as `static`.
- Exact `/32` entries in `netpos.txt` are also treated as known hosts: Hosts3D materializes them at startup, keeps them static, and shows them immediately even when `On Activity` is set to `Highlight Host`.
- Locked hosts are protected from dynamic cleanup and are also kept when layouts are saved.
- Saved layouts (`0network.hnl`..`4network.hnl`) persist static and locked hosts only.
- When a new build first opens an older incompatible `0network.hnl`, Hosts3D now skips that file, continues running, and writes a fresh compatible layout on exit.
- `[dynamic_hosts]` keys:
  - `dynamic_hosts_enabled=1`
  - `dynamic_host_ttl_seconds=300`
  - `dynamic_host_cleanup_interval_seconds=30`

### Local `hsen`
- `Configure Local Sensors (hsen)` now uses the same GUI on Windows and Linux.
- Open it by right-clicking in the 3D view and choosing `Configure Local Sensors (hsen)`.
- Interface discovery is done by calling `hsen -l`.
- Target host for local GUI-managed sensors is fixed to `127.0.0.1`.
- Ethernet adapters are preselected by default; WLAN and other adapters are listed but start deselected.
- `Save` stores the current selection.
- On Linux, `Save` also tries a one-time automatic `setcap` on the bundled `hsen` binary so local capture can run without a manual remote fix.
- `Start` launches one local `hsen` process per selected interface.
- `Stop` stops only the local `hsen` processes that were started and tracked by Hosts3D.
- On Windows, `Stop` and normal `Exit` now also re-scan for matching bundled `hsen.exe` processes from the same installation path instead of trusting only the saved PID list.
- On Windows, managed local `hsen.exe` validation now checks the full bundled executable path, not just the bare `hsen.exe` process name.
- On Linux, GUI-managed local `hsen` now records the real running process so `Stop` and `Exit` can terminate it reliably instead of leaving a launcher shell behind.
- `Stop` and normal `Exit` on Linux also sweep matching bundled local `hsen` processes from the same installation, so older orphaned sensor processes from previous runs get cleaned up too.
- Linux local `hsen` starts are now detached from the GUI process and brief zombie startup failures are treated as failed launches instead of as running sensors.
- On Linux, Hosts3D now marks its UDP receive socket close-on-exec and retries the packet-socket bind once after cleaning up orphaned managed local `hsen` processes, so stale previous runs do not silently break packet reception.
- The corresponding `settings.ini` keys are written under `[local_hsen]`.
- If automatic setup is not possible, Hosts3D shows the exact one-time `setcap` command to run on Linux.
- On Linux/macOS, `hsen_start_command` in `[hsen]` is still used as the launcher when you intentionally want something custom like `sudo -n hsen`.
- `start-hsenW.bat` remains the manual fallback for distributed/remote sensor setups.

### Synthetic test sender
- `testing/sim-hsen.ps1` sends synthetic `hsen`-compatible UDP packet metadata directly to `Hosts3D`.
- It is intended for visualization checks, OSD/menu checks, and quick regression demos without a live capture adapter.
- On normal Windows test machines, this PowerShell variant is the preferred helper because it does not require Python.
- `testing/sim-hsen.py` is a small Python 3 variant derived from the historical 2009 script.

## Controls and Interaction
This is the main usage section for moving around the scene and operating the UI.

Press `H` in Hosts3D to toggle a non-blocking in-app help overlay (`controls.txt` content).

Source of truth:
- `settings.ini`: `[keybindings]` stores the current keyboard mapping
- `src/hosts3d.cpp`: `checkControls()` writes `controls.txt`
- `src/hosts3d.cpp`: `keyboardGL()`, `clickGL()`, `wheelGL()`, `motionGL()`

Maintenance rule:
- If default controls or help descriptions change, update both `README.md` and `checkControls()` to keep docs and in-app help synchronized.

Runtime behavior not explicitly listed in `controls.txt`:
- Single-clicking a multi-host collision object cycles through hosts in that cluster.
- Mouse wheel in 3D mode moves camera up/down directly (no modifier required).
- The in-app help reflects the current bindings from `settings.ini`; the list below shows the default mapping.
- When the help overlay is visible, the mouse wheel scrolls the help only while the mouse cursor is over the overlay.
- Menus now show state markers directly: `[X]/[ ]` for toggles, `(*)/( )` for current mode choices.
- The top-right OSD now spells out current filters and toggles using the same naming style as the menu and in-app help, for example `Display Mode`, `Display Scope`, `On Activity`, and `Packet Limit`.
- Most mode/toggle rows in the top-right OSD can now be clicked directly to cycle them, so you do not have to remember the corresponding keyboard shortcut first.
- The OSD is grouped into `FILTERS`, `LABELS`, `PACKETS`, and `RUNTIME`, with grey labels, white values, yellow highlights for active deviations, and red alerts for important attention states.
- The `LABELS` group now also shows `Known NetPos Exact`, so it stays obvious when exact host rules from `netpos.txt` are being kept visible and labelled independently of the normal on-active host mode.
- The packet legend inside the OSD now renders actual miniature 3D packet examples at an angled view instead of flat markers, using the same shapes as the live animated packet objects.
- Clicking a packet example inside the OSD legend immediately applies the matching packet-tree filter.
- Packet traffic filters are exclusive: at any moment you either see all packet traffic, or exactly one active packet filter, whether that filter was chosen by sensor, the packet tree, or a port filter.
- The currently active OSD packet-example row is highlighted directly in the legend.
- The `Packets` menu mirrors the same active filter state for sensor, filter tree, and port using the same visible choice markers.
- Active packet recording and replay now show a dedicated bottom-left status panel with the current mode, elapsed time, replay packet time, and the active `traffic.hpt` label.
- Replay now opens a list of available `.hpt` recordings from `hsd-data`, instead of always replaying a single fixed file.
- Selected host details now include the last observed protocol, packet form, likely relevant port, and discovery name when available.
- Legacy short forms such as `Sen`, `Pro`, `Prt`, `Act`, and `Pkts` are no longer used in the OSD.
- `Esc` closes the open menu or dialog.

### Default Controls List

```text
Left Mouse Button	Select Host
	Click-and-Drag to Select Multiple Hosts
	Click Selected Host to Toggle Persistent Host Labels
Middle Mouse Button	Click-and-Drag to Change View
Right Mouse Button	Show Menu
Left Mouse Button on OSD Row	Cycle Clicked OSD Setting or Toggle
Left Mouse Button on OSD Packet Example	Apply Matching Packet Filter
Esc	Close Open Menu or Dialog
	Menu state markers: [X]/[ ] = toggle, (*)/( ) = current mode
Mouse Wheel	Move Up/Down
Up/Down	Move Forward/Back
Left/Right	Move Left/Right
Shift + Up/Down/Left/Right	Move at Triple Speed
Home	Recall Home View
Ctrl + Home	Recall Alternate Home View
F1	Recall View Position 1
F2	Recall View Position 2
F3	Recall View Position 3
F4	Recall View Position 4
Ctrl + F1	Save View Position 1
Ctrl + F2	Save View Position 2
Ctrl + F3	Save View Position 3
Ctrl + F4	Save View Position 4
Shift + F1	Restore Network Layout 1
Shift + F2	Restore Network Layout 2
Shift + F3	Restore Network Layout 3
Shift + F4	Restore Network Layout 4
Ctrl + Shift + F1	Save Network Layout 1
Ctrl + Shift + F2	Save Network Layout 2
Ctrl + Shift + F3	Save Network Layout 3
Ctrl + Shift + F4	Save Network Layout 4
Ctrl	Multi-Select
Ctrl + A	Select All Hosts
Ctrl + S	Invert Selection
Q/E	Move Selection Up/Down
W/S	Move Selection Forward/Back
A/D	Move Selection Left/Right
F	Find Hosts
Tab	Select Next Host in Selection
Ctrl + Tab	Select Previous Host in Selection
T	Toggle Persistent Host Labels for Selection
C	Cycle Show IP - IP/Name - Name Only
Ctrl + D	Toggle Create Hosts for Destination Targets [D]
M	Create Host...
N	Edit Hostname for Selected Host
Ctrl + N	Select All Named Hosts
R	Edit Remarks for Selected Host
L	Start Link Line from Selected Host (then press on the other host)
Ctrl + L	Start Delete Link Line from Selected Host (then press on the other host)
Y	Auto Link All Hosts
Ctrl + Y	Toggle Auto Link New Hosts [L]
J	Auto Link Hosts in Selection
Ctrl + J	Stop Auto Link Hosts in Selection
Ctrl + R	Delete Link Lines for All Hosts
P	Show Packets for Selection
Ctrl + P	Stop Showing Packets for Selection
U	Packet Display On
Ctrl + U	Toggle Show Packets for New Hosts [P]
1	Show Packets from Sensor 1
2	Show Packets from Sensor 2
3	Show Packets from Sensor 3
4	Show Packets from Sensor 4
5	Show Packets from Sensor 5
6	Show Packets from Sensor 6
7	Show Packets from Sensor 7
8	Show Packets from Sensor 8
9	Show Packets from Sensor 9
0	Show Packets from All Sensors
Comma / Period	Change Sensor to Show Packets from
B	Toggle Simulate Broadcast to all Known Net Hosts [B]
Minus	Decrease Allowed Packets
Plus	Increase Allowed Packets
Ctrl + T	Toggle Show Packet Destination Port
Z	Toggle Double Speed Packets [F]
K	Packet Display Off
F12	Start Recording Packet Traffic
F10	Replay Packet Traffic File...
Page Up	Skip to Next Packet during Replay Traffic
F11	Stop Recording / Replay
F7	Open Packet Traffic File...
F8	Save Packet Traffic File As...
Space	Pause / Resume Packet Animation
Ctrl + X	Cut Input Box Text
Ctrl + C	Copy Input Box Text
Ctrl + V	Paste Input Box Text
Ctrl + K	Acknowledge All Anomalies
O	Show OSD / Hide OSD
X	Export Selection Details in CSV File As...
I	Host Information
G	Information for Hosts in Selection
H	Toggle Help Overlay
```

## Right-Click Menu Map (1:1 From `mnuProcess()`)
This section is reference material for the current menu tree. You do not need it to get the program running.

All labels below are taken from `src/hosts3d.cpp` menu construction code.
The running UI is authoritative: menu items now show direct shortcuts in parentheses where a real keyboard command exists, and stateful items show `[X]/[ ]` or `(*)/( )`. For readability, the map below focuses on structure and the most important default bindings.

### Main Menu
| Menu | Items |
|---|---|
| `MAIN` | `Selected Host` (if a host is selected), `Selection of Hosts`, `Host Lines`, `Anomalies`, `Host Labels`, `Packet Filters`, `Packet Display On/Off`, `Pause/Resume Packet Animation`, `Packet Capture & Replay`, `On Activity`, `View`, `Net Layout`, `Net Positions Editor`, `Create Host...`, `Select Inactive Hosts`, `Find Hosts...`, `Show OSD / Hide OSD`, `Help`, `About Hosts3D`, `Configure Local Sensors (hsen)`, `Exit` |

### Menu Orientation
Use this as a quick guide for where to look first in day-to-day work.

- `[Frequently Used]` `Selected Host`, `Selection of Hosts`, `Host Labels`, `Packet Filters`, `Packet Display On/Off`, `Pause/Resume Packet Animation`, `View`, `Find Hosts...`
- `[Occasional]` `Anomalies`, `On Activity`, `Net Layout`, `Select Inactive Hosts`, `Show OSD / Hide OSD`, `Help`, `About Hosts3D`
- `[Advanced]` `Host Lines`, `Packet Capture & Replay`, `Net Positions Editor`, `Create Host...`, `Configure Local Sensors (hsen)`

The intent is:
- start with `Selection of Hosts` for most host workflows
- use `Packet Filters` plus `Packet Display` / `Packet Animation` for packet visibility work
- treat `Host Lines`, `Packet Capture & Replay`, `Net Positions Editor`, and local sensor configuration as advanced workflows
- keep `View` and `Find Hosts...` as fast everyday navigation tools

### `Selected Host`
| Item |
|---|
| `Host Information (I)` |
| `Show Only This Host's Packets` |
| `Move Here` |
| `Go To` |
| `Hostname (N)` |
| `Remarks (R)` |
| `Add This Host To Net Positions` |

### `Selection of Hosts`
| Item |
|---|
| `SELECTION TOOLS` |
| `Select All Hosts (Ctrl+A)` |
| `Invert Current Selection (Ctrl+S)` |
| `Select All Named Hosts (Ctrl+N)` |
| `Persistent Host Labels for Selection (T)` |
| `Show Packets for Selection (P)` |
| `Stop Showing Packets for Selection (Ctrl+P)` |
| `Export Selection Details in CSV File As... (X)` |
| `Information for Hosts in Selection (G)` |
| `Add Selected Hosts To Net Positions` |
| `Resolve Hostnames for Hosts in Selection` |
| `Combine by DNS Suffix (4+ Layers)` |
| `Combine by DNS Suffix (3 Layers)` |
| `Combine by DNS Suffix (2 Layers)` |
| `Set Host Colour` (inline colour boxes) |
| `Lock the Position` |
| `Move To Colour Zone` |
| `Auto Arrange` |
| `Run Commands` |
| `Reset` |
| `Delete Hosts in Selection` |

#### `Selection of Hosts > Set Host Colour`
Inline colour boxes: `Grey`, `Orange`, `Yellow`, `Fluro`, `Green`, `Mint`, `Aqua`, `Blue`, `Purple`, `Violet`

#### `Selection of Hosts > Lock the Position`
`On`, `Off`

#### `Selection of Hosts > Move To Colour Zone`
`Grey`, `Blue`, `Green`, `Red`

#### `Selection of Hosts > Auto Arrange`
`Default`, `10x10`, `10x10 2xSpc`, `Arrange By Net Positions`

#### `Selection of Hosts > Run Commands`
`Command 1`, `Command 2`, `Command 3`, `Command 4`, `Set`

#### `Selection of Hosts > Reset`
`Reset Traffic Counters`, `Services`

### `Host Lines`
| Item |
|---|
| `WITH SELECTED HOST` |
| `Start Link Line from Selected Host (L)` |
| `Start Delete Link Line from Selected Host (Ctrl+L)` |
| `AUTOMATIC` |
| `Auto Link All Hosts (Y)` |
| `Auto Link Hosts in Selection (J)` |
| `Stop Auto Link Hosts in Selection (Ctrl+J)` |
| `Auto Link New Hosts (Ctrl+Y)` |
| `DELETE` |
| `Delete Link Lines for Hosts in Selection` |
| `Delete All Link Lines (Ctrl+R)` |

### `Anomalies`
| Item |
|---|
| `Select Hosts With Anomalies` |
| `ACKNOWLEDGE ANOMALIES` |
| `Acknowledge Current Selection` |
| `Acknowledge All Anomalies (Ctrl+K)` |
| `ANOMALY DETECTION` |
| `Anomaly Detection On` |
| `Anomaly Detection Off` |

### `Host Labels`
Conditional items depend on current display mode.

| Possible item |
|---|
| `Show Labels for Hosts in Selection` |
| `Show Labels for All Hosts` |
| `Show Labels On Activity` |
| `Hide Standard Labels` |
| `Hide All Host Labels` |

### `Packet Filters`
| Item |
|---|
| `Sensor Filter` (inline choice boxes: `All`, `1`, `2`, `3`, `4`, `5`, `6`, `7`, `8`, `9`) |
| `Packet Type Filter` |
| `Port Filter` |
| `Select Hosts With Packet Display` |

### Top-Level Packet Toggles
| Item |
|---|
| `Packet Display On (U)` / `Packet Display Off (K)` |
| `Pause Packet Animation (Space)` / `Resume Packet Animation (Space)` |

### `Packet Capture & Replay`
| Item |
|---|
| `CAPTURE` |
| `Start Recording Packet Traffic (F12)` |
| `Stop Recording / Replay (F11)` |
| `REPLAY` |
| `Replay Packet Traffic File... (F10)` |
| `Skip to Next Replay Packet (Page Up)` |
| `FILES` |
| `Open Packet Traffic File... (F7)` |
| `Save Packet Traffic File As... (F8)` |

The top-right OSD also shows:
- `Packet Capture & Replay`: `Idle`, `Recording HH:MM:SS`, or `Replay`
- `Replay Packet Time`: the exact timestamp of the replay packet currently being shown

### Top-Level Host Creation
| Item |
|---|
| `Create Host... (M)` |

### Top-Level OSD Toggle
| Item |
|---|
| `Show OSD` / `Hide OSD` |

#### `Packet Filters > Packet Type Filter`
Current packet-tree items:

```text
All Traffic
TCP
 TCP control / no payload
 TCP setup / finish
 TCP reset
 TCP payload
 TCP larger payload
 TCP discovery
UDP
 UDP payload
 UDP larger payload
 UDP discovery
ICMP
ARP
 ARP request
 ARP reply
 ARP gratuitous
OTHER
 OTHER fragmented
```

#### `Packet Filters > Port Filter`
| Possible item |
|---|
| `All` |
| `Enter...` |

### `On Activity`
Conditional by active on-active mode.

| Possible item |
|---|
| `Create Activity Alert` |
| `Show Label On Activity` |
| `Highlight Host` |
| `Add Host to Selection On Activity` |
| `No Extra Action On Activity` |

### `View`
| Item |
|---|
| `LOAD VIEW` |
| `Home (Home)` |
| `Alternate Home (Ctrl+Home)` |
| `View 1 (F1)` |
| `View 2 (F2)` |
| `View 3 (F3)` |
| `View 4 (F4)` |
| `SAVE VIEW` |
| `View 1 (Ctrl+F1)` |
| `View 2 (Ctrl+F2)` |
| `View 3 (Ctrl+F3)` |
| `View 4 (Ctrl+F4)` |

### `Net Layout`
| Item |
|---|
| `LOAD LAYOUT` |
| `Open Layout File...` |
| `Layout 1 (Shift+F1)` |
| `Layout 2 (Shift+F2)` |
| `Layout 3 (Shift+F3)` |
| `Layout 4 (Shift+F4)` |
| `SAVE LAYOUT` |
| `Save To Layout File...` |
| `Layout 1 (Ctrl+Shift+F1)` |
| `Layout 2 (Ctrl+Shift+F2)` |
| `Layout 3 (Ctrl+Shift+F3)` |
| `Layout 4 (Ctrl+Shift+F4)` |
| `Clear Current Layout` |

### `Net Positions Editor`
| Item |
|---|
| `Net Positions Editor` |

### `Select Inactive Hosts`
| Item |
|---|
| `Older Than 5 Minutes` |
| `Older Than 1 Hour` |
| `Older Than 1 Day` |
| `Older Than 1 Week` |
| `Custom Threshold...` |

### `Find Hosts`
| Item |
|---|
| `Find Hosts... (F)` |

### `Help`
| Item |
|---|
| `Help (H)` |

### `About Hosts3D`
| Item |
|---|
| `About Hosts3D` |

The `About Hosts3D` window shows the current version, continuation status, the GitHub project path, and a short GPL / no-warranty reminder.

### `Configure Local Sensors (hsen)`
- discovers interfaces via `hsen -l`
- shows Ethernet/WLAN/Other adapters in a selection dialog
- shows the current IPv4 addresses of each discovered adapter to help choose the right local sensor
- uses `Refresh`, `Save`, and a combined `Start` / `Stop` button
- no extra success popups for local hsen actions; only errors open a dialog

## Net Positions (`netpos.txt`)
This is an advanced layout feature. Use it when you want known networks to appear in predictable positions and colors.

Formats:
```text
pos net x-position y-position z-position colour [hold]
host ip=1.2.3.4 [mac=00:11:22:33:44:55] [fqdn=host.example] x=0 y=0 z=0 color=green [hold]
```

Example:
```text
pos 123.123.123.123/32 10 0 -10 green
pos 123.123.123.123/32 10 0 -10 green hold
host ip=123.123.123.123 x=10 y=0 z=-10 color=green
host ip=123.123.123.123 mac=00:11:22:33:44:55 fqdn=host.example x=10 y=0 z=-10 color=green hold
```

Important behavior:
- `pos ...` rules continue to work for CIDR nets and exact `/32` IPs.
- `host ...` rules are for exact host identity and may use any combination of `ip=`, `mac=`, and `fqdn=`.
- In `host ...` rules, all given identity fields are AND-linked. If one does not match, the rule is ignored.
- Rules are evaluated strictly from top to bottom. The first fully matching rule wins, and all following rules are ignored.
- Exact rules with an IP (`pos .../32` and `host ip=...`) now also materialize those hosts immediately at startup if they are not already present in the current layout.
- These exact hosts are treated as known/static hosts, inherit their `netpos.txt` position/color, and show an IP or resolved name immediately.
- If an exact host already exists in the layout, `netpos.txt` still wins for its startup position/color.
- Normal colour rules use the given position as an anchor and move colliding hosts to the nearest free position around it.
- Adding `hold` disables that automatic offset, so the host stays exactly on the configured position even when it collides.

Host positioning axes:
- Grey/Red: positive x
- Blue/Green: negative x
- Up: positive y
- Down: negative y
- Grey/Blue: positive z
- Red/Green: negative z

Allowed color tokens:
- `default`, `orange`, `yellow`, `fluro`, `green`, `mint`, `aqua`, `blue`, `purple`, `violet`, `hold`

`hold` can be used by itself or after a colour token.

- `green` means: set colour and allow collision-avoidance offsetting
- `hold` means: keep the current colour and do not offset
- `green hold` means: set colour and also keep the host fixed exactly at that position

### Special Address / Identity Cases (Design Notes, Not Yet Implemented)
These are candidate rules under discussion for future automatic placement and display behavior. They do not describe the current implementation yet.

| Case | Current behavior | Candidate rule |
|---|---|---|
| `0.0.0.0` | Can appear as a normal IP host if traffic or `netpos` creates it. | Treat as a single grey meta-host at a reserved position, do not DNS-resolve it, and do not promote it to a normal confirmed host. |
| `255.255.255.255` | Can appear as a normal host in some paths, while broadcast handling also has separate fan-out logic. | Treat as one dedicated global broadcast marker at a reserved position, not as a normal host. |
| Mask-like addresses such as `255.255.255.0` | Treated as normal IP hosts today. | Treat as suspicious/meta addresses by default, unless an explicit `/32` `netpos` rule says otherwise. |
| Loopback `127.0.0.0/8` | Treated as normal IP hosts today. | Reserve a dedicated loopback marker or loopback zone and avoid normal hostname resolution for it. |
| Multiple IPs with the same MAC | Separate hosts with no visible relationship. | Keep separate IP hosts, but prefer nearby placement and add a future visual relation instead of auto-merging them. |
| One IP seen with multiple MACs | Host identity stays IP-based; the first seen MAC tends to stick. | Keep one IP host, but flag a MAC-conflict/anomaly state instead of silently trusting the first MAC forever. |

Planned precedence rule:
- explicit `/32` `netpos` rules should continue to override future automatic special-case placement rules

## Visual Mapping (Current Code)
This section helps you map what you see on-screen back to the current implementation (`src/misc.h`, `src/objects.cpp`, `src/hosts3d.cpp`).

### Palette (RGB)
| Name | RGB |
|---|---|
| white | `255,255,255` |
| bright red | `255,50,50` |
| red | `200,50,50` |
| orange | `220,170,50` |
| yellow | `200,200,50` |
| fluro | `170,220,50` |
| green | `50,200,50` |
| mint | `50,220,170` |
| aqua | `50,200,200` |
| sky (blue host tone) | `50,170,220` |
| blue | `50,50,200` |
| purple | `170,50,220` |
| violet | `200,50,200` |
| bright grey | `200,200,200` |
| grey | `150,150,150` |
| dull grey | `100,100,100` |

### Host and Packet Visual Rules
| Element | Mapping |
|---|---|
| Host colors | default/grey, orange, yellow, fluro, green, mint, aqua, blue(sky), purple, violet |
| Selected host | bright red |
| Multi-host collision object | red/yellow/green tones; selected variant in bright red tones |
| ICMP packet | red pyramid |
| TCP control / no payload | green cube |
| TCP setup/finish control packet | orange cube |
| TCP reset packet | red cube |
| TCP payload packet | green double/triple cuboid |
| TCP discovery packet | green sphere |
| UDP payload packet | blue double/triple cuboid |
| UDP discovery packet | blue sphere |
| ARP request | yellow cube |
| ARP reply | yellow double cuboid |
| ARP gratuitous | yellow sphere |
| Other packet | grey cube |
| Other fragmented packet | grey pyramid |
| Packet form: cube | control traffic such as TCP control packets or ARP requests |
| Packet form: double cuboid | payload-carrying traffic or ARP replies |
| Packet form: triple cuboid | larger payload-carrying traffic |
| Packet form: sphere | name/discovery traffic or gratuitous ARP |
| Packet form: pyramid | ICMP or fragmented traffic |
| Packet legend in OSD | rendered as miniature 3D examples of the real packet objects, shown from an angled view and clickable for packet-tree filters |
| Anomaly alert | bright red |
| On-active alert | protocol color |
| Link lines | dull grey |
| OSD/text/labels/port labels | white |

### 3D Object Types
| Object | Purpose |
|---|---|
| Cross object | orientation/zone reference |
| Host object | normal host marker |
| Multiple-host object | host collision marker |
| Packet object | animated packet marker |
| Alert object (active) | expanding wireframe box |
| Alert object (anomaly) | expanding anomaly wireframe marker |

## In-App Help
Press `H` to toggle the help overlay (`controls.txt`, generated from code).

## Behavior and Limits
- IPv4 only
- IP headers with options are ignored
- Supports optionless GRE and VLAN 802.1Q encapsulation
- Protocol `249` is used internally to identify ARP packets
- Protocol `250` is used internally to identify fragmented IP packets
- ARP traffic is distinguished into request, reply, and gratuitous ARP
- Default host creation is source-IP based; enable Create Hosts for Destination Targets to include destination-side targets as hosts
- Anomalies represent new hosts or new host services
- Dynamic hosts are automatically removed again after the configured inactivity timeout in `[dynamic_hosts]`, unless they are currently selected, locked, or have already been promoted to static
- Large menu operations on thousands of hosts can take minutes
- `settings.ini` is plain text and portable across 32-bit/64-bit builds
- Newly written `.hnl` layouts now use the versioned `HN2` format and are validated before loading
- Legacy raw layout/traffic files such as `HN1`/`HNL` and older `.hpt` recordings remain more sensitive to binary compatibility
- On Windows, running configured system commands can stall Hosts3D until command completion

## Troubleshooting Tip
On Linux/macOS/FreeBSD, both `Hosts3D` and `hsen` log to syslog.

## Project Continuation and Historical Reference
This repository is an independent, community-maintained continuation of the original Hosts3D 1.15 codebase.

- Original upstream project and authorship remain credited to Del Castle.
- This repository is not an official upstream by the original author.
- Historical upstream reference: <http://hosts3d.sourceforge.net/>

For attribution and maintenance context:
- See `CREDITS.md`
- See `RELEASE_NOTES_1.18.md`, `RELEASE_NOTES_1.17.md`, and `RELEASE_NOTES_1.16.md` for continuation history
- See `RELEASE_NOTES_1.15.md` for legacy upstream history up to original `1.15`

## License
GNU General Public License v2

## Reporting Bugs
Open an issue in this repository with repro steps and platform/build details.
- <https://github.com/kaestnja/hosts3d/issues>

## Copyright
Copyright (c) 2006-2011 Del Castle
