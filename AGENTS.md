# Hosts3D development

Hosts3D is an independently maintained continuation. This repository owns its
agent instructions, development workflows, technical documentation and planning.
Open [docs/README.md](docs/README.md) for the responsibility map. Use
[Todos.md](Todos.md) as the single source of active work and status, with
[Todos.en.md](Todos.en.md) as its synchronized English view; historical
session notes are not evidence of the current machine or runtime state.

Maintain new documentation, comments and operator text in English. Preserve
existing German material; create a German translation only on an explicit request.
Maintain any existing language pair together with equivalent meaning. Describe
this software and its components as tools or applications.

Keep changes small and preserve working behavior. Update affected README/runtime
instructions, generated help, menus, OSD text, build/staging files and references
in the same change. Search renamed or removed identifiers before completion.
Do not commit, push, publish or install unrelated tooling without a user request.

## Optional device commands

For shared device queries, first read the **Shared ScriptCommands** section in
[README.md](README.md#shared-scriptcommands). The existing `settings.ini` owns
`scriptcommands_path`; relative paths resolve against the executable directory.
Read `CONTRACT.md` and `AGENTS.md` in that checkout before changing a shared command.

Maintain common command corrections in the source checkout of the
optional external script directory. Use its existing Git checkout directly;
commit/push only when authorized. Do not introduce package/update management,
another invocation framework or private copies of shared device commands.
Hosts3D owns `hsd-data/switches.txt`, passes explicit method parameters to the
commands, and renders their observations. New device families also need a type
mapping in `Tools/snmp/scalance_switches_refresh.py` and suitable rendering.

External commands are optional: host/traffic views, capture/replay and saved or
manually maintained topology must remain usable without their directory. Missing
selected capabilities produce an inert unavailable result and preserve saved
topology; never report simulated data as a successful live query. Tests use
explicit fixtures. Do not copy external implementations into Hosts3D as a fallback.
Before changing an external command, read its standalone interface contract.
Caller integration records remain outside the command collection; coordinate
affected callers, tests and runtime packages through those records.

Public repository text describes the optional interface and neutral configuration.
Do not add internal project names, repository URLs or workplace-specific paths.
The existing absolute source-path example in README.md is owner-approved.
Keep it as a descriptive example, not an operational default.
Use general runtime examples such as `C:\Tools\Hosts3D` and
`C:\Tools\ExternalScripts`; live configured directories remain owner-managed.
Device manufacturers, model names and required attribution remain legitimate
documentation.

## Runtime and verification

Keep real laboratory settings in local configuration. Preserve configured devices
and access data; real non-secret examples in documentation/tests are allowed.
Never restore a specific laboratory host or implicit credentials as a code default.
On Windows, reuse an existing compatible MinGW toolchain, including WinLibs.
When no suitable development/build environment exists, recommend the documented
MSYS2 setup under `C:\msys64`; its provider and directory are not prerequisites.
Require only the selected operation's dependencies: the target compiler and GLFW
for Hosts3D builds, matching GDB for native debugging, `make` for make-based flows,
and Bash/pacman for MSYS2 operations. An x64 operation must not require x86 tools.
Before changing build or debug configuration, check the selected architecture's
tools with `CheckBuild`, `CheckTools` or `CheckDebug` in
`Tools/Hosts3D-DevTools.ps1`.
Run focused tests with `python -B -m unittest discover -s tests`. Build Windows
with `Tools/Hosts3D-DevTools.ps1 -Task BuildHosts3D -Config Debug -Arch x64`.
The C++ path test is `tests/test_scriptcommands_path.cpp`. Runtime invariants are
maintained in [docs/architecture/runtime.md](docs/architecture/runtime.md).
`.gitattributes` owns line endings; keep runtime outputs and local data ignored.

Read only the relevant local skill:

- `.agents/skills/hosts3d-development/SKILL.md` for build, debugger, packaging or
  toolchain work.
- `.agents/skills/hosts3d-switch-topology/SKILL.md` for optional device commands,
  switch configuration, refresh or topology rendering.

Lasting technical knowledge belongs in maintained documentation and unfinished
work in the synchronized planning record. Git retains historical evidence; do
not create another handover or session-memory authority.
