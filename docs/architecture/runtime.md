# Hosts3D runtime invariants

This document records maintenance constraints. The [README](../../README.md)
owns operator instructions and file syntax; [Todos.md](../../Todos.md) owns
unfinished work. Historical successful runs do not establish current readiness.

## Runtime ownership and persistence

- Hosts3D owns `hsd-data/settings.ini`, host/layout data, `switches.txt`, recordings
  and rendered topology. Preserve local records when configuration or formats
  change; do not replace them with test fixtures or implicit laboratory defaults.
- `Release/`, `Debug/`, `build/`, `.hosts3d/`, `hsd-data/`, local sensor state,
  recording files and demo outputs are local artifacts and remain ignored by Git.
- Host visibility and dynamic aging use the real settings, including
  `dynamic_host_ttl_seconds=0` for Off. Selected, locked or recently refreshed
  hosts may survive normal dynamic cleanup. A DNS result alone does not confirm
  a probe-only host; confirmation requires observation in both directions.
- `netpos.txt` rules are evaluated top to bottom, first match wins. Exact IP rules
  remain rules and do not create hosts automatically. Older handover claims that
  `/32` always overrides a broader rule or materializes a host are superseded.
- Keep legacy record/layout readers compatible where supported. Invalid older
  layout data is skipped with an actionable warning rather than blocking startup.

## Threads, process ownership and data integrity

- Preserve atomic access to `goHosts`. It does not provide a complete snapshot or
  locking model for the host tables; the remaining limitations are in `Todos.md`.
- Startup must not wait on packet-thread coordination before that thread exists.
  In particular, loading a layout that paused host work must not make an early
  exact-netpos synchronization wait indefinitely.
- Keep bounded text writes in `hostDetails()`, CSV export, `glwin` text fields and
  `hsddata()`; avoid rebuilding long text with unchecked `strcat()`.
- Identify managed `hsen` processes using the bundled executable's full path,
  not just a process name or a stale saved PID. Stop/Exit recover matching local
  sensor processes from the same installation without stopping unrelated sensors.
- Windows sensor inventory maps Npcap interfaces to adapters by GUID and retains
  their friendly name/IPv4 association. Reconcile saved interfaces with `hsen -l`.
- Linux sensor launch tracks the real sensor PID, detaches appropriately and does
  not count zombie processes as active. Preserve `FD_CLOEXEC` on the Hosts3D UDP
  socket so sensor children cannot retain port `10111` after the GUI closes.
- Local sensor setup may use the established one-time `setcap` path before giving
  an exact manual instruction. Successful start/stop remains silent; errors show
  actionable status, and managed shutdown retains its hard-kill fallback.

## Interaction and visual meaning

- OSD, menus, help and keyboard actions expose the same real state. Keep meaningful
  shortcuts and visible mode values, restrained colors and font-aligned hitboxes.
- Consume custom overlay clicks on press and release, including clicks that open
  another dialog, so they never select underlying 3D hosts.
- Packet filters are exclusive across sensor, packet kind and port. New dimensions
  must participate in the same exclusive-filter state; preserve record/replay
  metadata and older recording compatibility.
- Preserve separate meanings for manual persistent grey host lines and automatic
  runtime-only blue TCP links. DNS collision labels apply only to explicit DNS
  suffix grouping. Selected hosts make their shared collision object selected.
- Keep important port labels explicitly heuristic. Selection/export/detail views
  remain compact, and everyday versus advanced menu paths follow the README.
- Synthetic demo actions remain non-blocking, label their traffic as synthetic,
  store timing/state below `hsd-data` and use a selected local sensor identity when
  available. Packets/alerts fade quickly; demo hosts follow normal dynamic aging.
- The continuation's About dialog retains attribution, its GitHub destination and
  a short GPL/no-warranty reminder.

## Optional switch topology

- `settings.ini` owns `scriptcommands_path`; relative paths use the executable
  directory, independently of the launch directory. Changes to this saved path
  need a restart; edits within the selected command directory need no restart.
- `switches.txt` owns the inventory and explicit connection parameters. Only
  enabled rows select command capabilities. No implicit switch or credentials
  are queried when the inventory is empty.
- Missing external commands are an unavailable, inert operation. Host traffic,
  capture/replay and manual/saved topology remain available without them. Never
  substitute demo observations for a live successful query or modify live records
  to make an offline fixture appear available.
- The runner queries enabled switches serially and writes per-device JSON below
  `hsd-data/snmp`. It assembles one temporary topology file and replaces
  `switch-topology.txt` only after the complete selected query set succeeds.
- JSON is the raw diagnostic contract; `switch-topology.txt` is the compact,
  editable rendering contract. Parse a replacement into temporary state and retain
  the last valid visible topology while a query or parse fails.
- Without saved topology, the switch scene may display only its port scaffold.
  Unassigned observed traffic hosts are intentionally excluded from that scene.
- Keep per-device port counts and physical arrangements, separate switch blocks,
  IP/MAC/name identity pairs and world-X mirroring consistent with the README.
  Mirror/status colors and the F9 legend convey the same meanings; do not add
  unreadable rack-spanning mirror-link lines.
- Device commands, protocol notes and their contracts remain in the optional
  external directory. Hosts3D owns invocation, configuration and rendering.
