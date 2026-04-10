# Third-Party Layout (Local, No Global SDK Paths)

This project can be built on Windows using local dependencies in this `third_party` tree.

## Expected folder layout

```text
third_party/
  glfw2/
    include/GL/glfw.h
    lib/windows/x86/libglfw.a        (or libglfwdll.a)
    lib/windows/x64/libglfw.a        (or libglfwdll.a)
    bin/windows/x86/glfw.dll         (optional but recommended)
    bin/windows/x64/glfw.dll         (optional but recommended)
  wpcap/
    include/pcap.h                   (and related headers from SDK include folder)
    lib/windows/x86/libwpcap.a       (or wpcap.lib)
    lib/windows/x64/libwpcap.a       (or wpcap.lib)
    bin/windows/x86/wpcap.dll        (optional)
    bin/windows/x86/Packet.dll       (optional)
    bin/windows/x64/wpcap.dll        (optional)
    bin/windows/x64/Packet.dll       (optional)
```

## What to search/copy from old folders

### GLFW 2.x (for `Hosts3D.exe`)

Search for:
- `glfw.h` (must be GLFW 2 header under `GL\glfw.h`)
- `libglfw.a` or `libglfwdll.a` (MinGW import/static lib)
- `glfw.dll` (runtime DLL)

Copy to:
- `glfw.h` -> `third_party/glfw2/include/GL/glfw.h`
- `libglfw.a` or `libglfwdll.a` -> `third_party/glfw2/lib/windows/<x86|x64>/`
- `glfw.dll` -> `third_party/glfw2/bin/windows/<x86|x64>/`

For your current MinGW32 build, use `x86`.

Current repository state:
- `third_party/glfw2/lib/windows/x86/` is populated
- `third_party/glfw2/bin/windows/x86/` is populated
- `third_party/glfw2/lib/windows/x64/` is populated
- `third_party/glfw2/bin/windows/x64/` is populated

Windows x64 builds still additionally require an installed x64 MinGW toolchain.

### WinPcap/Npcap SDK files (for `hsen.exe`)

Search for:
- `pcap.h` and sibling headers from the SDK `Include` folder
- `libwpcap.a` (preferred for MinGW) or `wpcap.lib`
- optional runtime DLLs: `wpcap.dll`, `Packet.dll`

Copy to:
- SDK include folder contents -> `third_party/wpcap/include/`
- `libwpcap.a` or `wpcap.lib` -> `third_party/wpcap/lib/windows/<x86|x64>/`
- optional DLLs -> `third_party/wpcap/bin/windows/<x86|x64>/`

## Build output

Windows scripts place binaries in:
- `Release/windows/<arch>/`
- `Debug/windows/<arch>/`

`compile-hosts3d.bat` also copies `glfw.dll` from `third_party` into the output dir when found.

`compile-hsen.bat` also copies `wpcap.dll` and `Packet.dll` from `third_party` into the output dir when found.

Both Windows build scripts also copy `libwinpthread-1.dll` from the active MinGW `g++` toolchain directory into the output dir when found.

`compile-hosts3d.bat` and `compile-hsen.bat` now accept an explicit arch:
- `compile-hosts3d.bat Release x86`
- `compile-hosts3d.bat Release x64`
- `compile-hsen.bat Release x86`
- `compile-hsen.bat Release x64`

If the requested toolchain or third-party files are missing, the scripts now stop with a targeted error instead of silently selecting the wrong compiler.

For `hsen.exe`, if `third_party/wpcap/bin/windows/<arch>/` does not contain these DLLs, `compile-hsen.bat` falls back to installed Npcap system paths:
- x86: `%SystemRoot%\SysWOW64\Npcap` and `%SystemRoot%\SysWOW64`
- x64: `%SystemRoot%\System32\Npcap` and `%SystemRoot%\System32`

## Runtime note for packet capture

`hsen.exe` can be compiled with local SDK files, but live interface capture on Windows still typically requires Npcap driver/service installed and running.
