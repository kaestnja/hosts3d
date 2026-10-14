# Work request: capture and sensor management

[Original German view](Todos.md). Both language views describe the same planning
record; keep their full meaning and status synchronized.

## Intended direction

Hosts3D should eventually prepare, manage and check capture infrastructure in
addition to starting it locally. This comprises two connected areas:

1. Managing and deploying HSEN sensors.
2. Managing and checking network devices' mirroring capabilities.

Initially both areas should provide observation and assistance. Administrative
changes to remote systems, switches and sensor hosts are explicitly part of the
later intended scope, with traceable, bounded actions and deliberately selected
permissions.

## Part 1: general capture and sensor management

### Managing and deploying HSEN sensors

The objective is an inventory and operations view of sensors that capture packets
locally or remotely and deliver them to Hosts3D.

Questions to resolve:

1. Which sensor instances exist?
2. On which host does each instance run?
3. Which `hsen` version is installed?
4. Which capture interfaces are available?
5. Which interfaces are currently active?
6. Where does the sensor send its data?
7. Which capture permissions or capabilities are missing?
8. How is the sensor started: manually, by Hosts3D, as a service, by a script or
   through external deployment?
9. When was the sensor last seen?
10. Which errors or warnings does it report?

Possible software extensions:

- Maintain sensor inventory in `settings.ini` or a separate runtime file.
- Extend the local HSEN editor with remote sensor entries.
- Show editor status fields: installed, reachable, running, stale, wrong_version,
  missing_capture_rights.
- Define a deployment profile: target host, OS, architecture, installation path,
  start method and Hosts3D destination address.
- Generate a package or archive for sensor deployment.
- Offer an optional readiness check that determines whether capture could start
  without making persistent changes.
- Offer Start/Stop/Restart only for sensors whose management mode is explicit.

Operations and permissions requirements:

- Do not write passwords or tokens unnecessarily to plaintext logs.
- Do not install remotely implicitly; require an explicit action.
- Grant local capture rights according to least privilege: as little as practical,
  while sufficient for the requested capture or management operation.
- For example, prefer Linux `setcap` for capture alone, while allowing root or
  administrator rights when installation, service management or device
  configuration actually requires them.
- Treat sensor communication and administrative access separately.
- Label automatically determined states with a timestamp and source.

### Managing network devices' mirroring capabilities

The objective is a view of whether and how network devices can provide capture
traffic to HSEN sensors. Begin with discovery and plausibility checks; deliberately
controlled switch configuration should become possible later.

Questions to resolve:

1. Which network devices are relevant to capture?
2. What device type, manufacturer and firmware version is present?
3. Does the device support port mirroring, SPAN, RSPAN, ERSPAN or TAP-like features?
4. Is mirroring globally enabled?
5. Which destination ports are configured?
6. Which source ports, VLANs or sessions are mirrored?
7. Is ingress, egress or both mirrored?
8. Does the device support several mirror sessions or only one?
9. Which ports connect sensors, uplinks or endpoints?
10. Which neighbor data from LLDP, MAC tables, ARP/DNS/DHCP or asset information
    could help?

Possible software extensions:

- A new model for "Capture Sources", "Capture Sensors" and "Mirror Providers".
- Add a "Prepare Capture" or "Mirror Check" area to the local HSEN editor.
- For a selected sensor interface, show which switch ports could be mirror
  destinations.
- Initially provide read-only SNMP queries for known device classes as a diagnostic
  script or internal module.
- Define a switch query result format, such as JSON with safe values and derived
  correlations.
- Show a warning when a sensor is active but no matching mirroring is visible at
  the switch.
- Show a warning when mirroring is active but its destination port does not match
  the expected sensor.

Concrete initial implementation path:

1. Provide device-specific read-only diagnostics as an external JSON tool.
2. Stabilize the JSON structure against real switch outputs.
3. In parallel, assess SNMP procurement and implementation options: Net-SNMP binary,
   a reproducible local build, an equivalent SNMP CLI, a library or a separate
   management CLI.
4. Then add a small local HSEN editor integration: run "Mirror Check", display or
   summarize the last JSON file, and avoid unnecessarily persisting credentials
   in Hosts3D.
5. After successful diagnostics, plan managed switch changes, such as SNMP-SET,
   SSH configuration or API-based mirror-port configuration.

Scope boundaries:

- SNMP-SET, SSH configuration and API-based switch changes are later but explicitly
  desired steps.
- Manufacturer-private MIBs require validation for the device and firmware.
- MAC/IP/hostname correlations are indications, not proof.

## Device-specific work

Keep device-specific research, OID lists, validation steps and concrete
implementation plans in their own files. This Todo record remains the general
planning framework for capture and sensor management.

Current device-specific work:

- `Tools/snmp/README.md`: common SNMP helper documentation, OIDs, JSON contract,
  status model and staging instructions.
- `Tools/snmp/scalance_switches_refresh.py`: serial SNMP refresh runner for all
  enabled switches in `hsd-data/switches.txt`.
- `scalance_xr328_mirror_check.md` in the configured external command directory:
  device-specific SNMP observations for Siemens SCALANCE XR328-4C WG.
- `scalance_xc208g_mirror_check.md` in the configured external command directory:
  device-specific SNMP observations for Siemens SCALANCE XC208G.
- `scalance_xr328_mirror_check.py` in the configured external command directory:
  read-only tool for JSON diagnostics of SCALANCE mirroring state.

## Planning ownership and current checkpoint

This record, maintained in `Todos.md` and its synchronized `Todos.en.md` view,
is the sole source of active Hosts3D planning and status. The capture/sensor
proposals above remain proposals, not evidence of delivered features. New entries
use English. Technical contracts and operating instructions
belong in the maintained sources listed in `docs/README.md`; completed session
notes do not create another planning authority.

Checkpoint: 2026-10-14. Independent agent support, runtime invariants and local
development/topology workflows are maintained in their owning documentation.
The optional refresh now preflights enabled command families and has an inert
unavailable result without changing saved topology. Validation results are
recorded below.

Verified: all seven offline Python refresh tests pass, including missing-provider
preservation and a single enabled command family; both local skill frontmatter
checks pass. The existing project-supported WinLibs compiler built the corrected
Debug x64 application, and the native path/argument test passes. The native launcher
also preflights all enabled families before starting the detached runner, so a
missing selected command reports unavailable instead of claiming refresh started.
Quoted switch types and enabled flags are handled consistently in native and
Python preflight; the corresponding native and offline selected-family fixtures
pass. The two planning views have been reviewed side by side for equivalent tasks,
scope, status, constraints, checkpoint facts and next action.
The standard MSYS2 compiler/debugger packages are absent on this machine; this
verification used the already installed supported alternative, with no tooling
installation. Windows development now discovers existing compatible tools;
MSYS2 is a recommended setup only when no suitable environment exists. Build and
debug checks require only the selected architecture and operation. The resolver
fixtures pass for native/PATH/WinLibs discovery, explicit override rejection and
operation-specific dependencies. The automatic Debug x64 build passes with the
installed WinLibs compiler, and its GDB loads the executable and C++ debug sources.
GUI rendering, interactive breakpoint binding and real device queries were not
exercised by these checks.

### Deferred runtime issues

Status: deferred; no broad redesign is authorized by this list.

- `DrawList()` / `DrawView()` reread backing files each render frame, which is a
  performance concern for larger help/info/list windows.
- Info/export host-table traversal still lacks a complete lock/snapshot model for
  `hstsByIp` / `hstsByPos`; atomic `goHosts` addresses only the control-flag race.
- Legacy `goHosts` busy-wait loops could later use condition-based pause/resume.

### Application launcher and updates

Status: design proposal, not implemented. If approved later, prepare a small
Windows launcher before adding an OSD update action. Its initial scope is Start,
Check for Updates, Help and possibly Open Data Folder; defer tray behavior and
additional styling until useful. Use packaged release archives, preserve
`hsd-data`, stage replacement for restart and include bounded rollback/error
handling. Do not overwrite the running application or introduce another source
checkout/update system. Name, icon, explanatory text and window details remain
design decisions rather than completed work.

### Special-address and identity behavior

Status: design discussion, not implemented. Confirm the current first-match
`netpos.txt` contract before designing precedence; historical `/32` precedence
claims are superseded.

| Case | Candidate behavior |
|---|---|
| `0.0.0.0` | Grey reserved meta-host; no DNS or ordinary host confirmation |
| `255.255.255.255` | Dedicated global broadcast marker |
| Mask-like addresses | Suspicious/meta treatment unless explicit configuration says otherwise |
| Loopback `127.0.0.0/8` | Dedicated marker or zone |
| One MAC with several IPs | Separate hosts, nearby placement or a later relation marker; no automatic merge |
| One IP with several MACs | One IP host with conflict/anomaly status |

Last verified step: independent agent support, optional command preflight and
optional Windows toolchain discovery are implemented with the bounded checks
above. Next action: select the next pending
capture/sensor task or explicitly deferred proposal with the owner.
