@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "CONFIG=Release"
set "ARCH=x86"

if /I "%~1"=="Release" set "CONFIG=Release"
if /I "%~1"=="Debug" set "CONFIG=Debug"
if /I "%~1"=="x86" set "ARCH=x86"
if /I "%~1"=="x64" set "ARCH=x64"
if /I "%~1"=="arm64" set "ARCH=arm64"
if /I "%~2"=="x86" set "ARCH=x86"
if /I "%~2"=="x64" set "ARCH=x64"
if /I "%~2"=="arm64" set "ARCH=arm64"

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

for %%f in (Hosts3D.exe hsen.exe glfw.dll libwinpthread-1.dll Packet.dll wpcap.dll) do (
  if not exist "%RUNDIR%\%%f" (
    echo Missing "%RUNDIR%\%%f"
    goto :fail
  )
  copy /Y "%RUNDIR%\%%f" "%STAGEDIR%\%%f" >NUL
)

copy /Y "COPYING" "%STAGEDIR%\COPYING" >NUL
copy /Y "README-runtime-windows.md" "%STAGEDIR%\README.md" >NUL

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
