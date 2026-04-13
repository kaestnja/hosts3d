@echo off
rem Stage the Windows runtime files and create a distributable release ZIP plus SHA256 file.
rem Defaults to a public ZIP without Packet.dll/wpcap.dll; use with-npcap for private test packages.
rem Uses the already-built runtime from Release\windows\<arch>.
setlocal EnableExtensions
cd /d "%~dp0"

set "CONFIG=Release"
set "ARCH=x86"
set "INCLUDE_NPCAP=0"

:parse_args
if "%~1"=="" goto :args_done
if /I "%~1"=="Release" set "CONFIG=Release" & shift & goto :parse_args
if /I "%~1"=="Debug" set "CONFIG=Debug" & shift & goto :parse_args
if /I "%~1"=="x86" set "ARCH=x86" & shift & goto :parse_args
if /I "%~1"=="x64" set "ARCH=x64" & shift & goto :parse_args
if /I "%~1"=="arm64" set "ARCH=arm64" & shift & goto :parse_args
if /I "%~1"=="with-npcap" set "INCLUDE_NPCAP=1" & shift & goto :parse_args
if /I "%~1"=="public" set "INCLUDE_NPCAP=0" & shift & goto :parse_args
echo Usage: %~nx0 [Release^|Debug] [x86^|x64^|arm64] [public^|with-npcap]
echo Defaults: Release x86 public
goto :fail

:args_done

set "VERSION="
for /f "tokens=3" %%i in ('findstr /B /C:"#define HSD_VERSION_STR" src\\version.h') do set "VERSION=%%~i"
set "VERSION=%VERSION:"=%"
if not defined VERSION (
  echo Unable to detect version from src\version.h
  goto :fail
)

set "RUNDIR=%CONFIG%\windows\%ARCH%"
set "DISTDIR=Release\dist"
set "PKGNAME=hosts3d-%VERSION%-windows-%ARCH%"
if "%INCLUDE_NPCAP%"=="1" set "PKGNAME=%PKGNAME%-with-npcap"
set "STAGEDIR=%DISTDIR%\%PKGNAME%"
set "ZIPPATH=%DISTDIR%\%PKGNAME%.zip"
set "HASHPATH=%DISTDIR%\%PKGNAME%-SHA256.txt"

if not exist "%RUNDIR%\Hosts3D.exe" (
  echo Missing "%RUNDIR%\Hosts3D.exe"
  echo Build the Windows runtime first.
  goto :fail
)
if not exist "%RUNDIR%\hsen.exe" (
  echo Missing "%RUNDIR%\hsen.exe"
  echo Build the Windows runtime first.
  goto :fail
)

if not exist "%DISTDIR%" mkdir "%DISTDIR%"
if exist "%STAGEDIR%" rmdir /S /Q "%STAGEDIR%"
if exist "%ZIPPATH%" del /Q "%ZIPPATH%"
if exist "%HASHPATH%" del /Q "%HASHPATH%"

mkdir "%STAGEDIR%"
mkdir "%STAGEDIR%\testing"

for %%f in (Hosts3D.exe hsen.exe glfw3.dll libwinpthread-1.dll) do (
  if not exist "%RUNDIR%\%%f" (
    echo Missing "%RUNDIR%\%%f"
    goto :fail
  )
  copy /Y "%RUNDIR%\%%f" "%STAGEDIR%\%%f" >NUL
)

if "%INCLUDE_NPCAP%"=="1" (
  for %%f in (Packet.dll wpcap.dll) do (
    if not exist "%RUNDIR%\%%f" (
      echo Missing "%RUNDIR%\%%f"
      goto :fail
    )
    copy /Y "%RUNDIR%\%%f" "%STAGEDIR%\%%f" >NUL
  )
)

copy /Y "COPYING" "%STAGEDIR%\COPYING" >NUL
copy /Y "README-runtime-windows.md" "%STAGEDIR%\README-runtime-windows.md" >NUL
copy /Y "README-runtime-windows.md" "%STAGEDIR%\README.md" >NUL
for %%f in (sim-hsen.ps1 sim-hsen.py demo-hsen.ps1 demo-hsen.py README.md) do (
  copy /Y "testing\%%f" "%STAGEDIR%\testing\%%f" >NUL
)

powershell -NoProfile -Command "Add-Type -AssemblyName System.IO.Compression.FileSystem; if (Test-Path '%ZIPPATH%') { Remove-Item '%ZIPPATH%' -Force }; [System.IO.Compression.ZipFile]::CreateFromDirectory('%STAGEDIR%', '%ZIPPATH%')"
if errorlevel 1 goto :fail

set "HASH="
for /f "skip=1 tokens=* delims=" %%i in ('certutil -hashfile "%ZIPPATH%" SHA256 ^| findstr /R "^[0-9A-F ][0-9A-F ]*$"') do (
  if not defined HASH set "HASH=%%i"
)
set "HASH=%HASH: =%"
if not defined HASH (
  echo Failed to calculate SHA256 for "%ZIPPATH%"
  goto :fail
)
> "%HASHPATH%" <NUL set /p ="%HASH%  %PKGNAME%.zip"

echo Created "%ZIPPATH%"
echo Created "%HASHPATH%"
goto :success

:fail
exit /b 1

:success
exit /b 0
