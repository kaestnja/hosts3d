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
  goto :found_gpp_hosts3d
)
for %%i in (
  "C:\msys64\mingw32\bin\g++.exe"
  "C:\msys64\mingw64\bin\g++.exe"
  "C:\msys64\ucrt64\bin\g++.exe"
) do (
  if not defined GPP_EXE if exist "%%~i" set "GPP_EXE=%%~fi"
)
:found_gpp_hosts3d
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
set "WINDRES_EXE="
if exist "%MINGW_BIN%windres.exe" set "WINDRES_EXE=%MINGW_BIN%windres.exe"
if not defined WINDRES_EXE (
  for /f "delims=" %%i in ('where windres.exe 2^>NUL') do (
    set "WINDRES_EXE=%%i"
    goto :found_windres_hosts3d
  )
)
:found_windres_hosts3d
if not defined WINDRES_EXE (
  echo Missing windres.exe in PATH.
  goto :fail
)
echo Using "%WINDRES_EXE%"
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

set "GLFW_INCLUDE=third_party\glfw2\include"
set "GLFW_LIBDIR=third_party\glfw2\lib\windows\%ARCH%"
set "GLFW_BINDIR=third_party\glfw2\bin\windows\%ARCH%"

if not exist "%GLFW_INCLUDE%\GL\glfw.h" (
  echo Missing "%GLFW_INCLUDE%\GL\glfw.h"
  echo Copy GLFW 2.x headers into third_party\glfw2\include\GL\
  goto :fail
)

set "GLFW_LIB_OPT="
if exist "%GLFW_LIBDIR%\libglfw.a" set "GLFW_LIB_OPT=-lglfw"
if not defined GLFW_LIB_OPT if exist "%GLFW_LIBDIR%\libglfwdll.a" set "GLFW_LIB_OPT=-lglfwdll"
if not defined GLFW_LIB_OPT (
  echo Missing GLFW import/static library in "%GLFW_LIBDIR%"
  echo Expected libglfw.a or libglfwdll.a
  goto :fail
)

g++ -Wall -O2 -I"%GLFW_INCLUDE%" -c -o src/llist.o src/llist.cpp
if errorlevel 1 goto :fail
g++ -Wall -O2 -I"%GLFW_INCLUDE%" -c -o src/misc.o src/misc.cpp
if errorlevel 1 goto :fail
g++ -Wall -O2 -I"%GLFW_INCLUDE%" -c -o src/glwin.o src/glwin.cpp
if errorlevel 1 goto :fail
g++ -Wall -O2 -I"%GLFW_INCLUDE%" -c -o src/objects.o src/objects.cpp
if errorlevel 1 goto :fail
g++ -Wall -O2 -I"%GLFW_INCLUDE%" -c -o src/hosts3d.o src/hosts3d.cpp
if errorlevel 1 goto :fail
"%WINDRES_EXE%" -I"src" -O coff -i src/hosts3d.rc -o src/hosts3d-res.o
if errorlevel 1 goto :fail
g++ -Wall -O2 -static-libgcc -static-libstdc++ -o "%OUTDIR%\Hosts3D.exe" src/llist.o src/misc.o src/glwin.o src/objects.o src/hosts3d.o src/hosts3d-res.o -L"%GLFW_LIBDIR%" %GLFW_LIB_OPT% -lopengl32 -lglu32 -lws2_32
if errorlevel 1 goto :fail
if exist "%GLFW_BINDIR%\glfw.dll" copy /Y "%GLFW_BINDIR%\glfw.dll" "%OUTDIR%\glfw.dll" >NUL
if exist "%MINGW_BIN%libwinpthread-1.dll" copy /Y "%MINGW_BIN%libwinpthread-1.dll" "%OUTDIR%\libwinpthread-1.dll" >NUL
echo Built "%OUTDIR%\Hosts3D.exe"
goto :success

:fail
if "%PAUSE_ON_EXIT%"=="1" pause
exit /b 1

:success
if "%PAUSE_ON_EXIT%"=="1" pause
exit /b 0
