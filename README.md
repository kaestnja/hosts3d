# Hosts3D 1.17

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
3. Right-click in the 3D view, then choose `Local hsen`
4. Select one or more capture interfaces
5. Click `Save + Start`

Recommended paths:
- Windows 11:
  - install the MinGW/MSYS2 toolchain once
  - run `.\compile-hosts3d.bat`
  - run `.\compile-hsen.bat`
  - start `.\start-Hosts3DW.bat`
  - in Hosts3D, right-click in the 3D view, then choose `Local hsen`
- Linux/macOS:
  - build with the platform commands below
  - if local capture needs privileges, set `hsen_start_command=sudo -n hsen` in `settings.ini`
  - start `Hosts3D`, right-click in the 3D view, then choose `Local hsen`

## Requirements
Use the platform-specific subsection that matches the machine you are building on.

### Hardware
- Scroll mouse
- OpenGL-capable graphics card with installed drivers

### Software By Platform
| Platform | Runtime | Build Toolchain |
|---|---|---|
| Linux | `libglfw`, `libpcap` | `g++`, `libglfw-dev`, `libpcap-dev` |
| macOS | GLFW | GCC/Clang + GLFW |
| FreeBSD | `hsen` supported | standard C/C++ toolchain |
| Windows | Npcap/WinPcap-compatible runtime | MinGW (MSYS2) + GLFW + Wpcap/Npcap SDK files |

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

### macOS (legacy upstream flow)
```bash
./compile-hsen
./compile-mac-hosts3d
```

### FreeBSD (`hsen`)
```bash
./compile-hsen
```

### Windows 11 Build (MSYS2 + MinGW32)
> If `g++` is missing (`g++ is not recognized`), install toolchain first.

```powershell
winget install -e --id MSYS2.MSYS2
C:\msys64\usr\bin\bash -lc "pacman -Syu --noconfirm"
C:\msys64\usr\bin\bash -lc "pacman -S --needed --noconfirm mingw-w64-i686-gcc mingw-w64-i686-binutils make"
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
.\compile-hosts3d.bat
.\compile-hsen.bat
```

## Build Output Layout
Build scripts place binaries into config/os/arch folders:

| Binary | Release path | Debug path |
|---|---|---|
| Hosts3D | `Release/windows/x86/Hosts3D.exe` | `Debug/windows/x86/Hosts3D.exe` |
| hsen | `Release/windows/x86/hsen.exe` | `Debug/windows/x86/hsen.exe` |

Notes:
- Scripts overwrite known targets but do not wipe the whole `Release`/`Debug` tree.
- Local dependency layout for build scripts: see `third_party/README.md`.
- Archived old binaries for comparison: `Original/windows/x86/`.
- Portable Wireshark helper location: `Tools/Wireshark/`.

## Manual Start Helpers (Windows, Optional)
```powershell
.\start-Hosts3DW.bat
.\start-Hosts3DW.bat fullscreen
.\start-hsenW.bat
```

Use these helpers when you want the old script-driven workflow or a manual/distributed setup.  
For normal local use, the built-in `Local hsen` dialog is now the preferred path.

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
- use `Local hsen > Stop` in Hosts3D

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
| `0network.hnl` | network layout on exit |
| `1network.hnl`..`4network.hnl` | saved layouts |
| `netpos.txt` | CIDR-to-position/color mapping |
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
- Locked hosts are protected from dynamic cleanup and are also kept when layouts are saved.
- Saved layouts (`0network.hnl`..`4network.hnl`) persist static and locked hosts only.
- `[dynamic_hosts]` keys:
  - `dynamic_hosts_enabled=1`
  - `dynamic_host_ttl_seconds=300`
  - `dynamic_host_cleanup_interval_seconds=30`

### Local `hsen`
- `Local hsen` now uses the same GUI on Windows and Linux.
- Open it by right-clicking in the 3D view and choosing `Local hsen`.
- Interface discovery is done by calling `hsen -l`.
- Target host for local GUI-managed sensors is fixed to `127.0.0.1`.
- Ethernet adapters are preselected by default; WLAN and other adapters are listed but start deselected.
- `Save + Start` stores the current selection and launches one local `hsen` process per selected interface.
- `Stop` stops only the local `hsen` processes that were started and tracked by Hosts3D.
- The corresponding `settings.ini` keys are written under `[local_hsen]`.
- On Linux/macOS, `hsen_start_command` in `[hsen]` is still used as the launcher when you need something like `sudo -n hsen`.
- `start-hsenW.bat` remains the manual fallback for distributed/remote sensor setups.

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
- The top-right OSD now spells out current filters and toggles using the same names as `settings.ini`, for example `Display Mode`, `Display Scope`, `On-Active Action`, and `Packet Limit`.
- The OSD is grouped into `FILTERS`, `LABELS`, `PACKETS`, and `RUNTIME`, with grey labels, white values, yellow highlights for active deviations, and red alerts for important attention states.
- The packet legend inside the OSD now renders actual miniature 3D packet examples at an angled view instead of flat markers, using the same shapes as the live animated packet objects.
- Clicking a packet example inside the OSD legend immediately applies the matching packet-tree filter.
- Packet traffic filters are exclusive: at any moment you either see all packet traffic, or exactly one active packet filter, whether that filter was chosen by sensor, the packet tree, or a port filter.
- The currently active OSD packet-example row is highlighted directly in the legend.
- The `Packets` menu mirrors the same active filter state for sensor, filter tree, and port using the same visible choice markers.
- Selected host details now include the last observed protocol, packet form, important port, and discovery name when available.
- Legacy short forms such as `Sen`, `Pro`, `Prt`, `Act`, and `Pkts` are no longer used in the OSD.
- `Esc` closes the open menu or dialog.

### Default Controls List
Legacy spelling is preserved for parity: `Persistant`.

```text
Left Mouse Button	Select Host
	Click-and-Drag to Select Multiple Hosts
	Click Selected Host to Toggle Persistant IP/Name
Middle Mouse Button	Click-and-Drag to Change View
Right Mouse Button	Show Menu
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
T	Toggle Persistant IP/Name for Selection
C	Cycle Show IP - IP/Name - Name Only
Ctrl + D	Toggle Add Destination Hosts [D]
M	Make Host
N	Edit Hostname for Selected Host
Ctrl + N	Select All Named Hosts
R	Edit Remarks for Selected Host
L	Press Twice with Different Selected Hosts for Link Line
Ctrl + L	Delete Link Line (2nd Selected Host, Press Link on 1st)
Y	Automatic Link Lines for All Hosts
Ctrl + Y	Toggle Automatic Link Lines for New Hosts [L]
J	Automatic Link Lines for Selection
Ctrl + J	Stop Automatic Link Lines for Selection
Ctrl + R	Delete Link Lines for All Hosts
P	Show Packets for Selection
Ctrl + P	Stop Showing Packets for Selection
U	Show Packets for All Hosts
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
B	Toggle Show Simulated Broadcasts [B]
Minus	Decrease Allowed Packets
Plus	Increase Allowed Packets
Ctrl + T	Toggle Show Packet Destination Port
Z	Toggle Double Speed Packets [F]
K	Packets Off
F12	Record Packet Traffic
F10	Replay Recorded Packet Traffic
Page Up	Skip to Next Packet during Replay Traffic
F11	Stop Record/Replay of Packet Traffic
F7	Open Packet Traffic File...
F8	Save Packet Traffic File As...
Space	Toggle Pause Animation
Ctrl + X	Cut Input Box Text
Ctrl + C	Copy Input Box Text
Ctrl + V	Paste Input Box Text
Ctrl + K	Acknowledge All Anomalies
O	Toggle Show OSD
X	Export Selection Details in CSV File As...
I	Show Selected Host Information
G	Show Selection Information
H	Toggle Help Overlay
```

## Right-Click Menu Map (1:1 From `mnuProcess()`)
This section is reference material for the current menu tree. You do not need it to get the program running.

All labels below are taken from `src/hosts3d.cpp` menu construction code.
The running UI is authoritative: menu items now show direct shortcuts in parentheses where a real keyboard command exists, and stateful items show `[X]/[ ]` or `(*)/( )`. For readability, the map below focuses on structure and the most important default bindings.

### Main Menu
| Menu | Items |
|---|---|
| `MAIN` | `Selected` (if a host is selected), `Selection`, `Anomalies`, `IP/Name`, `Packets`, `On-Active`, `View`, `Layout`, `Other`, `Local hsen`, `Exit` (fullscreen only) |

### `Selected`
| Item |
|---|
| `Information (I)` |
| `Show Packets Only` |
| `Move Here` |
| `Go To` |
| `Hostname (N)` |
| `Remarks (R)` |
| `Add Net Position` |

### `Selection`
| Item |
|---|
| `Information (G)` |
| `Resolve Hostnames` |
| `Colour` |
| `Lock` |
| `Move To Zone` |
| `Arrange` |
| `Commands` |
| `Reset` |
| `Delete` |

#### `Selection > Colour`
`Grey`, `Orange`, `Yellow`, `Fluro`, `Green`, `Mint`, `Aqua`, `Blue`, `Purple`, `Violet`

#### `Selection > Lock`
`On`, `Off`

#### `Selection > Move To Zone`
`Grey`, `Blue`, `Green`, `Red`

#### `Selection > Arrange`
`Default`, `10x10`, `10x10 2xSpc`, `Into Nets`

#### `Selection > Commands`
`Command 1`, `Command 2`, `Command 3`, `Command 4`, `Set`

#### `Selection > Reset`
`Link Lines`, `Downloads`, `Uploads`, `Services`

#### `Selection > Delete`
`Confirm`

### `Anomalies`
| Item |
|---|
| `Select All` |
| `Acknowledge` |
| `Toggle Detection` |

#### `Anomalies > Acknowledge`
`Selection`, `All (Ctrl+K)`

### `IP/Name`
Conditional items depend on current display mode.

| Possible item |
|---|
| `Show Selection` |
| `Show All` |
| `Show On-Active` |
| `Show Off` |
| `All Off` |

### `Packets`
| Item |
|---|
| `Show All (U)` |
| `Sensor` |
| `Filter` |
| `Port` |
| `Select Showing` |
| `Off (K)` |

#### `Packets > Sensor`
| Possible item |
|---|
| `All` |
| `Sensor 1` |
| `Sensor 2` |
| `Sensor 3` |
| `Sensor 4` |
| `Sensor 5` |
| `Sensor 6` |
| `Sensor 7` |
| `Sensor 8` |
| `Sensor 9` |

#### `Packets > Filter`
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

#### `Packets > Port`
| Possible item |
|---|
| `All` |
| `Enter...` |

### `On-Active`
Conditional by active on-active mode.

| Possible item |
|---|
| `Alert` |
| `Show IP/Name` |
| `Show Host` |
| `Select` |
| `Do Nothing` |

### `View`
| Item |
|---|
| `Recall` |
| `Save` |

#### `View > Recall`
`Home (Home)`, `Alternate Home (Ctrl+Home)`, `Pos 1 (F1)`, `Pos 2 (F2)`, `Pos 3 (F3)`, `Pos 4 (F4)`

#### `View > Save`
`Pos 1 (Ctrl+F1)`, `Pos 2 (Ctrl+F2)`, `Pos 3 (Ctrl+F3)`, `Pos 4 (Ctrl+F4)`

### `Layout`
| Item |
|---|
| `Restore` |
| `Save` |
| `Net Positions` |
| `Clear` |

#### `Layout > Restore`
`File`, `Net 1 (Shift+F1)`, `Net 2 (Shift+F2)`, `Net 3 (Shift+F3)`, `Net 4 (Shift+F4)`

#### `Layout > Save`
`File`, `Net 1 (Ctrl+Shift+F1)`, `Net 2 (Ctrl+Shift+F2)`, `Net 3 (Ctrl+Shift+F3)`, `Net 4 (Ctrl+Shift+F4)`

#### `Layout > Clear`
`Confirm`

### `Other`
| Item |
|---|
| `Find Hosts (F)` |
| `Select Inactive` |
| `Help (H)` |
| `About` |

#### `Other > Select Inactive`
`> 5 Minutes`, `> 1 Hour`, `> 1 Day`, `> 1 Week`, `> Other`

### `Local hsen`
- discovers interfaces via `hsen -l`
- shows Ethernet/WLAN/Other adapters in a selection dialog
- `Save + Start`, `Stop`, `Refresh`, `Close`

## Net Positions (`netpos.txt`)
This is an advanced layout feature. Use it when you want known networks to appear in predictable positions and colors.

Format:
```text
pos net x-position y-position z-position colour
```

Example:
```text
pos 123.123.123.123/32 10 0 -10 green
```

Host positioning axes:
- Grey/Red: positive x
- Blue/Green: negative x
- Up: positive y
- Down: negative y
- Grey/Blue: positive z
- Red/Green: negative z

Allowed color tokens:
- `default`, `orange`, `yellow`, `fluro`, `green`, `mint`, `aqua`, `blue`, `purple`, `violet`, `hold`

`hold` keeps hosts in place (no color reassignment).

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
- Default host creation is source-IP based; enable Add Destination Hosts to include destination IPs
- Anomalies represent new hosts or new host services
- Dynamic hosts are automatically removed again after the configured inactivity timeout in `[dynamic_hosts]`, unless they are currently selected, locked, or have already been promoted to static
- Large menu operations on thousands of hosts can take minutes
- `settings.ini` is plain text and portable across 32-bit/64-bit builds
- Legacy binary layout/traffic files such as `.hnl` and `.hpt` remain architecture-specific
- On Windows, running configured system commands can stall Hosts3D until command completion

## Troubleshooting Tip
On Linux/macOS/FreeBSD, both `Hosts3D` and `hsen` log to syslog.

## Project Continuation and Historical Reference
This repository is an independent, community-maintained continuation of the original Hosts3D 1.15 codebase.

- Original upstream project and authorship remain credited to Del Castle.
- This repository is not an official upstream by the original author.
- Historical upstream reference: http://hosts3d.sourceforge.net/

For attribution and maintenance context:
- See `CREDITS.md`
- See `CHANGELOG.md` (continuation history)
- See `ChangeLog` (legacy upstream history up to 1.15)

## License
GNU General Public License v2

## Reporting Bugs
Open an issue in this repository with repro steps and platform/build details.
- https://github.com/kaestnja/hosts3d/issues

## Copyright
Copyright (c) 2006-2011 Del Castle
