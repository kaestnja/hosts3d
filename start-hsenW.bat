@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Release"
if /I not "%CONFIG%"=="Release" if /I not "%CONFIG%"=="Debug" (
  echo Usage: %~nx0 [Release^|Debug] [x86^|x64]
  exit /b 1
)

set "ARCH=%~2"
if "%ARCH%"=="" set "ARCH=x86"
if /I not "%ARCH%"=="x86" if /I not "%ARCH%"=="x64" (
  echo Invalid architecture "%ARCH%". Use x86 or x64.
  exit /b 1
)

set "BINDIR=%CD%\%CONFIG%\windows\%ARCH%"
set "EXE=%BINDIR%\hsen.exe"
set "IFLIST=%CD%\hsen-interfaces-%COMPUTERNAME%.txt"
set "LOCALCFG=%CD%\start-hsenW.local.bat"
set "EXAMPLECFG=%CD%\start-hsenW.local.example.bat"

if not exist "%EXE%" (
  echo Missing "%EXE%"
  echo Build first with compile-hsen.bat %CONFIG%
  exit /b 1
)

set "HSEN_ID=1"
set "HSEN_IFACE="
set "HSEN_DEST=127.0.0.1"
set "HSEN_PROMISC=1"

if exist "%LOCALCFG%" call "%LOCALCFG%"

if not defined HSEN_IFACE (
  echo No interface configured.
  echo Creating interface list at "%IFLIST%"
  "%EXE%" -d > "%IFLIST%"
  if not exist "%LOCALCFG%" if exist "%EXAMPLECFG%" copy /Y "%EXAMPLECFG%" "%LOCALCFG%" >NUL
  echo Edit "%LOCALCFG%" and set HSEN_IFACE to one entry from "%IFLIST%".
  exit /b 1
)

set "PROMISC_ARG="
if /I "%HSEN_PROMISC%"=="1" set "PROMISC_ARG=-p"
if /I "%HSEN_PROMISC%"=="true" set "PROMISC_ARG=-p"
if /I "%HSEN_PROMISC%"=="yes" set "PROMISC_ARG=-p"
if /I "%HSEN_PROMISC%"=="-p" set "PROMISC_ARG=-p"

for /f "tokens=2 delims= " %%i in ('tasklist ^| findstr /B /I "hsen.exe"') do taskkill /F /PID %%i /T >NUL 2>NUL

if defined PROMISC_ARG (
  start "hsen (%CONFIG% %ARCH%)" /MIN /D "%BINDIR%" "%EXE%" %HSEN_ID% "%HSEN_IFACE%" %HSEN_DEST% %PROMISC_ARG%
) else (
  start "hsen (%CONFIG% %ARCH%)" /MIN /D "%BINDIR%" "%EXE%" %HSEN_ID% "%HSEN_IFACE%" %HSEN_DEST%
)

timeout /t 2 >NUL
tasklist | findstr /B /I "hsen.exe" >NUL
if errorlevel 1 (
  echo hsen.exe did not start.
  exit /b 1
)

echo Started hsen from "%BINDIR%"
echo Sensor ID: %HSEN_ID%
echo Interface: %HSEN_IFACE%
echo Destination: %HSEN_DEST%
exit /b 0
