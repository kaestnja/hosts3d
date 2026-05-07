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

## OpenSSL input layout

`compile-net-snmp-windows.bat` prepares this layout automatically via vcpkg.
The files are local build inputs and are intentionally ignored by Git:

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

Do not use the Git-SDK/MSYS2 MinGW OpenSSL libraries for this MSVC build. Those
are `.a` / `.dll.a` libraries and belong to the MinGW toolchain, while this
batch builds Net-SNMP with `cl.exe` and `link.exe`.

## vcpkg OpenSSL preparation

The batch finds `vcpkg.exe` in this order:

- `%VCPKG_ROOT%\vcpkg.exe`
- `..\..\microsoft\vcpkg\vcpkg.exe` relative to the Hosts3D repository
- `vcpkg.exe` on `PATH`

It then runs:

```bat
vcpkg install openssl:x64-windows-static openssl:x86-windows-static
```

After vcpkg completes, the batch copies:

```text
<vcpkg>\installed\x64-windows-static\include -> third_party\openssl\windows\x64\include
<vcpkg>\installed\x64-windows-static\lib\libcrypto.lib -> third_party\openssl\windows\x64\lib\libcrypto_static.lib
<vcpkg>\installed\x64-windows-static\lib\libssl.lib    -> third_party\openssl\windows\x64\lib\libssl_static.lib
<vcpkg>\installed\x86-windows-static\include -> third_party\openssl\windows\x86\include
<vcpkg>\installed\x86-windows-static\lib\libcrypto.lib -> third_party\openssl\windows\x86\lib\libcrypto_static.lib
<vcpkg>\installed\x86-windows-static\lib\libssl.lib    -> third_party\openssl\windows\x86\lib\libssl_static.lib
```

The script verifies `opensslv.h`, `libcrypto_static.lib`, and
`libssl_static.lib` for both architectures before starting the Net-SNMP build.

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
