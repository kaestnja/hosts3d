# Net-SNMP Windows build inputs

`..\..\compile-net-snmp-windows.bat` builds the Net-SNMP command line tools
from a separate Net-SNMP source checkout and copies only the required tools
into `Release\windows\<arch>`.

The batch intentionally keeps all Hosts3D-specific build assumptions here, not
in the Net-SNMP checkout:

- build targets: `snmpget.exe`, `snmpwalk.exe`, `snmpset.exe`
- Visual Studio / `nmake` build
- Net-SNMP static link mode
- static MSVC runtime (`/MT`)
- OpenSSL enabled for SNMPv3 SHA/AES support
- default MIB loading disabled because Hosts3D uses numeric OIDs

## Required OpenSSL layout

Provide MSVC-built OpenSSL headers and libraries below:

```text
third_party\openssl\windows\x64\
  include\openssl\opensslv.h
  lib\libcrypto_static.lib
  lib\libssl_static.lib

third_party\openssl\windows\x86\
  include\openssl\opensslv.h
  lib\libcrypto_static.lib
  lib\libssl_static.lib
```

The batch also accepts `libcrypto.lib` and `libssl.lib`; if those exist but the
`*_static.lib` names are missing, it creates the names Net-SNMP's MSVC pragma
expects.

Do not use the Git-SDK/MSYS2 MinGW OpenSSL libraries for this MSVC build. Those
are `.a` / `.dll.a` libraries and belong to the MinGW toolchain, while this
batch builds Net-SNMP with `cl.exe` and `link.exe`.

## Practical ways to provide OpenSSL

Option A: vcpkg static triplets

```bat
vcpkg install openssl:x64-windows-static openssl:x86-windows-static
```

Then copy each installed tree into the layout above, for example:

```text
<vcpkg>\installed\x64-windows-static\include -> third_party\openssl\windows\x64\include
<vcpkg>\installed\x64-windows-static\lib     -> third_party\openssl\windows\x64\lib
<vcpkg>\installed\x86-windows-static\include -> third_party\openssl\windows\x86\include
<vcpkg>\installed\x86-windows-static\lib     -> third_party\openssl\windows\x86\lib
```

Option B: prebuilt MSVC OpenSSL

Use a package that provides Visual Studio `.lib` files for both x64 and x86,
preferably static libraries built with the static runtime. Place its `include`
and static `lib` directories into the same layout.

## Net-SNMP source changes

No permanent Net-SNMP source patch should be necessary. The batch generates
Net-SNMP's Win32 Makefiles, patches only those generated build files for `/MT`,
temporarily patches the generated `win32\net-snmp\net-snmp-config.h` for this
build, and restores the original header afterwards.

## Verified local build

Verified on 2026-05-07 with:

- vcpkg OpenSSL `3.6.2`
- Net-SNMP `5.10.pre2` checkout
- Visual Studio 2022 Professional MSVC tools

The command:

```bat
compile-net-snmp-windows.bat C:\Users\kaestnja\source\repos\github.com\net-snmp\net-snmp
```

created:

```text
Release\windows\x64\snmpget.exe
Release\windows\x64\snmpwalk.exe
Release\windows\x64\snmpset.exe
Release\windows\x86\snmpget.exe
Release\windows\x86\snmpwalk.exe
Release\windows\x86\snmpset.exe
```

`snmpget -V`, `snmpwalk -V`, and `snmpset -V` reported `NET-SNMP version: 5.10.pre2` for both architectures.

`dumpbin /dependents` showed only Windows system DLLs:

```text
ADVAPI32.dll
WS2_32.dll
KERNEL32.dll
USER32.dll
CRYPT32.dll
```

No OpenSSL DLLs or MSVC runtime DLLs were required by the generated tools.
