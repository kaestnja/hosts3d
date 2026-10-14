# Hosts3D 1.19 for Windows

## Shared ScriptCommands

External commands are optional. Host/traffic views, capture/replay and saved or
manually maintained topology remain available without them. Missing commands for
enabled device families produce an unavailable refresh and retain the last
complete topology; no demo data is reported as a live query. Unused families do
not need their commands installed.

Hosts3D uses an optional external script directory for its
SCALANCE XR328 and XC208G queries. In `hsd-data/settings.ini`, set:

```ini
[scriptcommands]
scriptcommands_path=C:\Tools\ExternalScripts
```

For example, place Hosts3D at `C:\Tools\Hosts3D` and select the external script
directory above. On another computer, select its configured directory; the same
sibling layout can use `scriptcommands_path=..\ExternalScripts`.
Relative paths resolve against the
Hosts3D executable directory. Edit `settings.ini` with Hosts3D closed and restart
it after a path change; ordinary command edits need no restart.

Install Python 3.10 or newer and provide Net-SNMP `snmpget`/`snmpwalk` on PATH or
beside Hosts3D.exe. F9 (or **View > Refresh Switch Topology**) calls the existing
refresh runner, which reads enabled entries from `hsd-data/switches.txt` and
passes their connection parameters to the shared commands. For example:

```text
switch name=switch1 type=scalance_xr328 host=192.0.2.10 version=2c community=COMMUNITY enabled=1 auto_refresh=1 refresh_seconds=60
```

Use `type=scalance_xc208g` for XC208G. SNMPv3 accepts `user`, `level`, `auth_proto`,
`auth_pass`, `priv_proto`, `priv_pass`; `user_env`, `auth_pass_env`, `priv_pass_env`
refer to environment variables. For SNMPv1/v2c, provide `community` explicitly;
`community_fallback` optionally supplies one explicit alternative. The earlier
lab default must be represented by a local switch row. No device is queried
when there are no enabled rows. Missing checkout files are reported at refresh.

Raw observations stay in `hsd-data/snmp/`; Hosts3D renders `switch-topology.txt`.
The last complete topology remains until every enabled switch query succeeds.
Configuration and rendered data remain in Hosts3D; reusable device commands and
their protocol notes/tests are in the shared checkout.

To fix or extend a command, read that checkout's `CONTRACT.md`, edit and test
there, then commit and push there. Use `git pull --ff-only` in the same checkout
to update it. Wait for an active refresh to finish before updating. There is no
extra package or update mechanism. For a new device family, add a shared command
and its type mapping in `Tools/snmp/scalance_switches_refresh.py`.


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
- `Tools\snmp\`

Private/local test packages may additionally include:
- `Packet.dll`
- `wpcap.dll`

Packages built after the optional Net-SNMP helper step may additionally include:
- `snmpget.exe`
- `snmpwalk.exe`
- `snmpset.exe`

The optional `Tools\snmp\` folder contains switch-specific diagnostic helpers.
Current SCALANCE helper files live there as:
- `Tools\snmp\scalance_switches_refresh.py`
- `Tools\snmp\scalance_xr328_mirror_check.py`
- `Tools\snmp\scalance_xr328_mirror_check.md`
- `Tools\snmp\scalance_xc208g_mirror_check.py`
- `Tools\snmp\scalance_xc208g_mirror_check.md`
- `Tools\snmp\README.md`
- `Tools\snmp\run-scalance-check.ps1`
- `Tools\snmp\run-scalance-check.cmd`

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

The F9 switch-topology scene can also create and use:
- `hsd-data\switches.txt`
- `hsd-data\snmp\*.json`
- `hsd-data\switch-topology.txt`

F9 starts the SCALANCE refresh runner. With no enabled switch rows, it reports missing configuration and preserves the existing topology. With enabled rows in `switches.txt`, it queries them serially by `type=...` and writes raw per-switch JSON under `hsd-data\snmp\`. The combined topology is built in a temporary file and replaces `switch-topology.txt` only after all enabled switch helpers succeed, so the last complete topology stays visible during refresh. With `auto_refresh=1`, an enabled row refreshes automatically; `View > Refresh Switch Topology` starts the same refresh manually.

The F9 switch-topology scene draws only hosts that can be assigned to a switch port. It keeps host labels stacked over multiple short lines, merges safe LLDP/FDB identity pairs in the topology data, keeps the last successfully loaded topology visible while a refresh is running, and separates multiple switches into device-shaped layouts. For 28-port XR328 switch blocks, it uses the front-panel order P1-P12 over P13-P24 with P25/P26 over P27/P28; hosts connected to the upper and lower physical port rows are drawn on opposite sides. XC208G switch blocks are drawn as vertical P1-P8 port columns, with hosts to the right of their ports and multiple XC208G switches arranged side by side. Mirror state and port status are shown through port colors and the F9 OSD legend; grey is normal, red is `port down` (`up=0`), and mirror link lines are intentionally not drawn.

`scalance_xc208g_mirror_check.py` is a shared read-only command and can be selected by `type=scalance_xc208g` in `switches.txt`. It shares the XR328 JSON contract, receives explicit `snmpget.exe` and `snmpwalk.exe` paths from Hosts3D when available, and has been validated against SCALANCE XC208G devices with 8 physical ports.

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
- `snmpget.exe`, `snmpwalk.exe`, and `snmpset.exe` are optional administrative helpers for SNMP-based switch diagnostics and later mirror-port management. If present, they are self-contained command line tools built with static OpenSSL and static MSVC runtime linkage.
- Only the Hosts3D refresh runner is staged below `Tools\snmp\`. Device commands and protocol notes stay in the configured ScriptCommands checkout.
- For SNMPv1/v2c provide `community=...` directly; for SNMPv3 passwords prefer `user_env`, `auth_pass_env`, and `priv_pass_env` in `switches.txt`.

## License
Hosts3D is distributed under the GNU General Public License. See `COPYING`.
This package is provided without warranty. Current project page: <https://github.com/kaestnja/hosts3d>.
