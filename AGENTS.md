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

## Operation input snapshot

At invocation, resolve the complete effective input—selection, editor values,
configured references and defaults—into an operation-owned immutable value
snapshot. Materialize collections before the first asynchronous or background
handoff and use that snapshot through continuations, retries, progress, results
and persistence. Later selection or value changes belong to the next operation.
Bind cancellation and output to the owners captured for the active operation.

Verify affected behavior by holding the first asynchronous handoff, changing
selection or values, and confirming that the active operation uses the captured
values while the next invocation uses the changed values. Durable domain
entities remain owned by their existing sources; the snapshot is a value copy.

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

Before any repository or external-state mutation, and before scope-sensitive
multi-step work, read `.agents/skills/goal-fidelity/SKILL.md` and establish its
task contract. Then read only the applicable domain skill; a domain skill remains
subordinate to the authorized objective, phases, scope and stop boundary.

- `.agents/skills/hosts3d-development/SKILL.md` for build, debugger, packaging or
  toolchain work.
- `.agents/skills/secure-package-consumption/SKILL.md` for dependency,
source/registry, lockfile or package-manager install/update decisions, not an ordinary build
  with an unchanged locked dependency graph.
- `.agents/skills/hosts3d-switch-topology/SKILL.md` for optional device commands,
  switch configuration, refresh or topology rendering.

Lasting technical knowledge belongs in maintained documentation and unfinished
work in the synchronized planning record. Git retains historical evidence; do
not create another handover or session-memory authority.

## graphify

This project has a knowledge graph at graphify-out/ with god nodes, community structure, and cross-file relationships.

When the user types `/graphify`, use the installed graphify skill or instructions before doing anything else.

Rules:
- When graphify-out/graph.json exists, run `graphify query "<question>"` before your first grep, search or source-file read for a codebase question, and put this rule in every subagent prompt that explores code. Read raw files only for lines the graph or an earlier read already located. Use `graphify path "<A>" "<B>"` for relationships and `graphify explain "<concept>"` for focused concepts. These return a scoped subgraph, usually much smaller than GRAPH_REPORT.md or raw grep output.
- Dirty graphify-out/ files are expected after hooks or incremental updates; dirty graph files are not a reason to skip graphify. Only skip graphify if the task is about stale or incorrect graph output, or the user explicitly says not to use it.
- If graphify-out/wiki/index.md exists, use it for broad navigation instead of raw source browsing.
- Read graphify-out/GRAPH_REPORT.md only for broad architecture review or when query/path/explain do not surface enough context.
- After modifying code, run `graphify update .` to keep the graph current (AST-only, no API cost).
- A committed graphify-out/graph.json is rewritten whole on every build, so a large diff to it in a pull request is expected. Do not flag it as churn or suggest skipping the graph rebuild or its hooks.
