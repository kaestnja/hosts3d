@echo off
rem Stage all requested Windows runtime files and create distributable release ZIPs plus SHA256 files.
rem Defaults to Release x64+x86 without Packet.dll/wpcap.dll.
rem Uses the already-built runtime from Release\windows\<arch>.
rem Delegates package payload contents to Tools\Stage-RuntimePayload.ps1.
rem Normal release flow uses compile-all-windows.bat, which calls this script after building.
rem Use this script directly only to repackage already-built runtimes.
rem Debug packages use a -debug suffix to avoid overwriting release ZIPs.
rem Examples: package-all-windows.bat
rem           package-all-windows.bat with-npcap
setlocal EnableExtensions
cd /d "%~dp0"

set "CONFIG=Release"
set "ARCHES=x64 x86"
set "INCLUDE_NPCAP=0"

:parse_args
if "%~1"=="" goto :args_done
if /I "%~1"=="--help" call :usage & exit /b 0
if /I "%~1"=="/?" call :usage & exit /b 0
if /I "%~1"=="Debug" set "CONFIG=Debug" & shift & goto :parse_args
if /I "%~1"=="x64" set "ARCHES=x64" & shift & goto :parse_args
if /I "%~1"=="x86" set "ARCHES=x86" & shift & goto :parse_args
if /I "%~1"=="arm64" set "ARCHES=arm64" & shift & goto :parse_args
if /I "%~1"=="with-npcap" set "INCLUDE_NPCAP=1" & shift & goto :parse_args
call :usage
exit /b 1

:args_done

set "VERSION="
for /f "tokens=3" %%i in ('findstr /B /C:"#define HSD_VERSION_STR" src\\version.h') do set "VERSION=%%~i"
set "VERSION=%VERSION:"=%"
if not defined VERSION (
  echo Unable to detect version from src\version.h
  goto :fail
)

set "NPCAP_LABEL=without-npcap"
if "%INCLUDE_NPCAP%"=="1" set "NPCAP_LABEL=with-npcap"
set "POWERSHELL_EXE=pwsh"
where pwsh >NUL 2>NUL
if errorlevel 1 set "POWERSHELL_EXE=powershell"
echo Packaging Windows release assets: %CONFIG% %ARCHES% %NPCAP_LABEL%
for %%a in (%ARCHES%) do (
  echo.
  echo === Package %CONFIG% %%a ===
  call :package_arch %%a
  if errorlevel 1 goto :fail
)

echo.
echo Packaged requested Windows release assets successfully.
exit /b 0

:package_arch
set "ARCH=%~1"
set "RUNDIR=%CONFIG%\windows\%ARCH%"
set "DISTDIR=Release\dist"
set "PKGNAME=hosts3d-%VERSION%-windows-%ARCH%"
if /I "%CONFIG%"=="Debug" set "PKGNAME=%PKGNAME%-debug"
if "%INCLUDE_NPCAP%"=="1" set "PKGNAME=%PKGNAME%-with-npcap"
set "STAGEDIR=%DISTDIR%\%PKGNAME%"
set "ZIPPATH=%DISTDIR%\%PKGNAME%.zip"
set "HASHPATH=%DISTDIR%\%PKGNAME%-SHA256.txt"

if not exist "%RUNDIR%\Hosts3D.exe" (
  echo Missing "%RUNDIR%\Hosts3D.exe"
  echo Build the Windows runtime first.
  exit /b 1
)
if not exist "%RUNDIR%\hsen.exe" (
  echo Missing "%RUNDIR%\hsen.exe"
  echo Build the Windows runtime first.
  exit /b 1
)

if not exist "%DISTDIR%" mkdir "%DISTDIR%"
if exist "%STAGEDIR%" rmdir /S /Q "%STAGEDIR%"
if exist "%ZIPPATH%" del /Q "%ZIPPATH%"
if exist "%HASHPATH%" del /Q "%HASHPATH%"

mkdir "%STAGEDIR%"

if "%INCLUDE_NPCAP%"=="1" (
  "%POWERSHELL_EXE%" -NoProfile -ExecutionPolicy Bypass -File "%~dp0Tools\Stage-RuntimePayload.ps1" -Config "%CONFIG%" -Arch "%ARCH%" -Destination "%STAGEDIR%" -WithNpcap
) else (
  "%POWERSHELL_EXE%" -NoProfile -ExecutionPolicy Bypass -File "%~dp0Tools\Stage-RuntimePayload.ps1" -Config "%CONFIG%" -Arch "%ARCH%" -Destination "%STAGEDIR%"
)
if errorlevel 1 exit /b 1

"%POWERSHELL_EXE%" -NoProfile -Command "Add-Type -AssemblyName System.IO.Compression.FileSystem; if (Test-Path '%ZIPPATH%') { Remove-Item '%ZIPPATH%' -Force }; [System.IO.Compression.ZipFile]::CreateFromDirectory('%STAGEDIR%', '%ZIPPATH%')"
if errorlevel 1 exit /b 1

set "HASH="
for /f "skip=1 tokens=* delims=" %%i in ('certutil -hashfile "%ZIPPATH%" SHA256 ^| findstr /R "^[0-9A-F ][0-9A-F ]*$"') do (
  if not defined HASH set "HASH=%%i"
)
set "HASH=%HASH: =%"
if not defined HASH (
  echo Failed to calculate SHA256 for "%ZIPPATH%"
  exit /b 1
)
> "%HASHPATH%" <NUL set /p ="%HASH%  %PKGNAME%.zip"

echo Created "%ZIPPATH%"
echo Created "%HASHPATH%"
exit /b 0

:fail
echo.
echo Packaging failed.
exit /b 1

:usage
echo Usage: %~nx0 [Debug] [x64^|x86^|arm64] [with-npcap]
echo Defaults: Release x64+x86 without Npcap DLLs
echo Examples:
echo   %~nx0
echo   %~nx0 x64
echo   %~nx0 with-npcap
echo   %~nx0 Debug x64
exit /b 0
