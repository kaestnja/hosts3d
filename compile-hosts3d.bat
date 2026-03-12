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
  goto :found_gpp_hosts3d
)
:found_gpp_hosts3d
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

set "GLFW_INCLUDE=third_party\glfw2\include"
set "GLFW_LIBDIR=third_party\glfw2\lib\windows\%ARCH%"
set "GLFW_BINDIR=third_party\glfw2\bin\windows\%ARCH%"

if not exist "%GLFW_INCLUDE%\GL\glfw.h" (
  echo Missing "%GLFW_INCLUDE%\GL\glfw.h"
  echo Copy GLFW 2.x headers into third_party\glfw2\include\GL\
  exit /b 1
)

set "GLFW_LIB_OPT="
if exist "%GLFW_LIBDIR%\libglfw.a" set "GLFW_LIB_OPT=-lglfw"
if not defined GLFW_LIB_OPT if exist "%GLFW_LIBDIR%\libglfwdll.a" set "GLFW_LIB_OPT=-lglfwdll"
if not defined GLFW_LIB_OPT (
  echo Missing GLFW import/static library in "%GLFW_LIBDIR%"
  echo Expected libglfw.a or libglfwdll.a
  exit /b 1
)

g++ -Wall -O2 -I"%GLFW_INCLUDE%" -c -o src/llist.o src/llist.cpp
if errorlevel 1 exit /b 1
g++ -Wall -O2 -I"%GLFW_INCLUDE%" -c -o src/misc.o src/misc.cpp
if errorlevel 1 exit /b 1
g++ -Wall -O2 -I"%GLFW_INCLUDE%" -c -o src/glwin.o src/glwin.cpp
if errorlevel 1 exit /b 1
g++ -Wall -O2 -I"%GLFW_INCLUDE%" -c -o src/objects.o src/objects.cpp
if errorlevel 1 exit /b 1
g++ -Wall -O2 -I"%GLFW_INCLUDE%" -c -o src/hosts3d.o src/hosts3d.cpp
if errorlevel 1 exit /b 1
g++ -Wall -O2 -static-libgcc -static-libstdc++ -o "%OUTDIR%\Hosts3D.exe" src/llist.o src/misc.o src/glwin.o src/objects.o src/hosts3d.o -L"%GLFW_LIBDIR%" %GLFW_LIB_OPT% -lopengl32 -lglu32 -lws2_32
if errorlevel 1 exit /b 1
if exist "%GLFW_BINDIR%\glfw.dll" copy /Y "%GLFW_BINDIR%\glfw.dll" "%OUTDIR%\glfw.dll" >NUL
if exist "%MINGW_BIN%libwinpthread-1.dll" copy /Y "%MINGW_BIN%libwinpthread-1.dll" "%OUTDIR%\libwinpthread-1.dll" >NUL
echo Built "%OUTDIR%\Hosts3D.exe"
