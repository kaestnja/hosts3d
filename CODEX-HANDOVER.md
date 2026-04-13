# CODEX Handover

This file is a compact project handover for future Codex sessions.
It is not end-user documentation. It exists to preserve current context, working agreements, and the practical state of the continuation work.

## Purpose

Use this file to recover:

- the current technical state of the project
- the conventions already agreed for this repository
- the files that act as authoritative references
- the practical rules for making further changes without regressing recent work

## Current State

- Current working version: `1.18`
- Version source of truth: `src/version.h`
- Current Windows target in daily use: `Release/windows/x86`
- Build toolchains now available on Windows: `MSYS2/MinGW32` and `MSYS2/MinGW64`
- The project is already in productive use; changes should stay pragmatic and low-risk
- `master` is now the active GLFW 3 / `1.18` mainline

## Existing Reference Files

These files already existed before this handover and remain important:

- `README.md`
  - authoritative user-facing documentation
- `RELEASE_NOTES_1.15.md`
  - legacy upstream history up to original `1.15`
- `RELEASE_NOTES_1.16.md`
- `RELEASE_NOTES_1.17.md`
- `RELEASE_NOTES_1.18.md`
- `README-runtime-windows.md`
  - packaging/runtime handoff for Windows release output
- `README-runtime-linux.md`
  - packaging/runtime handoff for Linux release output
- `testing/sim-hsen.ps1`
  - preferred synthetic packet sender for Windows visualization tests
- `testing/sim-hsen.py`
  - small Python 3 variant derived from the historical upstream helper
- `.markdownlint.json`
  - repo-local markdown lint relaxations
- `.vscode/c_cpp_properties.json`
  - VS Code IntelliSense configuration for the current MinGW32 Windows setup

There was no existing Codex-specific handover file before this one.

## Build and Runtime Baseline

### Windows build

- `compile-hosts3d.bat`
- `compile-hsen.bat`

The build scripts now accept:

- `compile-hosts3d.bat [Release|Debug] [x86|x64|arm64] [--no-pause]`
- `compile-hsen.bat [Release|Debug] [x86|x64|arm64] [--no-pause]`

Build safety note:

- Windows build scripts now use separate object directories under `build/windows/<target>/<arch>/<config>/`
- `x86`/`x64` and `Hosts3D`/`hsen` builds can therefore run in parallel without mixing object files
- if old mixed `src/*.o` or `src/*-res.o` artifacts are still around from earlier builds, they can be deleted safely

The current working machine/repo state is:

- `C:\msys64\mingw32\bin\g++.exe`
- `C:\msys64\mingw32\include\GLFW\glfw3.h`
- `C:\msys64\mingw32\lib\libglfw3.a`
- `third_party\wpcap\include`
- `third_party\wpcap\lib\windows\x86`

Windows bootstrap note for future Codex sessions:

- if `C:\msys64` is missing, install MSYS2 first from `https://www.msys2.org/` or via `winget install -e --id MSYS2.MSYS2`
- once MSYS2 exists, a Codex session can usually install/update the needed packages directly with:
  - `C:\msys64\usr\bin\bash -lc "pacman -Syu --noconfirm"`
  - `C:\msys64\usr\bin\bash -lc "pacman -S --needed --noconfirm mingw-w64-i686-gcc mingw-w64-i686-binutils mingw-w64-i686-glfw make"`
  - `C:\msys64\usr\bin\bash -lc "pacman -S --needed --noconfirm mingw-w64-x86_64-gcc mingw-w64-x86_64-binutils mingw-w64-x86_64-glfw make"`
- the interactive MSYS2 installer itself is better launched by the user than assumed by automation

Important x64 note:

- the current machine now has both the MinGW32 and MinGW64 compilers installed
- the verified x64 compiler path is `C:\msys64\mingw64\bin\g++.exe`
- the matching GLFW 3 MSYS2 packages are installed for both `mingw32` and `mingw64`
- `third_party\wpcap\lib\windows\x64` is already populated
- `Release/windows/x64/Hosts3D.exe` and `Release/windows/x64/hsen.exe` now build successfully
- the required code fix was changing the legacy hashtable callback arg type from `long` to pointer-safe `HtArgType` (`intptr_t`) for Windows x64
- `Release/windows/x64/Hosts3D.exe` has been smoke-tested: it starts and creates `hsd-data` in the x64 runtime dir
- `Release/windows/x64/hsen.exe -l` works and lists interfaces
- `package-release-windows.bat Release x64` now produces `Release/dist/hosts3d-1.18-windows-x64.zip`
- `compile-hosts3d.bat` now links `Hosts3D` against GLFW 3 and copies `glfw3.dll` from the selected MSYS2 toolchain into the runtime output

### GLFW 3 migration status

- `Hosts3D` now includes `src/glfw_compat.h`, which pulls in `GLFW/glfw3.h` with `GLU`
- the old GLFW 2 threading API has been removed from the app code and replaced by `std::thread`, `std::mutex`, and `std::condition_variable`
- the main window/event loop now uses `GLFWwindow*`, `glfwCreateWindow`, `glfwSetScrollCallback`, `glfwSetCursorPosCallback`, `glfwSetCharCallback`, `glfwWindowShouldClose`, and `glfwSwapBuffers(window)`
- text entry in `MyGLWin` no longer depends on GLFW 2 ASCII-style key callbacks; printable text now enters via the GLFW 3 character callback
- remaining follow-up risk is mostly runtime behavior verification, not basic compile portability

### macOS helper status

- `compile-mac-hosts3d` is still present as a helper script
- the current `1.18` sources have not been recently verified on a real macOS machine
- future Codex sessions should not present the macOS build path as fully verified unless it has been tested on macOS again
- the script itself now warns and pauses for confirmation before continuing with a manual local build

### Main runtime output

- `Release/windows/x86/Hosts3D.exe`
- `Release/windows/x86/hsen.exe`
- `Release/windows/x86/README-runtime-windows.md`

The Windows build/package flow now keeps the runtime README visible automatically:

- `compile-hosts3d.bat` and `compile-hsen.bat` copy `README-runtime-windows.md` into the runtime output directory
- `package-release-windows.bat` includes both `README-runtime-windows.md` and `README.md` in the staged Windows release ZIP
- `package-release-windows.bat` now defaults to a public ZIP without `wpcap.dll` and `Packet.dll`
- `package-release-linux` creates `Release/dist/hosts3d-<version>-linux-<arch>.tar.gz` plus SHA256 from the already-built Linux runtime
- runtime binaries under `Release/` and `Debug/` are now treated as local build outputs, not as Git-tracked release artifacts
- use `with-npcap` only for private/local test packages when you explicitly want those DLLs carried in the ZIP

### VS Code support

The repository now contains:

- `.vscode/c_cpp_properties.json`

This was added because VS Code IntelliSense was reporting many false missing-include errors even though the project built correctly.

## Major Functional State Already Implemented

These are important because future changes should not silently undo them.

### Settings and persistence

- `settings.ini` is now the human-readable settings format
- old `settings-hsd` binary settings are migrated/imported
- keybindings are stored in `settings.ini`
- `controls.txt` is generated from the current keybindings
- newly written `.hnl` layouts now use a versioned `HN2` format with explicit record sizes instead of raw `host_type` dumps
- older `HN1`/`HNL` layouts are still read when they pass size checks
- obviously incompatible layout files should now be skipped cleanly with a visible warning instead of wedging startup

### Dynamic vs static hosts

- auto-discovered hosts start as `dynamic`
- dynamic hosts age out after the configured inactivity timeout
- locked or currently selected hosts are not removed by cleanup
- hosts become effectively persistent when manually created, edited, named, loaded from layouts, or otherwise promoted to static
- exact `/32` entries in `hsd-data/netpos.txt` are also materialized as known static hosts at startup
- those known `/32` hosts should stay visible and labelled even when `On Activity` is `Highlight Host`
- exact `/32` `netpos.txt` rules now have priority over broader matching nets
- `netpos.txt` now accepts `colour [hold]`, so fixed hosts can be both coloured and non-offset
- saved layouts keep static and locked hosts only

### Local `hsen`

- `Configure Local Sensors (hsen)` exists as a first-class menu entry
- the local `hsen` GUI is aligned across Windows and Linux
- Windows local `hsen` management uses interface discovery from `hsen -l`
- local sensor management state is machine-local
- distributed/manual use via `start-hsenW.bat` still remains available as a separate path

### Menus, help, and controls

- menu items show their real shortcuts in parentheses where applicable
- stateful menu items use visible state markers such as `[X]/[ ]` and `(*)/( )`
- help is now an overlay, not a blocking window
- the OSD has been expanded and rewritten for human readability
- the OSD uses grouped labels instead of opaque legacy abbreviations

### Packet visualization

- packet rendering now uses shapes as well as colors
- control/no-payload traffic uses cubes
- payload traffic uses double/triple cuboids
- discovery/name traffic uses spheres
- ICMP and fragmented traffic use pyramids
- TCP setup/finish and reset are visually distinguished
- ARP is distinguished into request, reply, and gratuitous

### Packet filters

- packet filters are now exclusive
- at any moment the user should see either:
  - all traffic
  - or exactly one active packet filter
- the OSD packet legend is clickable and mirrors the filter tree
- the `Packets` menu mirrors the same filter tree and visible active state

### Packet recording / replay

- packet recordings preserve the newer extended metadata format
- old `.hpt` recordings remain readable
- the bottom-left packet traffic status panel shows:
  - mode
  - elapsed time
  - replay packet time
  - current file label
- new recordings no longer overwrite a single fixed file
- recordings auto-increment to the next free `traffic*.hpt`
- replay opens a list of available `.hpt` recordings from `hsd-data`

## Working Agreements For Future Codex Sessions

These are the most important project-specific rules that emerged during the work.

### General engineering style

- prefer small, pragmatic, low-risk changes
- preserve working behavior unless the user explicitly wants a redesign
- do not make broad stylistic rewrites unless they are asked for
- when in doubt, keep compatibility and user habits intact

### Documentation sync

When behavior changes, also check whether these must be updated:

- `README.md`
- `RELEASE_NOTES_1.18.md` or the next release notes file
- generated help/controls text in `checkControls()` / `controls.txt`
- menu labels and OSD wording

If controls, menus, OSD labels, or packet meanings change, documentation should usually be updated in the same session.

### Release workflow

For future tagged releases, use the package/assets model instead of relying on tracked runtime binaries inside the repo.

Recommended order:

- verify the repo is clean and the version in `src/version.h` matches the intended release
- build Windows runtimes:
  - `compile-hosts3d.bat Release x86`
  - `compile-hsen.bat Release x86`
  - `compile-hosts3d.bat Release x64`
  - `compile-hsen.bat Release x64`
- create Windows release assets:
  - `package-release-windows.bat Release x86`
  - `package-release-windows.bat Release x64`
- on the Raspberry Pi / Linux arm64 build machine:
  - `git pull --ff-only`
  - `./compile-hosts3d Release`
  - `./compile-hsen Release`
  - `./package-release-linux Release arm64`
- tag the release (`v1.xx`) and push the tag
- create the GitHub release and upload the generated assets manually

Expected release assets now include:

- `hosts3d-<version>-windows-x86.zip`
- `hosts3d-<version>-windows-x86-SHA256.txt`
- `hosts3d-<version>-windows-x64.zip`
- `hosts3d-<version>-windows-x64-SHA256.txt`
- `hosts3d-<version>-linux-arm64.tar.gz`
- `hosts3d-<version>-linux-arm64-SHA256.txt`

Important policy:

- runtime binaries under `Release/` and `Debug/` are local build outputs
- release-ready binaries belong in packaged archives/assets, not as tracked Git artifacts
- if packaging behavior changes, update `README.md`, `BUILDINGNOTES.txt`, `README-runtime-windows.md`, `README-runtime-linux.md`, and this handover in the same session

### Mandatory propagation sweep after non-trivial changes

This repository now has an explicit anti-drift rule:

- after any non-trivial change, cleanup, rename, migration, or removal, do not assume the directly edited files are enough
- future Codex sessions must run a deliberate follow-up sweep for affected references, wording, packaging, and maintenance files before calling the task "done"

Why this rule exists:

- local code changes were repeatedly completed while secondary references in docs, handover notes, ignore rules, packaging notes, or legacy support files were only caught later by manual user follow-up
- Codex tends to optimize strongly for the immediate task unless these propagation checks are made explicit

Minimum required sweep areas:

- repo docs:
  - `README.md`
  - `README-runtime-windows.md`
  - `BUILDINGNOTES.txt`
  - `CREDITS.md`
  - relevant `RELEASE_NOTES_*.md`
  - `CODEX-HANDOVER.md`
- build/package/install files:
  - `compile-*.bat`
  - `compile-*`
  - `package-release-windows.bat`
  - `Makefile.am` / `Makefile.in`
  - `configure.ac` / generated `configure`
  - `man/*.1`
  - `third_party/README.md`
- repo hygiene:
  - `.gitignore`
  - `.gitattributes`
  - legacy folders/files that may now be obsolete
  - references to deleted or renamed paths

For renames/removals/migrations, future Codex sessions should explicitly search for:

- old names
- old menu labels
- old feature wording
- old paths/folders
- old dependency names
- old branch/worktree references

Preferred method:

- run targeted `rg` searches for the old/new terms before finalizing
- do not rely only on memory or on the files already opened during the main task
- if a cleanup removes historical/legacy material, also verify whether `.gitignore`, packaging docs, handover notes, and release notes need to be updated

Completion rule:

- for non-trivial repo cleanups or wording migrations, do not report the work as fully finished until this propagation sweep has been done

### Keybinding and UI rules

- menu-only artificial shortcut layers were intentionally removed
- only real commands should have real keybindings
- visible state is preferred over hidden mode/state
- if something is toggleable or mode-based, it should ideally show its active state in the UI

### OSD rules

- the OSD should remain readable first, decorative second
- avoid turning it into a rainbow; use limited, meaningful emphasis
- OSD legend items are part of the interaction model now, not just decoration
- if host-visibility behavior gains exceptions, reflect that explicitly in the OSD instead of leaving `Display Scope` or `On Activity` misleading
- most mode/toggle rows in the top-right OSD are now clickable and should stay consistent with both the visible value and the corresponding keyboard/menu behavior

### Packet filter rules

- no mixed packet filters by default
- if a new filter dimension is added, it must be considered in the exclusive-filter logic
- OSD and menu should stay consistent with each other

### Runtime data rules

The following are machine-local/runtime artifacts and should stay out of version control:

- `hsd-data/settings.ini`
- `hsd-data/controls.txt`
- `hsd-data/0network.hnl` to `4network.hnl`
- `hsd-data/traffic*.hpt`
- temp files in `hsd-data`
- local `hsen` state files

Keep in mind:

- after upgrading an older installation, the first startup may skip an incompatible old `0network.hnl`
- the next normal exit should then rewrite `0network.hnl` in the newer `HN2` format
- `netposExactHostsSync()` must not wait on `goHosts` before the packet thread exists; this caused a real startup hang when `0network.hnl` had already set `goHosts = 0`
- the current guard for that is `packetThreadStarted`, set only after `glfwCreateThread(pktProcess, ...)` succeeds

Do not accidentally reintroduce tracked runtime artifacts.

### Line endings

Repo line ending policy:

- tracked text files use `LF`
- only Windows command scripts `*.bat` and `*.cmd` use `CRLF`
- this is enforced in `.gitattributes`

When editing:

- do not introduce mixed line endings
- if unsure, check with `git ls-files --eol`

### Markdown / editor hygiene

Markdown linting has intentionally been relaxed for this repo.
Current repo-local markdown exceptions are in `.markdownlint.json`.

This was done because:

- the README is dense technical reference material
- some markdownlint rules were producing noise without improving readability

Do not blindly re-tighten markdown rules unless there is a clear benefit.

VS Code IntelliSense support for this project is partially handled by:

- `.vscode/c_cpp_properties.json`

If VS Code shows missing include errors while the project still builds, check editor configuration before touching the code.

### Current UI / runtime rules

Recent behavior decisions that future sessions should preserve unless intentionally changed:

- normal OSD settings rows use a click hitbox aligned to the font baseline band; do not reintroduce the old downward-shifted hit area
- hostname resolving now runs asynchronously in the background and shows a permanent OSD status row
- newly seen hosts and `netpos` exact hosts should each get one automatic hostname resolve attempt
- `probe-only` hosts stay grey and flat until they have been seen both as packet source and destination; DNS resolution alone does not confirm them
- rectangle selection should use the visible projected host bounds, including the flat `probe-only` shape
- if any host inside a collision is selected, the shared collision object should render selected
- DNS collision labels are valid only for collisions created by `Combine by DNS Suffix ...`; do not guess labels for arbitrary collisions
- `Last Packet Form` should use the same semantic labels as the packet filter tree, and `Last Likely Relevant Port` is intentionally heuristic wording
- `HOST INFORMATION` and `SELECTION INFORMATION` should stay as smaller left-aligned windows, not near-fullscreen overlays
- `Selection of Hosts -> Add Selected Hosts To Net Positions` writes one `/32` line per selected host using the host's current coordinates and colour
- the `Configure Local Sensors (hsen)` dialog now shows current adapter IPv4 addresses in a dedicated extra column, uses a wider window, and has separate `Refresh` / `Save` buttons plus a single `Start` / `Stop` button
- local hsen start/stop should stay silent on success and only open an extra dialog on errors; the stop path should fall back to a hard kill if normal termination does not clear all managed `hsen` processes
- on Linux, `Configure Local Sensors (hsen) -> Save` and `Start` should try a one-time automatic `setcap` for the bundled `hsen` binary before falling back to a precise manual command; keep this self-service and avoid reintroducing a “needs Codex/SSH” setup step for normal Pi/Linux users
- on Linux, GUI-managed local `hsen` should launch in a way that preserves the actual running `hsen` PID for state tracking; do not regress to recording only a transient shell/launcher PID, because that breaks reliable `Stop`/`Exit` cleanup
- on Linux, `Stop` and normal `Exit` should also sweep matching bundled local `hsen` processes from the same installation path instead of trusting the state file alone; this intentionally recovers older orphaned local sensor processes left behind by previous buggy runs
- on Linux, GUI-managed local `hsen` should stay detached from the GUI parent process and zombies must not count as active managed sensors; otherwise startup can silently fail and `Stop`/`Exit` can hang on defunct children
- on Linux, the Hosts3D UDP receive socket must stay `FD_CLOEXEC`; otherwise local `hsen` children inherit port `10111`, and later Hosts3D starts can fail to bind the receive socket and silently lose all packet reception
- menu confirm submenus for `Selection -> Delete` and `Net Layout -> Clear Current Layout` were intentionally removed; these actions now trigger immediately
- flattened grouped menus should use a visible fixed text indent for child entries; `View`, `Select Inactive Hosts`, `Net Layout`, and `Anomalies` now use this pattern, and `Net Positions Editor`, `Find Hosts`, `Help`, and `About` were intentionally moved into the top-level main menu
- `Selection of Hosts -> Set Host Colour` is now a direct inline colour strip with clickable coloured boxes, not a separate submenu
- `Selection of Hosts` now starts with a grouped `SELECTION TOOLS` block for `Select All Hosts`, `Invert Current Selection`, `Select All Named Hosts`, `Show Packets for Selection`, `Stop Showing Packets for Selection`, and `Export Selection Details in CSV File As...`
- `Selection of Hosts -> SELECTION TOOLS` also includes `Persistent Host Labels for Selection`, and `Reset` now only covers traffic counters and services
- `Host Lines` is now its own top-level grouped menu for selected-host link actions, automatic link modes, and link deletion
- `Packet Filters -> Sensor Filter` is now also a direct inline strip (`All`, `1`..`9`), not a separate submenu
- `Packet Display On/Off` is now a direct stateful top-level main-menu toggle; it no longer lives inside `Packet Filters`
- `Pause/Resume Packet Animation` is now also a direct stateful top-level main-menu toggle near `Packet Filters`
- `Packet Capture & Replay` is now its own top-level grouped menu for recording, replaying, skipping, stopping, and file open/save
- `Create Host...` now lives directly in the top-level main menu near `Net Positions Editor`
- `Show OSD` / `Hide OSD` is now a direct stateful top-level main-menu toggle near `Find Hosts...`, `Help`, and `About`
- a small set of especially important top-level workflow items uses a subtle tinted menu background (currently `Host Lines`, packet display/animation toggles, `Packet Capture & Replay`, `Net Positions Editor`, `Create Host...`, and the OSD toggle); keep this restrained, not broad
- non-clickable menu group titles use a muted visual style so they stay readable but are clearly distinct from clickable actions
- treat `Selected Host`, `Selection of Hosts`, `Host Labels`, `Packet Filters`, the packet display/animation toggles, `View`, and `Find Hosts...` as the core everyday menu paths
- treat `Host Lines`, `Packet Capture & Replay`, `Net Positions Editor`, `Create Host...`, and `Configure Local Sensors (hsen)` as the advanced paths; if their wording or placement changes, update the README `Menu Orientation` section too
- OSD wording should stay aligned with the modernized menu/help wording, not older shorthand; examples now include `Packet Type Filter`, `On Activity`, `Show Packets for New Hosts`, `Auto Link New Hosts`, and `Show Packet Destination Port`
- the OSD `RUNTIME` section now includes `Packet Capture & Replay` plus `Replay Packet Time` while replay is active; keep the exact replay timestamp visible there
- `netpos.txt` now supports both legacy `pos ...` rules and newer exact `host ...` rules with optional `ip=`, `mac=`, and `fqdn=` identity fields
- `netpos` matching is now strict top-to-bottom first-match-wins; do not reintroduce the older `/32`-beats-broad-net special case
- exact rules with an IP (`pos .../32` or `host ip=...`) are the ones that auto-materialize known hosts at startup
- the former high-risk string-overflow paths in `hostDetails()` and selection CSV export have been replaced with bounded appends / direct field writing; do not reintroduce raw `strcat()`-style assembly for long host text
- `glwin` fixed-size text fields (`CreateWin`, `AddButton`, `AddList`, `AddView`, `AddMenu`, `PutLabel`) now clamp copied text; keep new UI text writes bounded by destination buffer sizes
- `hsddata()` now builds paths with bounded formatting instead of raw `strcpy`/`strcat`
- `goHosts` is now wrapped in an atomic byte state to reduce the old packet-thread/UI-thread data race; preserve atomic access if that control path is touched again

## Known Technical Debt / Deferred

Still intentionally not fully solved:

- `DrawList()` and `DrawView()` still reopen and reread backing files every render frame; this is a known performance hotspot for larger help/info/list windows
- host-table traversals used by info/export flows still do not have a broader lock/snapshot model around `hstsByIp` / `hstsByPos`; the `goHosts` atomic fix only addresses the control flag race, not full container synchronization
- there are still legacy busy-wait loops around `goHosts` (`while (goHosts == 1) usleep(1000)` style); acceptable for now, but a future condition-based pause/resume would be cleaner and cheaper

## Things To Verify Before Bigger Changes

Before or after meaningful feature work, check these areas if relevant:

- build still succeeds with `compile-hosts3d.bat`
- if `hsen` changed, also build with `compile-hsen.bat`
- README/help/menu/OSD wording still match the actual behavior
- packet record/replay still works if packet metadata or filters were touched
- local `hsen` start/stop still works if capture or settings logic changed

## Suggested Workflow For Next Sessions

1. Read this file
2. Read `README.md`
3. Check `src/version.h`
4. Check recent commits with `git log --oneline -n 10`
5. Only then start changing behavior

## Planned / TODO

Current idea under consideration; not implemented yet:

- before any in-OSD update button, first build a small Windows launcher/update EXE
- the launcher/update EXE should own the start/update flow so it can safely swap binaries while `Hosts3D.exe` is not running, then launch the main app
- this is expected to become a real small product surface of its own, not just a hidden helper
- likely needs its own name, icon, short explanatory texts, window, buttons, and help entry
- possible later extras include a tray icon, richer colors/styling, and more polished progress/error UI

Recommended phasing:

- MVP first: small window with `Start`, `Check for Updates`, `Help`, and possibly `Open Data Folder`
- keep tray behavior out of the first version unless there is a strong reason; it adds UX/state complexity quickly
- after the basic launcher works, add update staging, progress feedback, release text, and clearer failure handling
- only after that consider an OSD-level update button that hands off to the launcher/update flow

Architecture notes to preserve:

- prefer packaged GitHub release ZIPs / runtime bundles over raw `git pull` style updates
- keep runtime data in `hsd-data` separate from binaries
- stage replacement files for the next restart instead of trying to overwrite the currently running `Hosts3D.exe`
- treat rollback/backup and user-visible error handling as part of the real update design, not as an afterthought

### Planned special IP / MAC rules

Design discussion only; not implemented yet:

| Case | Candidate rule |
|---|---|
| `0.0.0.0` | single grey meta-host at a reserved position; no DNS resolve; not a normal confirmed host |
| `255.255.255.255` | one dedicated global broadcast marker instead of a normal host |
| mask-like addresses such as `255.255.255.0` | treat as suspicious/meta by default unless an explicit `/32` `netpos` rule says otherwise |
| loopback `127.0.0.0/8` | dedicated loopback marker or loopback zone instead of normal host placement |
| same MAC on multiple IPs | keep separate hosts, prefer nearby placement, maybe add a future relation marker; no auto-merge by default |
| same IP with multiple MACs | keep one IP host but flag a MAC-conflict/anomaly state |

Planned precedence:

- explicit `/32` `netpos` rules should still override future automatic special-case placement behavior

## Open Guidance

This file should be kept compact and practical.
Do not duplicate the entire README here.
Update it when:

- a new design rule is established
- a new subsystem becomes important
- a session introduces non-obvious conventions that future work should preserve
