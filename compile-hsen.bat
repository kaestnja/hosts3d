@echo off
setlocal
cd /d "%~dp0"

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Release"
if /I not "%CONFIG%"=="Release" if /I not "%CONFIG%"=="Debug" (
  echo Usage: %~nx0 [Release^|Debug]
  exit /b 1
)

set "GPP_EXE="
for /f "delims=" %%i in ('where g++ 2^>NUL') do (
  set "GPP_EXE=%%i"
  goto :found_gpp_hsen
)
:found_gpp_hsen
if not defined GPP_EXE (
  echo Missing g++ in PATH.
  echo Add your MinGW bin directory to PATH before running this script.
  exit /b 1
)
for %%i in ("%GPP_EXE%") do set "MINGW_BIN=%%~dpi"
for /f "delims=" %%i in ('g++ -dumpmachine 2^>NUL') do set "MACHINE=%%i"
if not defined MACHINE (
  echo Unable to detect compiler target with g++ -dumpmachine.
  exit /b 1
)
set "ARCH=x86"
echo %MACHINE% | findstr /I "x86_64 amd64" >NUL && set "ARCH=x64"
echo %MACHINE% | findstr /I "aarch64 arm64" >NUL && set "ARCH=arm64"

set "OUTDIR=%CONFIG%\windows\%ARCH%"
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

set "WPCAP_INCLUDE=third_party\wpcap\include"
set "WPCAP_LIBDIR=third_party\wpcap\lib\windows\%ARCH%"
set "WPCAP_BINDIR=third_party\wpcap\bin\windows\%ARCH%"

if not exist "%WPCAP_INCLUDE%\pcap.h" (
  echo Missing "%WPCAP_INCLUDE%\pcap.h"
  echo Copy WinPcap/Npcap SDK headers into third_party\wpcap\include\
  exit /b 1
)

set "USE_WPCAP_A=0"
if exist "%WPCAP_LIBDIR%\libwpcap.a" set "USE_WPCAP_A=1"
if "%USE_WPCAP_A%"=="0" if not exist "%WPCAP_LIBDIR%\wpcap.lib" (
  echo Missing wpcap import library in "%WPCAP_LIBDIR%"
  echo Expected libwpcap.a or wpcap.lib
  exit /b 1
)

g++ -Wall -O2 -I"%WPCAP_INCLUDE%" -c -o src/llist.o src/llist.cpp
if errorlevel 1 exit /b 1
g++ -Wall -O2 -I"%WPCAP_INCLUDE%" -c -o src/misc.o src/misc.cpp
if errorlevel 1 exit /b 1
g++ -Wall -O2 -I"%WPCAP_INCLUDE%" -c -o src/proto.o src/proto.cpp
if errorlevel 1 exit /b 1
g++ -Wall -O2 -I"%WPCAP_INCLUDE%" -c -o src/hsen.o src/hsen.cpp
if errorlevel 1 exit /b 1
if "%USE_WPCAP_A%"=="1" (
  g++ -Wall -O2 -static-libgcc -static-libstdc++ -o "%OUTDIR%\hsen.exe" src/llist.o src/misc.o src/proto.o src/hsen.o -L"%WPCAP_LIBDIR%" -lwpcap -lws2_32
) else (
  g++ -Wall -O2 -static-libgcc -static-libstdc++ -o "%OUTDIR%\hsen.exe" src/llist.o src/misc.o src/proto.o src/hsen.o "%WPCAP_LIBDIR%\wpcap.lib" -lws2_32
)
if errorlevel 1 exit /b 1
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
if not exist "%OUTDIR%\wpcap.dll" echo Warning: wpcap.dll not found in third_party or installed Npcap paths.
if not exist "%OUTDIR%\Packet.dll" echo Warning: Packet.dll not found in third_party or installed Npcap paths.
echo Built "%OUTDIR%\hsen.exe"
