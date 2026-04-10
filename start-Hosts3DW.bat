@echo off
rem Start the already-built Windows Hosts3D runtime from Release\windows\<arch>.
rem This helper is for local testing and quick restarts, not for packaging.
setlocal EnableExtensions
cd /d "%~dp0"

set "ARG1=%~1"
set "ARG2=%~2"
set "ARG3=%~3"
set "CONFIG=Release"
set "ARCH=x86"
set "VIEW=window"

if /I "%ARG1%"=="Release" (
  set "CONFIG=Release"
  set "ARCH=%ARG2%"
  set "VIEW=%ARG3%"
) else if /I "%ARG1%"=="Debug" (
  set "CONFIG=Debug"
  set "ARCH=%ARG2%"
  set "VIEW=%ARG3%"
) else (
  set "ARCH=%ARG1%"
  set "VIEW=%ARG2%"
)

if "%ARCH%"=="" set "ARCH=x86"
if /I not "%ARCH%"=="x86" if /I not "%ARCH%"=="x64" (
  if /I "%ARCH%"=="window" (
    set "VIEW=%ARCH%"
    set "ARCH=x86"
  ) else if /I "%ARCH%"=="fullscreen" (
    set "VIEW=%ARCH%"
    set "ARCH=x86"
  ) else (
    echo Usage: %~nx0 [Release^|Debug] [x86^|x64] [window^|fullscreen]
    echo Defaults: Release x86 window
    exit /b 1
  )
)

if /I not "%CONFIG%"=="Release" if /I not "%CONFIG%"=="Debug" (
  echo Usage: %~nx0 [Release^|Debug] [x86^|x64] [window^|fullscreen]
  echo Defaults: Release x86 window
  exit /b 1
)

if "%VIEW%"=="" set "VIEW=window"
if /I not "%VIEW%"=="window" if /I not "%VIEW%"=="fullscreen" (
  echo Invalid view "%VIEW%". Use window or fullscreen.
  exit /b 1
)

set "BINDIR=%CD%\%CONFIG%\windows\%ARCH%"
set "EXE=%BINDIR%\Hosts3D.exe"

if not exist "%EXE%" (
  echo Missing "%EXE%"
  echo Build first with compile-hosts3d.bat %CONFIG%
  exit /b 1
)

for /f "tokens=2 delims= " %%i in ('tasklist ^| findstr /B /I "Hosts3D.exe"') do taskkill /F /PID %%i /T >NUL 2>NUL

if /I "%VIEW%"=="fullscreen" (
  start "Hosts3D (%CONFIG% %ARCH%)" /D "%BINDIR%" "%EXE%" -f
) else (
  start "Hosts3D (%CONFIG% %ARCH%)" /D "%BINDIR%" "%EXE%"
)

timeout /t 2 >NUL
tasklist | findstr /B /I "Hosts3D.exe" >NUL
if errorlevel 1 (
  echo Hosts3D.exe did not start.
  exit /b 1
)

echo Started Hosts3D from "%BINDIR%"
exit /b 0
