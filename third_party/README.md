# Third-Party Layout (Local, No Global SDK Paths)

This project can be built on Windows using local dependencies in this `third_party` tree.
For the current `1.18` mainline, `Hosts3D.exe` no longer uses a vendored GLFW tree.
It links against the GLFW 3 package installed in the active MSYS2 MinGW toolchain and copies `glfw3.dll` from that toolchain into the runtime output.

## Expected folder layout

```text
third_party/
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

### GLFW 3 (for `Hosts3D.exe`)

Install the matching MSYS2 package for the selected MinGW toolchain:

- `mingw-w64-i686-glfw`
- `mingw-w64-x86_64-glfw`

The Windows `Hosts3D` build script resolves headers, libraries and `glfw3.dll` from the active toolchain root.

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

`compile-hosts3d.bat` also copies `glfw3.dll` from the active MSYS2 MinGW toolchain into the output dir when found.

`compile-hsen.bat` also copies `wpcap.dll` and `Packet.dll` from `third_party` into the output dir when found.

Both Windows build scripts also copy `libwinpthread-1.dll` from the active MinGW `g++` toolchain directory into the output dir when found.

`compile-hosts3d.bat` and `compile-hsen.bat` now accept an explicit arch:
- `compile-hosts3d.bat Release x86`
- `compile-hosts3d.bat Release x64`
- `compile-hsen.bat Release x86`
- `compile-hsen.bat Release x64`

If the requested toolchain or required GLFW 3 / WinPcap files are missing, the scripts stop with a targeted error instead of silently selecting the wrong compiler or dependency set.

For `hsen.exe`, if `third_party/wpcap/bin/windows/<arch>/` does not contain these DLLs, `compile-hsen.bat` falls back to installed Npcap system paths:
- x86: `%SystemRoot%\SysWOW64\Npcap` and `%SystemRoot%\SysWOW64`
- x64: `%SystemRoot%\System32\Npcap` and `%SystemRoot%\System32`

## Runtime note for packet capture

`hsen.exe` can be compiled with local SDK files, but live interface capture on Windows still typically requires Npcap driver/service installed and running.
