@echo off
rem Build the Windows Hosts3D runtime into Release\windows\<arch> or Debug\windows\<arch>.
rem Also refresh the local runtime README in the output directory.
setlocal EnableExtensions
cd /d "%~dp0"

set "CONFIG=Release"
set "ARCH=x86"
set "PAUSE_ON_EXIT=1"

:parse_args
if "%~1"=="" goto :args_done
if /I "%~1"=="Release" set "CONFIG=Release" & shift & goto :parse_args
if /I "%~1"=="Debug" set "CONFIG=Debug" & shift & goto :parse_args
if /I "%~1"=="x86" set "ARCH=x86" & shift & goto :parse_args
if /I "%~1"=="x64" set "ARCH=x64" & shift & goto :parse_args
if /I "%~1"=="arm64" set "ARCH=arm64" & shift & goto :parse_args
if /I "%~1"=="--no-pause" set "PAUSE_ON_EXIT=0" & shift & goto :parse_args
call :usage
goto :fail

:args_done

set "GPP_EXE="
set "MACHINE="
if /I "%ARCH%"=="x86" call :probe_compiler "C:\msys64\mingw32\bin\g++.exe" "%ARCH%"
if /I "%ARCH%"=="x64" call :probe_compiler "C:\msys64\mingw64\bin\g++.exe" "%ARCH%"
if not defined GPP_EXE if /I "%ARCH%"=="x64" call :probe_compiler "C:\msys64\ucrt64\bin\g++.exe" "%ARCH%"
if not defined GPP_EXE (
  for /f "delims=" %%i in ('where g++.exe 2^>NUL') do (
    if not defined GPP_EXE call :probe_compiler "%%i" "%ARCH%"
  )
)
if not defined GPP_EXE (
  if /I "%ARCH%"=="x64" (
    echo Missing an x64 MinGW g++ toolchain.
    echo Install one of these and try again:
    echo   C:\msys64\mingw64\bin\g++.exe
    echo   C:\msys64\ucrt64\bin\g++.exe
  ) else if /I "%ARCH%"=="x86" (
    echo Missing an x86 MinGW g++ toolchain.
    echo Install:
    echo   C:\msys64\mingw32\bin\g++.exe
  ) else (
    echo Missing a supported %ARCH% MinGW g++ toolchain.
  )
  goto :fail
)
for %%i in ("%GPP_EXE%") do set "MINGW_BIN=%%~dpi"
for %%i in ("%MINGW_BIN%..") do set "MINGW_ROOT=%%~fi"
set "PATH=%MINGW_BIN%;%PATH%"
echo Using "%GPP_EXE%"
echo Compiler target "%MACHINE%"
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

set "OUTDIR=%CONFIG%\windows\%ARCH%"
if not exist "%OUTDIR%" mkdir "%OUTDIR%"
set "OBJDIR=build\windows\hosts3d\%ARCH%\%CONFIG%"
if not exist "%OBJDIR%" mkdir "%OBJDIR%"

set "GLFW_INCLUDE=%MINGW_ROOT%\include"
set "GLFW_LIBDIR=%MINGW_ROOT%\lib"
set "GLFW_BINDIR=%MINGW_ROOT%\bin"

if not exist "%GLFW_INCLUDE%\GLFW\glfw3.h" (
  echo Missing "%GLFW_INCLUDE%\GLFW\glfw3.h"
  echo Install the matching MSYS2 GLFW 3 package for this toolchain.
  goto :fail
)

set "GLFW_LIB_OPT="
if exist "%GLFW_LIBDIR%\libglfw3.a" set "GLFW_LIB_OPT=-lglfw3"
if not defined GLFW_LIB_OPT if exist "%GLFW_LIBDIR%\libglfw3dll.a" set "GLFW_LIB_OPT=-lglfw3dll"
if not defined GLFW_LIB_OPT (
  echo Missing GLFW 3 import/static library in "%GLFW_LIBDIR%"
  echo Expected libglfw3.a or libglfw3dll.a
  goto :fail
)

"%GPP_EXE%" -std=gnu++11 -Wall -O2 -I"%GLFW_INCLUDE%" -c -o "%OBJDIR%\llist.o" src/llist.cpp
if errorlevel 1 goto :fail
"%GPP_EXE%" -std=gnu++11 -Wall -O2 -I"%GLFW_INCLUDE%" -c -o "%OBJDIR%\misc.o" src/misc.cpp
if errorlevel 1 goto :fail
"%GPP_EXE%" -std=gnu++11 -Wall -O2 -I"%GLFW_INCLUDE%" -c -o "%OBJDIR%\glwin.o" src/glwin.cpp
if errorlevel 1 goto :fail
"%GPP_EXE%" -std=gnu++11 -Wall -O2 -I"%GLFW_INCLUDE%" -c -o "%OBJDIR%\objects.o" src/objects.cpp
if errorlevel 1 goto :fail
"%GPP_EXE%" -std=gnu++11 -Wall -O2 -I"%GLFW_INCLUDE%" -c -o "%OBJDIR%\hosts3d.o" src/hosts3d.cpp
if errorlevel 1 goto :fail
"%WINDRES_EXE%" -I"src" -O coff -i src/hosts3d.rc -o "%OBJDIR%\hosts3d-res.o"
if errorlevel 1 goto :fail
"%GPP_EXE%" -std=gnu++11 -Wall -O2 -static-libgcc -static-libstdc++ -o "%OUTDIR%\Hosts3D.exe" "%OBJDIR%\llist.o" "%OBJDIR%\misc.o" "%OBJDIR%\glwin.o" "%OBJDIR%\objects.o" "%OBJDIR%\hosts3d.o" "%OBJDIR%\hosts3d-res.o" -L"%GLFW_LIBDIR%" %GLFW_LIB_OPT% -lopengl32 -lglu32 -lws2_32 -liphlpapi
if errorlevel 1 goto :fail
if exist "%GLFW_BINDIR%\glfw3.dll" copy /Y "%GLFW_BINDIR%\glfw3.dll" "%OUTDIR%\glfw3.dll" >NUL
if exist "%MINGW_BIN%libwinpthread-1.dll" copy /Y "%MINGW_BIN%libwinpthread-1.dll" "%OUTDIR%\libwinpthread-1.dll" >NUL
if exist "README-runtime-windows.md" copy /Y "README-runtime-windows.md" "%OUTDIR%\README-runtime-windows.md" >NUL
echo Built "%OUTDIR%\Hosts3D.exe"
goto :success

:fail
if "%PAUSE_ON_EXIT%"=="1" pause
exit /b 1

:success
if "%PAUSE_ON_EXIT%"=="1" pause
exit /b 0

:usage
echo Usage: %~nx0 [Release^|Debug] [x86^|x64^|arm64] [--no-pause]
echo Defaults: Release x86
exit /b 0

:probe_compiler
if "%~1"=="" exit /b 1
if not exist "%~1" exit /b 1
set "CAND_MACHINE="
for /f "delims=" %%i in ('"%~1" -dumpmachine 2^>NUL') do set "CAND_MACHINE=%%i"
if not defined CAND_MACHINE exit /b 1
call :machine_matches "%CAND_MACHINE%" "%~2"
if errorlevel 1 exit /b 1
set "GPP_EXE=%~1"
set "MACHINE=%CAND_MACHINE%"
exit /b 0

:machine_matches
if /I "%~2"=="x86" (
  echo %~1 | findstr /I "i686 i586 i486 i386" >NUL && exit /b 0
  exit /b 1
)
if /I "%~2"=="x64" (
  echo %~1 | findstr /I "x86_64 amd64" >NUL && exit /b 0
  exit /b 1
)
if /I "%~2"=="arm64" (
  echo %~1 | findstr /I "aarch64 arm64" >NUL && exit /b 0
  exit /b 1
)
exit /b 1
