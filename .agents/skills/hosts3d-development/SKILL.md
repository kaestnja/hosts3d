---
name: hosts3d-development
description: Build, debug or package Hosts3D and hsen, or repair their local toolchain and runtime staging. Use for development operations, not switch-protocol changes alone.
---

# Hosts3D development operations

Read [development operations](../../../docs/operations/development.md) for the
affected build, debug or release path. Local `Tools/Hosts3D-DevTools.ps1` handles
Windows toolchain and MSYS2 boundaries; no sibling project supplies required tooling.

Reuse an existing compatible MinGW toolchain, including WinLibs. Recommend the
documented MSYS2 setup only when no suitable development/build environment exists;
neither MSYS2 nor `C:\msys64` is mandatory. Before changing build/debug
configuration, run `CheckBuild`, `CheckTools` or `CheckDebug` for the selected
architecture. `CheckAll` also applies only to that architecture.
Require compiler/GLFW for Hosts3D compilation, matching GDB only for debugging,
`make` only for make-based flows, and Bash/pacman only for MSYS2 operations.
Prefer the existing x64 target; an x64 operation must not require x86 tools.
A release covers available x64/x86 targets; a focused fix uses relevant
compilation and runtime checks.
VS Code project debugging uses its complete build/staging task, not a standalone
active `.cpp` file. Preserve user JSONC settings when repairing editor setup.
Use the conditional process-PATH setup in development operations if the selected
compiler/debugger are not visible to VS Code; do not mandate machine-wide PATH
changes or restore hardcoded provider directories. An explicit `-ToolchainRoot`
override must fail for a missing or wrong-architecture compiler.

Use [runtime invariants](../../../docs/architecture/runtime.md) when changing
threads, sensor ownership, record/layout readers or interaction. Update affected
README, help, editor tasks and package/staging references together. Build success
does not prove runtime behavior or capture readiness.

Run focused existing tests and the relevant build once. Check text with the local
scanner before delivery. Record unfinished work, attempted step, verified outcome
and next action in [Todos.md](../../../Todos.md); no session-handover maintenance.
Tagging, pushing and release publication require a user request.

For changes to tool resolution or feature-specific dependency checks, use
`tests/Test-DevelopmentToolchain.ps1`; do not repeat native builds for guidance-only
changes.
