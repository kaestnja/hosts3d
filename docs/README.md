# Hosts3D documentation responsibilities

Each maintained source has one responsibility:

| Subject | Authoritative source |
|---|---|
| Agent rules and workflow routing | [AGENTS.md](../AGENTS.md) |
| Active work, deferred proposals and checkpoints | [Todos.md](../Todos.md), synchronized [English view](../Todos.en.md) of the same status |
| User behavior, controls and data formats | [README.md](../README.md) |
| Windows and Linux runtime instructions | [Windows](../README-runtime-windows.md), [Linux](../README-runtime-linux.md) |
| Runtime invariants and ownership | [Runtime architecture](architecture/runtime.md) |
| Development, debugging and releases | [Development operations](operations/development.md) |
| Optional device command integration | [Switch refresh](../Tools/snmp/README.md) |
| Synthetic traffic and demos | [Testing instructions](../testing/README.md) |
| Version identifier | `src/version.h` |
| Published behavior history and attribution | [Release notes](../RELEASE_NOTES.md), [credits](../CREDITS.md) |

Local `.agents/skills` describe the reusable development and switch-topology
workflows. They consume the above sources rather than maintaining another status.
No other project is required to supply agent rules, planning or build helpers.
An optionally configured command directory remains a software dependency for
live device queries; it is not a prerequisite for ordinary Hosts3D operation.

Git retains historical evidence. Prior machine versions, completed sessions, old
defaults and proposals are not current authority. Lasting knowledge belongs in
the maintained sources above, and only unfinished work belongs in the planning
record. Do not create a replacement handover or session-memory archive.
