@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "CONFIG=Release"
set "PAUSE_ON_EXIT=1"

if /I "%~1"=="Release" set "CONFIG=Release"
if /I "%~1"=="Debug" set "CONFIG=Debug"
if /I "%~1"=="--no-pause" set "PAUSE_ON_EXIT=0"
if /I "%~2"=="--no-pause" set "PAUSE_ON_EXIT=0"

if not "%~1"=="" if /I not "%~1"=="Release" if /I not "%~1"=="Debug" if /I not "%~1"=="--no-pause" (
  echo Usage: %~nx0 [Release^|Debug]
  echo Default: Release
  echo Optional: --no-pause
  goto :fail
)
if not "%~2"=="" if /I not "%~2"=="--no-pause" (
  echo Usage: %~nx0 [Release^|Debug]
  echo Default: Release
  echo Optional: --no-pause
  goto :fail
)

set "GPP_EXE="
for /f "delims=" %%i in ('where g++.exe 2^>NUL') do (
  set "GPP_EXE=%%i"
  goto :found_gpp_hsen
)
for %%i in (
  "C:\msys64\mingw32\bin\g++.exe"
  "C:\msys64\mingw64\bin\g++.exe"
  "C:\msys64\ucrt64\bin\g++.exe"
) do (
  if not defined GPP_EXE if exist "%%~i" set "GPP_EXE=%%~fi"
)
:found_gpp_hsen
for %%i in ("%GPP_EXE%") do if defined GPP_EXE if /I not "%%~xi"==".exe" set "GPP_EXE="
if not defined GPP_EXE (
  echo Missing g++ in PATH.
  echo Install MSYS2/MinGW or place g++.exe in one of these locations:
  echo   C:\msys64\mingw32\bin
  echo   C:\msys64\mingw64\bin
  echo   C:\msys64\ucrt64\bin
  goto :fail
)
for %%i in ("%GPP_EXE%") do set "MINGW_BIN=%%~dpi"
set "PATH=%MINGW_BIN%;%PATH%"
echo Using "%GPP_EXE%"
for /f "delims=" %%i in ('"%GPP_EXE%" -dumpmachine 2^>NUL') do set "MACHINE=%%i"
if not defined MACHINE (
  echo Unable to detect compiler target with g++ -dumpmachine.
  goto :fail
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
  goto :fail
)

set "USE_WPCAP_A=0"
if exist "%WPCAP_LIBDIR%\libwpcap.a" set "USE_WPCAP_A=1"
if "%USE_WPCAP_A%"=="0" if not exist "%WPCAP_LIBDIR%\wpcap.lib" (
  echo Missing wpcap import library in "%WPCAP_LIBDIR%"
  echo Expected libwpcap.a or wpcap.lib
  goto :fail
)

g++ -Wall -O2 -I"%WPCAP_INCLUDE%" -c -o src/llist.o src/llist.cpp
if errorlevel 1 goto :fail
g++ -Wall -O2 -I"%WPCAP_INCLUDE%" -c -o src/misc.o src/misc.cpp
if errorlevel 1 goto :fail
g++ -Wall -O2 -I"%WPCAP_INCLUDE%" -c -o src/proto.o src/proto.cpp
if errorlevel 1 goto :fail
g++ -Wall -O2 -I"%WPCAP_INCLUDE%" -c -o src/hsen.o src/hsen.cpp
if errorlevel 1 goto :fail
if "%USE_WPCAP_A%"=="1" (
  g++ -Wall -O2 -static-libgcc -static-libstdc++ -o "%OUTDIR%\hsen.exe" src/llist.o src/misc.o src/proto.o src/hsen.o -L"%WPCAP_LIBDIR%" -lwpcap -lws2_32
) else (
  g++ -Wall -O2 -static-libgcc -static-libstdc++ -o "%OUTDIR%\hsen.exe" src/llist.o src/misc.o src/proto.o src/hsen.o "%WPCAP_LIBDIR%\wpcap.lib" -lws2_32
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
