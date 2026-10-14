@echo off
rem Build the Windows hsen runtime into Release\windows\<arch> or Debug\windows\<arch>.
rem Also refresh the local runtime README in the output directory.
setlocal EnableExtensions
cd /d "%~dp0"

set "CONFIG=Release"
set "ARCH=x86"
set "PAUSE_ON_EXIT=1"

:parse_args
if "%~1"=="" goto :args_done
if /I "%~1"=="Release" set "CONFIG=Release" & shift & goto :parse_args
if /I "%~1"=="Debug" set "CONFIG=Debug" & shift & goto :parse_args
if /I "%~1"=="x86" set "ARCH=x86" & shift & goto :parse_args
if /I "%~1"=="x64" set "ARCH=x64" & shift & goto :parse_args
if /I "%~1"=="arm64" set "ARCH=arm64" & shift & goto :parse_args
if /I "%~1"=="--no-pause" set "PAUSE_ON_EXIT=0" & shift & goto :parse_args
call :usage
goto :fail

:args_done

set "GPP_EXE="
set "MACHINE="
rem Prefer an explicit prefix or an existing compatible compiler on PATH.
if defined HOSTS3D_TOOLCHAIN_ROOT (
  call :probe_compiler "%HOSTS3D_TOOLCHAIN_ROOT%\bin\g++.exe" "%ARCH%"
  if not defined GPP_EXE (
    echo HOSTS3D_TOOLCHAIN_ROOT does not provide a compatible %ARCH% MinGW compiler:
    echo   "%HOSTS3D_TOOLCHAIN_ROOT%\bin\g++.exe"
    goto :fail
  )
)
if not defined GPP_EXE (
  for /f "delims=" %%i in ('where g++.exe 2^>NUL') do (
    if not defined GPP_EXE call :probe_compiler "%%i" "%ARCH%"
  )
)
rem MSYS2 is a recommended default installation, not a required layout.
if not defined HOSTS3D_MSYS2_ROOT set "HOSTS3D_MSYS2_ROOT=C:\msys64"
set "TOOLCHAIN_FOLDERS="
if /I "%ARCH%"=="x86" set "TOOLCHAIN_FOLDERS=mingw32"
if /I "%ARCH%"=="x64" set "TOOLCHAIN_FOLDERS=mingw64 ucrt64 clang64"
if /I "%ARCH%"=="arm64" set "TOOLCHAIN_FOLDERS=clangarm64"
if not defined GPP_EXE (
  for %%f in (%TOOLCHAIN_FOLDERS%) do (
    if not defined GPP_EXE call :probe_compiler "%HOSTS3D_MSYS2_ROOT%\%%f\bin\g++.exe" "%ARCH%"
  )
)
rem Existing native WinLibs installations also work without an MSYS2 shell.
if not defined GPP_EXE if defined LOCALAPPDATA (
  for /f "delims=" %%d in ('dir /b /ad /on "%LOCALAPPDATA%\Microsoft\WinGet\Packages\BrechtSanders.WinLibs*" 2^>NUL') do (
    for %%f in (%TOOLCHAIN_FOLDERS%) do (
      if not defined GPP_EXE call :probe_compiler "%LOCALAPPDATA%\Microsoft\WinGet\Packages\%%d\%%f\bin\g++.exe" "%ARCH%"
    )
  )
)
if not defined GPP_EXE (
  echo Missing a compatible %ARCH% MinGW g++ toolchain.
  echo Use an existing compiler on PATH or set HOSTS3D_TOOLCHAIN_ROOT to its prefix.
  echo If no development environment is installed, see README.md for recommended MSYS2 setup.
  goto :fail
)
for %%i in ("%GPP_EXE%") do set "MINGW_BIN=%%~dpi"
set "PATH=%MINGW_BIN%;%PATH%"
echo Using "%GPP_EXE%"
echo Compiler target "%MACHINE%"
set "WINDRES_EXE="
if exist "%MINGW_BIN%windres.exe" set "WINDRES_EXE=%MINGW_BIN%windres.exe"
if not defined WINDRES_EXE (
  for /f "delims=" %%i in ('where windres.exe 2^>NUL') do (
    set "WINDRES_EXE=%%i"
    goto :found_windres_hsen
  )
)
:found_windres_hsen
if not defined WINDRES_EXE (
  echo Missing windres.exe in PATH.
  goto :fail
)
echo Using "%WINDRES_EXE%"

set "OUTDIR=%CONFIG%\windows\%ARCH%"
if not exist "%OUTDIR%" mkdir "%OUTDIR%"
set "OBJDIR=build\windows\hsen\%ARCH%\%CONFIG%"
if not exist "%OBJDIR%" mkdir "%OBJDIR%"
set "CXXFLAGS=-Wall -O2"
if /I "%CONFIG%"=="Debug" set "CXXFLAGS=-Wall -g -O0 -DDEBUG"

set "WPCAP_INCLUDE=third_party\wpcap\include"
set "WPCAP_LIBDIR=third_party\wpcap\lib\windows\%ARCH%"
set "WPCAP_BINDIR=third_party\wpcap\bin\windows\%ARCH%"

if not exist "%WPCAP_INCLUDE%\pcap.h" (
  echo Missing "%WPCAP_INCLUDE%\pcap.h"
  echo Copy WinPcap/Npcap SDK headers into third_party\wpcap\include\
  goto :fail
)

set "USE_WPCAP_A=0"
if exist "%WPCAP_LIBDIR%\libwpcap.a" set "USE_WPCAP_A=1"
if "%USE_WPCAP_A%"=="0" if not exist "%WPCAP_LIBDIR%\wpcap.lib" (
  echo Missing wpcap import library in "%WPCAP_LIBDIR%"
  echo Expected libwpcap.a or wpcap.lib
  goto :fail
)

"%GPP_EXE%" %CXXFLAGS% -I"%WPCAP_INCLUDE%" -c -o "%OBJDIR%\llist.o" src/llist.cpp
if errorlevel 1 goto :fail
"%GPP_EXE%" %CXXFLAGS% -I"%WPCAP_INCLUDE%" -c -o "%OBJDIR%\misc.o" src/misc.cpp
if errorlevel 1 goto :fail
"%GPP_EXE%" %CXXFLAGS% -I"%WPCAP_INCLUDE%" -c -o "%OBJDIR%\proto.o" src/proto.cpp
if errorlevel 1 goto :fail
"%GPP_EXE%" %CXXFLAGS% -I"%WPCAP_INCLUDE%" -c -o "%OBJDIR%\hsen.o" src/hsen.cpp
if errorlevel 1 goto :fail
"%WINDRES_EXE%" -I"src" -O coff -i src/hsen.rc -o "%OBJDIR%\hsen-res.o"
if errorlevel 1 goto :fail
if "%USE_WPCAP_A%"=="1" (
  "%GPP_EXE%" %CXXFLAGS% -static-libgcc -static-libstdc++ -o "%OUTDIR%\hsen.exe" "%OBJDIR%\llist.o" "%OBJDIR%\misc.o" "%OBJDIR%\proto.o" "%OBJDIR%\hsen.o" "%OBJDIR%\hsen-res.o" -L"%WPCAP_LIBDIR%" -lwpcap -lws2_32
) else (
  "%GPP_EXE%" %CXXFLAGS% -static-libgcc -static-libstdc++ -o "%OUTDIR%\hsen.exe" "%OBJDIR%\llist.o" "%OBJDIR%\misc.o" "%OBJDIR%\proto.o" "%OBJDIR%\hsen.o" "%OBJDIR%\hsen-res.o" "%WPCAP_LIBDIR%\wpcap.lib" -lws2_32
)
if errorlevel 1 goto :fail
if exist "%WPCAP_BINDIR%\wpcap.dll" copy /Y "%WPCAP_BINDIR%\wpcap.dll" "%OUTDIR%\wpcap.dll" >NUL
if exist "%WPCAP_BINDIR%\Packet.dll" copy /Y "%WPCAP_BINDIR%\Packet.dll" "%OUTDIR%\Packet.dll" >NUL
if not exist "%OUTDIR%\wpcap.dll" (
  if /I "%ARCH%"=="x86" (
    if exist "%SystemRoot%\SysWOW64\Npcap\wpcap.dll" copy /Y "%SystemRoot%\SysWOW64\Npcap\wpcap.dll" "%OUTDIR%\wpcap.dll" >NUL
    if not exist "%OUTDIR%\wpcap.dll" if exist "%SystemRoot%\SysWOW64\wpcap.dll" copy /Y "%SystemRoot%\SysWOW64\wpcap.dll" "%OUTDIR%\wpcap.dll" >NUL
  ) else (
    if exist "%SystemRoot%\System32\Npcap\wpcap.dll" copy /Y "%SystemRoot%\System32\Npcap\wpcap.dll" "%OUTDIR%\wpcap.dll" >NUL
    if not exist "%OUTDIR%\wpcap.dll" if exist "%SystemRoot%\System32\wpcap.dll" copy /Y "%SystemRoot%\System32\wpcap.dll" "%OUTDIR%\wpcap.dll" >NUL
  )
)
if not exist "%OUTDIR%\Packet.dll" (
  if /I "%ARCH%"=="x86" (
    if exist "%SystemRoot%\SysWOW64\Npcap\Packet.dll" copy /Y "%SystemRoot%\SysWOW64\Npcap\Packet.dll" "%OUTDIR%\Packet.dll" >NUL
    if not exist "%OUTDIR%\Packet.dll" if exist "%SystemRoot%\SysWOW64\Packet.dll" copy /Y "%SystemRoot%\SysWOW64\Packet.dll" "%OUTDIR%\Packet.dll" >NUL
  ) else (
    if exist "%SystemRoot%\System32\Npcap\Packet.dll" copy /Y "%SystemRoot%\System32\Npcap\Packet.dll" "%OUTDIR%\Packet.dll" >NUL
    if not exist "%OUTDIR%\Packet.dll" if exist "%SystemRoot%\System32\Packet.dll" copy /Y "%SystemRoot%\System32\Packet.dll" "%OUTDIR%\Packet.dll" >NUL
  )
)
if exist "%MINGW_BIN%libwinpthread-1.dll" copy /Y "%MINGW_BIN%libwinpthread-1.dll" "%OUTDIR%\libwinpthread-1.dll" >NUL
if exist "README-runtime-windows.md" copy /Y "README-runtime-windows.md" "%OUTDIR%\README-runtime-windows.md" >NUL
if not exist "%OUTDIR%\wpcap.dll" echo Warning: wpcap.dll not found in third_party or installed Npcap paths.
if not exist "%OUTDIR%\Packet.dll" echo Warning: Packet.dll not found in third_party or installed Npcap paths.
echo Built "%OUTDIR%\hsen.exe"
goto :success

:fail
if "%PAUSE_ON_EXIT%"=="1" pause
exit /b 1

:success
if "%PAUSE_ON_EXIT%"=="1" pause
exit /b 0

:usage
echo Usage: %~nx0 [Release^|Debug] [x86^|x64^|arm64] [--no-pause]
echo Defaults: Release x86
exit /b 0

:probe_compiler
if "%~1"=="" exit /b 1
if not exist "%~1" exit /b 1
set "CAND_MACHINE="
for /f "delims=" %%i in ('"%~1" -dumpmachine 2^>NUL') do set "CAND_MACHINE=%%i"
if not defined CAND_MACHINE exit /b 1
call :machine_matches "%CAND_MACHINE%" "%~2"
if errorlevel 1 exit /b 1
set "GPP_EXE=%~1"
set "MACHINE=%CAND_MACHINE%"
exit /b 0

:machine_matches
echo %~1 | findstr /I "mingw" >NUL || exit /b 1
if /I "%~2"=="x86" (
  echo %~1 | findstr /I "i686 i586 i486 i386" >NUL && exit /b 0
  exit /b 1
)
if /I "%~2"=="x64" (
  echo %~1 | findstr /I "x86_64 amd64" >NUL && exit /b 0
  exit /b 1
)
if /I "%~2"=="arm64" (
  echo %~1 | findstr /I "aarch64 arm64" >NUL && exit /b 0
  exit /b 1
)
exit /b 1
