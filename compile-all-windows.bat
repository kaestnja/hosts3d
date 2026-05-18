@echo off
rem Build all requested Windows Hosts3D binaries with the existing per-target scripts.
rem Defaults to Release x64+x86 and packages the resulting runtimes.
rem Normal use: compile-all-windows.bat
rem Lab/test package with bundled Npcap DLLs: compile-all-windows.bat with-npcap
rem Build only, for a broken/special architecture case: compile-all-windows.bat x64 no-package
setlocal EnableExtensions
cd /d "%~dp0"

set "CONFIG=Release"
set "ARCHES=x64 x86"
set "DO_PACKAGE=1"
set "INCLUDE_NPCAP=0"

:parse_args
if "%~1"=="" goto :args_done
if /I "%~1"=="--help" call :usage & exit /b 0
if /I "%~1"=="/?" call :usage & exit /b 0
if /I "%~1"=="Debug" set "CONFIG=Debug" & shift & goto :parse_args
if /I "%~1"=="x64" set "ARCHES=x64" & shift & goto :parse_args
if /I "%~1"=="x86" set "ARCHES=x86" & shift & goto :parse_args
if /I "%~1"=="arm64" set "ARCHES=arm64" & shift & goto :parse_args
if /I "%~1"=="no-package" set "DO_PACKAGE=0" & shift & goto :parse_args
if /I "%~1"=="with-npcap" set "INCLUDE_NPCAP=1" & shift & goto :parse_args
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
  call :package_requested
  if errorlevel 1 goto :fail
)

echo.
echo Built requested Windows binaries successfully.
exit /b 0

:package_requested
if /I "%ARCHES%"=="x64 x86" goto :package_requested_default_arches
if /I "%CONFIG%"=="Debug" goto :package_requested_one_arch_debug
if "%INCLUDE_NPCAP%"=="1" call "%~dp0package-all-windows.bat" %ARCHES% with-npcap
if not "%INCLUDE_NPCAP%"=="1" call "%~dp0package-all-windows.bat" %ARCHES%
exit /b %ERRORLEVEL%

:package_requested_one_arch_debug
if "%INCLUDE_NPCAP%"=="1" call "%~dp0package-all-windows.bat" Debug %ARCHES% with-npcap
if not "%INCLUDE_NPCAP%"=="1" call "%~dp0package-all-windows.bat" Debug %ARCHES%
exit /b %ERRORLEVEL%

:package_requested_default_arches
if /I "%CONFIG%"=="Debug" goto :package_requested_default_arches_debug
if "%INCLUDE_NPCAP%"=="1" call "%~dp0package-all-windows.bat" with-npcap
if not "%INCLUDE_NPCAP%"=="1" call "%~dp0package-all-windows.bat"
exit /b %ERRORLEVEL%

:package_requested_default_arches_debug
if "%INCLUDE_NPCAP%"=="1" call "%~dp0package-all-windows.bat" Debug with-npcap
if not "%INCLUDE_NPCAP%"=="1" call "%~dp0package-all-windows.bat" Debug
exit /b %ERRORLEVEL%

:fail
echo.
echo Build failed.
exit /b 1

:usage
echo Usage: %~nx0 [Debug] [x64^|x86^|arm64] [with-npcap] [no-package]
echo Defaults: Release x64+x86, package without Npcap DLLs
echo Examples:
echo   %~nx0
echo   %~nx0 Debug x64
echo   %~nx0 with-npcap
echo   %~nx0 x64 no-package
exit /b 0
