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
if /I "%ARCH%"=="x86" call :probe_compiler "C:\msys64\mingw32\bin\g++.exe" "%ARCH%"
if /I "%ARCH%"=="x64" call :probe_compiler "C:\msys64\mingw64\bin\g++.exe" "%ARCH%"
if not defined GPP_EXE if /I "%ARCH%"=="x64" call :probe_compiler "C:\msys64\ucrt64\bin\g++.exe" "%ARCH%"
if not defined GPP_EXE (
  for /f "delims=" %%i in ('where g++.exe 2^>NUL') do (
    if not defined GPP_EXE call :probe_compiler "%%i" "%ARCH%"
  )
)
if not defined GPP_EXE (
  if /I "%ARCH%"=="x64" (
    echo Missing an x64 MinGW g++ toolchain.
    echo Install one of these and try again:
    echo   C:\msys64\mingw64\bin\g++.exe
    echo   C:\msys64\ucrt64\bin\g++.exe
  ) else if /I "%ARCH%"=="x86" (
    echo Missing an x86 MinGW g++ toolchain.
    echo Install:
    echo   C:\msys64\mingw32\bin\g++.exe
  ) else (
    echo Missing a supported %ARCH% MinGW g++ toolchain.
  )
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

"%GPP_EXE%" -Wall -O2 -I"%WPCAP_INCLUDE%" -c -o src/llist.o src/llist.cpp
if errorlevel 1 goto :fail
"%GPP_EXE%" -Wall -O2 -I"%WPCAP_INCLUDE%" -c -o src/misc.o src/misc.cpp
if errorlevel 1 goto :fail
"%GPP_EXE%" -Wall -O2 -I"%WPCAP_INCLUDE%" -c -o src/proto.o src/proto.cpp
if errorlevel 1 goto :fail
"%GPP_EXE%" -Wall -O2 -I"%WPCAP_INCLUDE%" -c -o src/hsen.o src/hsen.cpp
if errorlevel 1 goto :fail
"%WINDRES_EXE%" -I"src" -O coff -i src/hsen.rc -o src/hsen-res.o
if errorlevel 1 goto :fail
if "%USE_WPCAP_A%"=="1" (
  "%GPP_EXE%" -Wall -O2 -static-libgcc -static-libstdc++ -o "%OUTDIR%\hsen.exe" src/llist.o src/misc.o src/proto.o src/hsen.o src/hsen-res.o -L"%WPCAP_LIBDIR%" -lwpcap -lws2_32
) else (
  "%GPP_EXE%" -Wall -O2 -static-libgcc -static-libstdc++ -o "%OUTDIR%\hsen.exe" src/llist.o src/misc.o src/proto.o src/hsen.o src/hsen-res.o "%WPCAP_LIBDIR%\wpcap.lib" -lws2_32
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
