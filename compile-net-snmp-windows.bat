@echo off
rem Build the minimal Net-SNMP Windows tools used by Hosts3D helpers.
rem Usage: compile-net-snmp-windows.bat C:\path\to\net-snmp
rem
rem The build intentionally uses Net-SNMP's static MSVC link mode, static CRT,
rem and an MSVC OpenSSL tree from third_party. It copies only the CLI tools
rem beside the existing Hosts3D/hsen runtime binaries.
setlocal EnableExtensions
cd /d "%~dp0"

if "%~1"=="" goto :usage
if not "%~2"=="" goto :usage

set "NETSNMP_SRC=%~1"
for %%i in ("%NETSNMP_SRC%") do set "NETSNMP_SRC=%%~fi"

if not exist "%NETSNMP_SRC%\win32\Configure" (
  echo Missing "%NETSNMP_SRC%\win32\Configure"
  echo Pass the root of a Net-SNMP source checkout.
  goto :fail
)
if not exist "%NETSNMP_SRC%\configure" (
  echo Missing "%NETSNMP_SRC%\configure"
  echo Pass the root of a Net-SNMP source checkout.
  goto :fail
)

set "PERL_EXE="
for %%p in ("C:\git-sdk-64\usr\bin\perl.exe" "C:\msys64\usr\bin\perl.exe" "C:\Strawberry\perl\bin\perl.exe" "C:\Perl64\bin\perl.exe") do (
  if not defined PERL_EXE if exist "%%~p" "%%~p" -MExtUtils::Embed -e "exit 0" >NUL 2>NUL && set "PERL_EXE=%%~p"
)
if not defined PERL_EXE (
  for /f "delims=" %%i in ('where perl.exe 2^>NUL') do (
    if not defined PERL_EXE "%%i" -MExtUtils::Embed -e "exit 0" >NUL 2>NUL && set "PERL_EXE=%%i"
  )
)
if not defined PERL_EXE (
  echo Missing a Perl with ExtUtils::Embed.
  echo Known working candidates on this machine are usually:
  echo   C:\git-sdk-64\usr\bin\perl.exe
  echo   C:\msys64\usr\bin\perl.exe
  goto :fail
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSROOT="
if exist "%VSWHERE%" (
  for /f "usebackq tokens=* delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    if not defined VSROOT if exist "%%i\VC\Auxiliary\Build" set "VSROOT=%%i"
  )
)
if not defined VSROOT if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build" set "VSROOT=C:\Program Files\Microsoft Visual Studio\2022\Professional"
if not defined VSROOT (
  echo Missing Visual Studio C++ build environment.
  echo Install Visual Studio with the MSVC x86/x64 tools.
  goto :fail
)
set "VCVARS_X64=%VSROOT%\VC\Auxiliary\Build\vcvars64.bat"
set "VCVARS_X86=%VSROOT%\VC\Auxiliary\Build\vcvarsamd64_x86.bat"
if not exist "%VCVARS_X64%" (
  echo Missing "%VCVARS_X64%"
  goto :fail
)
if not exist "%VCVARS_X86%" (
  echo Missing "%VCVARS_X86%"
  goto :fail
)

set "HOST_ARCH=x64"
set "CONFIG=Release"
set "SNMP_TARGETS=snmpget snmpwalk snmpset"
set "THIRD_PARTY_DIR=%~dp0third_party"
set "OPENSSL_BASE=%THIRD_PARTY_DIR%\openssl\windows"

echo Net-SNMP source: "%NETSNMP_SRC%"
echo Perl: "%PERL_EXE%"
echo Visual Studio: "%VSROOT%"
echo OpenSSL base: "%OPENSSL_BASE%"
echo.

echo === Net-SNMP %CONFIG% x64 ===
call :build_arch x64
if errorlevel 1 goto :fail

echo === Net-SNMP %CONFIG% x86 ===
call :build_arch x86
if errorlevel 1 goto :fail

echo.
echo Net-SNMP tools refreshed successfully.
exit /b 0

:build_arch
set "ARCH=%~1"
set "VCVARS=%VCVARS_X64%"
if /I "%ARCH%"=="x86" set "VCVARS=%VCVARS_X86%"

set "RUNDIR=%CONFIG%\windows\%ARCH%"
if not exist "%RUNDIR%" mkdir "%RUNDIR%"

call :find_openssl %ARCH%
if errorlevel 1 exit /b 1

call "%VCVARS%" >NUL
if errorlevel 1 (
  echo Failed to initialize Visual Studio for %ARCH%.
  exit /b 1
)

where cl.exe >NUL 2>NUL
if errorlevel 1 (
  echo cl.exe was not found after Visual Studio initialization.
  exit /b 1
)
where nmake.exe >NUL 2>NUL
if errorlevel 1 (
  echo nmake.exe was not found after Visual Studio initialization.
  exit /b 1
)

pushd "%NETSNMP_SRC%\win32"
if errorlevel 1 exit /b 1

call :clean_net_snmp_arch_outputs

set "CONFIG_HEADER_BACKUP=%TEMP%\net-snmp-config-%RANDOM%-%ARCH%.h"
set "CONFIG_HEADER_RESTORE="
if exist "net-snmp\net-snmp-config.h" (
  copy /Y "net-snmp\net-snmp-config.h" "%CONFIG_HEADER_BACKUP%" >NUL
  if not errorlevel 1 set "CONFIG_HEADER_RESTORE=1"
)

set "NO_EXTERNAL_DEPS=1"
set "SNMPCONFPATH=t"
set "MIBDIRS=%NETSNMP_SRC%\mibs"

"%PERL_EXE%" Configure --config=release --linktype=static --prefix="c:/net-snmp-portable" --with-sdk --with-ipv6 --with-ssl --with-sslincdir="%OPENSSL_INC%" --with-ssllibdir="%OPENSSL_LIB%"
if errorlevel 1 (
  popd
  echo Net-SNMP Configure failed for %ARCH%.
  exit /b 1
)

for %%m in ("Makefile" "libsnmp\Makefile" "snmpget\Makefile" "snmpwalk\Makefile" "snmpset\Makefile") do (
  "%PERL_EXE%" -0pi -e "s!/MD(\s+/D NDEBUG /O2)!/MT$1!g" "%%~m"
  if errorlevel 1 (
    popd
    echo Failed to switch Net-SNMP %%~m to the static C runtime for %ARCH%.
    exit /b 1
  )
)
for %%m in ("snmpget\Makefile" "snmpwalk\Makefile" "snmpset\Makefile") do (
  "%PERL_EXE%" -0pi -e "s!^(LDFLAGS=.*)$!$1 crypt32.lib bcrypt.lib gdi32.lib!m" "%%~m"
  if errorlevel 1 (
    popd
    echo Failed to add OpenSSL system libraries to Net-SNMP %%~m for %ARCH%.
    exit /b 1
  )
)
"%PERL_EXE%" -0pi -e "s!^#define NETSNMP_DEFAULT_MIBS .*$!#define NETSNMP_DEFAULT_MIBS \"\"!mg; s!^#define NETSNMP_DEFAULT_MIBDIRS .*$!#define NETSNMP_DEFAULT_MIBDIRS \"\"!mg" "net-snmp\net-snmp-config.h"
if errorlevel 1 (
  popd
  echo Failed to disable Net-SNMP default MIB loading for %ARCH%.
  exit /b 1
)

nmake /nologo libsnmp
if errorlevel 1 (
  popd
  echo Net-SNMP libsnmp build failed for %ARCH%.
  exit /b 1
)

for %%t in (%SNMP_TARGETS%) do (
  nmake /nologo %%t
  if errorlevel 1 (
    popd
    echo Net-SNMP %%t build failed for %ARCH%.
    exit /b 1
  )
)

if defined CONFIG_HEADER_RESTORE copy /Y "%CONFIG_HEADER_BACKUP%" "net-snmp\net-snmp-config.h" >NUL
if exist "%CONFIG_HEADER_BACKUP%" del /Q "%CONFIG_HEADER_BACKUP%" >NUL 2>NUL

popd

for %%t in (%SNMP_TARGETS%) do (
  if not exist "%NETSNMP_SRC%\win32\bin\release\%%t.exe" (
    echo Missing built tool "%NETSNMP_SRC%\win32\bin\release\%%t.exe"
    exit /b 1
  )
  copy /Y "%NETSNMP_SRC%\win32\bin\release\%%t.exe" "%RUNDIR%\%%t.exe" >NUL
)

echo Copied Net-SNMP tools to "%RUNDIR%"
exit /b 0

:clean_net_snmp_arch_outputs
for %%d in ("libsnmp\release" "snmpget\release" "snmpwalk\release" "snmpset\release") do (
  if not exist "%%~d" mkdir "%%~d" >NUL 2>NUL
  for %%e in (obj sbr pdb idb ilk res) do if exist "%%~d\*.%%e" del /Q "%%~d\*.%%e" >NUL 2>NUL
)
if exist "lib\release\netsnmp.lib" del /Q "lib\release\netsnmp.lib" >NUL 2>NUL
if exist "bin\release\snmpget.exe" del /Q "bin\release\snmpget.exe" >NUL 2>NUL
if exist "bin\release\snmpwalk.exe" del /Q "bin\release\snmpwalk.exe" >NUL 2>NUL
if exist "bin\release\snmpset.exe" del /Q "bin\release\snmpset.exe" >NUL 2>NUL
exit /b 0

:find_openssl
set "OPENSSL_ARCH=%~1"
set "OPENSSL_ROOT="
for %%o in ("%OPENSSL_BASE%\%OPENSSL_ARCH%" "%OPENSSL_BASE%\%OPENSSL_ARCH%-static" "%OPENSSL_BASE%\%OPENSSL_ARCH%-mt") do (
  if not defined OPENSSL_ROOT if exist "%%~o\include\openssl\opensslv.h" set "OPENSSL_ROOT=%%~o"
)
if not defined OPENSSL_ROOT (
  echo Missing MSVC OpenSSL for %OPENSSL_ARCH%.
  echo Expected one of:
  echo   "%OPENSSL_BASE%\%OPENSSL_ARCH%\include\openssl\opensslv.h"
  echo   "%OPENSSL_BASE%\%OPENSSL_ARCH%-static\include\openssl\opensslv.h"
  echo See "third_party\net-snmp\README.md" for the required layout.
  exit /b 1
)
set "OPENSSL_INC=%OPENSSL_ROOT%\include"
set "OPENSSL_LIB="
for %%l in ("%OPENSSL_ROOT%\lib\VC\static" "%OPENSSL_ROOT%\lib\VC" "%OPENSSL_ROOT%\lib" "%OPENSSL_ROOT%\lib64" "%OPENSSL_ROOT%\lib32") do (
  if not defined OPENSSL_LIB if exist "%%~l\libcrypto_static.lib" if exist "%%~l\libssl_static.lib" set "OPENSSL_LIB=%%~l"
  if not defined OPENSSL_LIB if exist "%%~l\libcrypto.lib" if exist "%%~l\libssl.lib" set "OPENSSL_LIB=%%~l"
)
if not defined OPENSSL_LIB (
  echo Missing MSVC OpenSSL libraries for %OPENSSL_ARCH% below "%OPENSSL_ROOT%".
  echo Need libcrypto_static.lib/libssl_static.lib, or libcrypto.lib/libssl.lib.
  exit /b 1
)
if not exist "%OPENSSL_LIB%\libcrypto_static.lib" if exist "%OPENSSL_LIB%\libcrypto.lib" copy /Y "%OPENSSL_LIB%\libcrypto.lib" "%OPENSSL_LIB%\libcrypto_static.lib" >NUL
if not exist "%OPENSSL_LIB%\libssl_static.lib" if exist "%OPENSSL_LIB%\libssl.lib" copy /Y "%OPENSSL_LIB%\libssl.lib" "%OPENSSL_LIB%\libssl_static.lib" >NUL
if not exist "%OPENSSL_LIB%\libcrypto_static.lib" (
  echo Missing "%OPENSSL_LIB%\libcrypto_static.lib".
  exit /b 1
)
if not exist "%OPENSSL_LIB%\libssl_static.lib" (
  echo Missing "%OPENSSL_LIB%\libssl_static.lib".
  exit /b 1
)
echo OpenSSL %OPENSSL_ARCH%: "%OPENSSL_ROOT%"
exit /b 0

:usage
echo Usage: %~nx0 NET_SNMP_SOURCE_ROOT
echo Example:
echo   %~nx0 C:\Users\kaestnja\source\repos\github.com\net-snmp\net-snmp
exit /b 1

:fail
echo.
echo Net-SNMP tool refresh failed.
exit /b 1
