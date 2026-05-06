@echo off
rem Build all requested Windows Hosts3D binaries with the existing per-target scripts.
rem Defaults to Release all, which builds x64 first and then x86.
setlocal EnableExtensions
cd /d "%~dp0"

set "CONFIG=Release"
set "ARCHES=x64 x86"
set "DO_PACKAGE=0"
set "NPCAP_MODE=public"

:parse_args
if "%~1"=="" goto :args_done
if /I "%~1"=="--help" call :usage & exit /b 0
if /I "%~1"=="/?" call :usage & exit /b 0
if /I "%~1"=="Release" set "CONFIG=Release" & shift & goto :parse_args
if /I "%~1"=="Debug" set "CONFIG=Debug" & shift & goto :parse_args
if /I "%~1"=="all" set "ARCHES=x64 x86" & shift & goto :parse_args
if /I "%~1"=="x64" set "ARCHES=x64" & shift & goto :parse_args
if /I "%~1"=="x86" set "ARCHES=x86" & shift & goto :parse_args
if /I "%~1"=="arm64" set "ARCHES=arm64" & shift & goto :parse_args
if /I "%~1"=="--package" set "DO_PACKAGE=1" & shift & goto :parse_args
if /I "%~1"=="package" set "DO_PACKAGE=1" & shift & goto :parse_args
if /I "%~1"=="public" set "NPCAP_MODE=public" & shift & goto :parse_args
if /I "%~1"=="with-npcap" set "NPCAP_MODE=with-npcap" & shift & goto :parse_args
call :usage
exit /b 1

:args_done

echo Building Hosts3D Windows binaries: %CONFIG% %ARCHES%
for %%a in (%ARCHES%) do (
  echo.
  echo === Hosts3D %CONFIG% %%a ===
  call "%~dp0compile-hosts3d.bat" %CONFIG% %%a --no-pause
  if errorlevel 1 goto :fail

  echo.
  echo === hsen %CONFIG% %%a ===
  call "%~dp0compile-hsen.bat" %CONFIG% %%a --no-pause
  if errorlevel 1 goto :fail
)

if "%DO_PACKAGE%"=="1" (
  echo.
  echo Packaging Windows release assets: %CONFIG% %ARCHES% %NPCAP_MODE%
  for %%a in (%ARCHES%) do (
    echo.
    echo === Package %CONFIG% %%a ===
    call "%~dp0package-release-windows.bat" %CONFIG% %%a %NPCAP_MODE%
    if errorlevel 1 goto :fail
  )
)

echo.
echo Built requested Windows binaries successfully.
exit /b 0

:fail
echo.
echo Build failed.
exit /b 1

:usage
echo Usage: %~nx0 [Release^|Debug] [all^|x64^|x86^|arm64] [--package^|package] [public^|with-npcap]
echo Defaults: Release all public
echo Examples:
echo   %~nx0
echo   %~nx0 Release --package
echo   %~nx0 Debug x64
echo   %~nx0 Release all --package with-npcap
exit /b 0
