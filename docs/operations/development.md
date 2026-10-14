# Hosts3D development and release operations

The [README](../../README.md#build) contains platform build instructions.
`src/version.h` owns the version; machine-local tool versions and previous test
results must be rechecked when relevant instead of copied from session notes.

## Windows development

Reuse an existing compatible MinGW toolchain, including native WinLibs. If no
suitable development/build environment exists, recommend the MSYS2 installation
in the [Windows build instructions](../../README.md#windows-11-build). Its default
directory `C:\msys64` and provider are optional, not a project requirement.

Dependencies follow the selected operation and architecture: Hosts3D compilation
needs a compatible target compiler and GLFW 3; hsen needs its capture SDK inputs;
native debugging additionally needs matching GDB. `make` is needed for make-based
flows, and Bash/pacman only for MSYS2 operations. None of these tools is a
prerequisite for running an already built runtime. An x64 operation does not
require an x86 toolchain or debugger.

Prefer x64; use x86 as the primary runtime only when a concrete x64 problem is
observed and record that limitation. A normal Windows release covers both when
the corresponding toolchains are available; a focused change uses a targeted build.

PowerShell controls the local helper. Use MSYS2/Bash for MSYS2-native operations.
Batch entrypoints remain supported; the helper handles their quoting boundaries.
Do not repeat package-manager work while an earlier process is still running.

```powershell
pwsh -NoProfile -File Tools/Hosts3D-DevTools.ps1 -Task CheckBuild -Arch x64
pwsh -NoProfile -File Tools/Hosts3D-DevTools.ps1 -Task BuildHosts3D -Config Debug -Arch x64
pwsh -NoProfile -File Tools/Hosts3D-DevTools.ps1 -Task BuildHsen -Config Debug -Arch x64
```

The examples use `pwsh`; Windows PowerShell 5.1 can also run this helper by
replacing `pwsh` with `powershell.exe`. A separate PowerShell 7 installation is
optional for these native build and debug tasks.

`CheckBuild` checks the selected compiler and GLFW inputs. `CheckTools`,
`CheckDebug` and `CheckAll` also apply only to the selected `-Arch`; unavailable
x86 tools do not block x64. `CheckDebug -Arch x64` checks native debugging;
`-Program` optionally selects a different executable.

Automatic selection normally needs no override. For an exceptional installation,
the helper's `-ToolchainRoot` selects a prefix containing `bin/g++.exe`, and
`-Msys2Root` selects the MSYS2 installation prefix. An explicit toolchain override
with a missing compiler or wrong architecture fails instead of silently choosing
another compiler. Batch entrypoints accept equivalent process-local variables
`HOSTS3D_TOOLCHAIN_ROOT` and `HOSTS3D_MSYS2_ROOT`. These options do not require
persistent operating-system environment changes.

Objects are separated below `build/windows/<target>/<arch>/<config>`; never mix
architecture or target objects. GLFW 3 compatibility lives in `src/glfw_compat.h`;
retain pointer-safe `HtArgType` and standard C++ thread synchronization. Build
success alone does not verify scene, input or capture behavior.

VS Code configuration belongs to `.vscode/launch.json`, `tasks.json` and
`c_cpp_properties.json`. `Debug Hosts3D x64` is the normal project debug target;
its task builds and stages a complete Debug runtime, including refresh/demo tools.
The single-target helper is useful for compilation but does not replace that
complete staging step. Debug uses `-g -O0 -DDEBUG`; Release uses `-O2`.

The portable editor configuration finds `g++.exe` and GDB on the VS Code process
PATH and derives compiler include directories from the selected compiler. If
compatible automatically detected tools are not on that PATH, start VS Code from
a PowerShell session with the selected toolchain's bin directory:

```powershell
. .\Tools\Hosts3D-DevTools.ps1
$tools = Get-Hosts3DToolchain -Architecture x64
$env:Path = $tools.BinDirectory + ';' + $env:Path
code .
```

An already running VS Code process may retain its earlier environment. If that
causes tool resolution to fail, close it and start it from the prepared session,
or use local explicit `compilerPath`/`miDebuggerPath` overrides. Select the matching
architecture for the compiler, debugger and launch entry.

For debugger trouble use `CheckDebug` or `GdbFile` in `Hosts3D-DevTools.ps1`.
Useful scene breakpoints are `displayGL()`, `sceneViewToggle()` and
`switchTopologySceneDraw()`. Check editor configuration before changing working
source merely to silence missing-include warnings.

When VS Code setup is requested, install only missing relevant extensions and
merge settings rather than replacing the user's JSON/JSONC. C/C++ uses the project
launch entries; Python/PowerShell single-file configurations use the active file
and selected interpreter. Verify actual breakpoint binding before claiming setup
works. Treat batch execution as run-only unless a real debugger was verified.

Optional clang-tidy analysis is `-Task RunClangTidySarif`; output stays in
`build/sarif/clang-tidy.sarif`. A historical finding count is not a release gate.

## Verification and propagation

- Run focused Python tests with `python -B -m unittest discover -s tests`.
- Compile/run `tests/test_scriptcommands_path.cpp` with the selected compiler when
  path resolution or process quoting changes.
- Run `tests/Test-DevelopmentToolchain.ps1` when tool resolution or operation-specific
  dependency checks change.
- For behavior changes, review the affected README/runtime notes, help/controls,
  menus and OSD together. Exercise capture/replay or local sensor lifecycle only
  when those paths changed.
- For rename/removal/staging changes, search affected identifiers in documentation,
  build/package scripts, ignore rules, editor tasks and manpages. Do not treat
  historical evidence as an active caller.
- `.gitattributes` is the line-ending contract, including its PowerShell exception.
  Before delivery, use the local text scanner and preview any required repair:

```powershell
pwsh -NoProfile -File Tools/Find-InvalidTextBytes.ps1 -Root . -TrackedOnly
pwsh -NoProfile -File Tools/Repair-InvalidTextBytes.ps1 -Root . -TrackedOnly -DryRun
```

New text is ASCII by default, including plain punctuation. The scanner allows
existing German umlauts unless its stricter switch is enabled; do not broaden
Unicode exceptions or tighten Markdown rules without a concrete requirement.

## Releases and runtime staging

`Tools/Stage-RuntimePayload.ps1` is the single Windows runtime payload staging
step. `compile-all-windows.bat` builds x64/x86 and packages default ZIPs;
`package-all-windows.bat` repackages already built runtimes. Debug packages use a
distinct suffix. Keep flat README/demo files beside the binaries and the refresh
runner below `Tools/snmp`. Shared device implementations stay external.

Default Windows packages omit Npcap DLLs; use `with-npcap` only for the established
explicit private/local packaging choice. Debug staging may use Release Net-SNMP
tools when those optional binaries were not built in Debug. Linux archives use
`package-release-linux` and include their SHA256 companion. macOS helper presence
is not proof that the current sources were verified on macOS.

For a requested release, check the version and exact intended source state, build
and verify the required architectures, and create archive/hash assets. On an
available Linux arm64 build machine, use the existing Linux build/package flow.
Tag/push/upload only when requested. Packaged assets carry binaries; do not commit
runtime folders or create parallel source/backup trees. Update affected build
notes and runtime documentation together if package behavior changes.
