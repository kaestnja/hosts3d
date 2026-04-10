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

- Current working version: `1.17`
- Version source of truth: `src/version.h`
- Current Windows target in daily use: `Release/windows/x86`
- Build toolchains now available on Windows: `MSYS2/MinGW32` and `MSYS2/MinGW64`
- The project is already in productive use; changes should stay pragmatic and low-risk

## Existing Reference Files

These files already existed before this handover and remain important:

- `README.md`
  - authoritative user-facing documentation
- `CHANGELOG.md`
  - continuation-repository history
- `ChangeLog`
  - legacy upstream history up to original `1.15`
- `RELEASE_NOTES_1.16.md`
- `RELEASE_NOTES_1.17.md`
- `README-runtime-windows.md`
  - packaging/runtime handoff for Windows release output
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
- `third_party\glfw2\include`
- `third_party\glfw2\lib\windows\x86`
- `third_party\wpcap\include`
- `third_party\wpcap\lib\windows\x86`

Important x64 note:

- the current machine now has both the MinGW32 and MinGW64 compilers installed
- the verified x64 compiler path is `C:\msys64\mingw64\bin\g++.exe`
- `third_party\glfw2\lib\windows\x64` and `third_party\glfw2\bin\windows\x64` are now populated from the official `glfw-2.7.9.bin.WIN64.zip` SourceForge package
- `third_party\wpcap\lib\windows\x64` is already populated
- `Release/windows/x64/Hosts3D.exe` and `Release/windows/x64/hsen.exe` now build successfully
- the required code fix was changing the legacy hashtable callback arg type from `long` to pointer-safe `HtArgType` (`intptr_t`) for Windows x64
- `Release/windows/x64/Hosts3D.exe` has been smoke-tested: it starts and creates `hsd-data` in the x64 runtime dir
- `Release/windows/x64/hsen.exe -l` works and lists interfaces
- `package-release-windows.bat Release x64` now produces `Release/dist/hosts3d-1.17-windows-x64.zip`

### Main runtime output

- `Release/windows/x86/Hosts3D.exe`
- `Release/windows/x86/hsen.exe`
- `Release/windows/x86/README-runtime-windows.md`

The Windows build/package flow now keeps the runtime README visible automatically:

- `compile-hosts3d.bat` and `compile-hsen.bat` copy `README-runtime-windows.md` into the runtime output directory
- `package-release-windows.bat` includes both `README-runtime-windows.md` and `README.md` in the staged Windows release ZIP
- `package-release-windows.bat` now defaults to a public ZIP without `wpcap.dll` and `Packet.dll`
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
- those known `/32` hosts should stay visible and labelled even when `On-Active Action` is `Show Host`
- exact `/32` `netpos.txt` rules now have priority over broader matching nets
- `netpos.txt` now accepts `colour [hold]`, so fixed hosts can be both coloured and non-offset
- saved layouts keep static and locked hosts only

### Local `hsen`

- `Local hsen` exists as a first-class menu entry
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
- `RELEASE_NOTES_1.17.md` or the next release notes file
- generated help/controls text in `checkControls()` / `controls.txt`
- menu labels and OSD wording

If controls, menus, OSD labels, or packet meanings change, documentation should usually be updated in the same session.

### Keybinding and UI rules

- menu-only artificial shortcut layers were intentionally removed
- only real commands should have real keybindings
- visible state is preferred over hidden mode/state
- if something is toggleable or mode-based, it should ideally show its active state in the UI

### OSD rules

- the OSD should remain readable first, decorative second
- avoid turning it into a rainbow; use limited, meaningful emphasis
- OSD legend items are part of the interaction model now, not just decoration
- if host-visibility behavior gains exceptions, reflect that explicitly in the OSD instead of leaving `Display Scope` or `On-Active Action` misleading
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

## Open Guidance

This file should be kept compact and practical.
Do not duplicate the entire README here.
Update it when:

- a new design rule is established
- a new subsystem becomes important
- a session introduces non-obvious conventions that future work should preserve
