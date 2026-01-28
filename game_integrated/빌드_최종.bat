@echo off
setlocal enabledelayedexpansion
chcp 65001 >nul
pushd "%~dp0"

echo ========================================
echo   Simple Build (MSYS2 UCRT64)
echo ========================================
echo.

rem 1) g++ auto-detect - check common locations first
set "GPP="
if exist "C:\Program Files (x86)\ucrt64\bin\g++.exe" set "GPP=C:\Program Files (x86)\ucrt64\bin\g++.exe"
if "!GPP!"=="" if exist "C:\Program Files (x86)\mingw64\bin\g++.exe" set "GPP=C:\Program Files (x86)\mingw64\bin\g++.exe"
if "!GPP!"=="" if exist "C:\Program Files (x86)\clang64\bin\g++.exe" set "GPP=C:\Program Files (x86)\clang64\bin\g++.exe"
if "!GPP!"=="" if exist "C:\msys64\ucrt64\bin\g++.exe" set "GPP=C:\msys64\ucrt64\bin\g++.exe"
if "!GPP!"=="" if exist "C:\msys64\mingw64\bin\g++.exe" set "GPP=C:\msys64\mingw64\bin\g++.exe"
if "!GPP!"=="" if exist "C:\msys64\clang64\bin\g++.exe" set "GPP=C:\msys64\clang64\bin\g++.exe"
if "!GPP!"=="" if exist "D:\msys64\ucrt64\bin\g++.exe" set "GPP=D:\msys64\ucrt64\bin\g++.exe"
if "!GPP!"=="" if exist "D:\msys64\mingw64\bin\g++.exe" set "GPP=D:\msys64\mingw64\bin\g++.exe"
if "!GPP!"=="" if exist "E:\msys64\ucrt64\bin\g++.exe" set "GPP=E:\msys64\ucrt64\bin\g++.exe"
if "!GPP!"=="" if exist "E:\msys64\mingw64\bin\g++.exe" set "GPP=E:\msys64\mingw64\bin\g++.exe"

rem Try to find g++ in PATH
if "!GPP!"=="" (
  for /f "usebackq delims=" %%p in (`where g++.exe 2^>nul`) do (
    if "!GPP!"=="" set "GPP=%%p"
  )
)

rem Search all drives for msys64
if "!GPP!"=="" (
  echo [INFO] Searching for MSYS2 installation...
  for %%D in (C D E F G H) do (
    if "!GPP!"=="" if exist "%%D:\msys64\ucrt64\bin\g++.exe" set "GPP=%%D:\msys64\ucrt64\bin\g++.exe"
    if "!GPP!"=="" if exist "%%D:\msys64\mingw64\bin\g++.exe" set "GPP=%%D:\msys64\mingw64\bin\g++.exe"
    if "!GPP!"=="" if exist "%%D:\msys64\clang64\bin\g++.exe" set "GPP=%%D:\msys64\clang64\bin\g++.exe"
  )
)

if "!GPP!"=="" goto :ask_gpp
if not exist "!GPP!" goto :ask_gpp
goto :gpp_found

:ask_gpp
echo [INFO] Could not auto-detect g++. Please paste full path (e.g., C:\msys64\ucrt64\bin\g++.exe)
set /p "GPP=GPP path: "
if "!GPP!"=="" goto :ask_gpp
if not exist "!GPP!" (
  echo ERROR: g++ not found at "!GPP!"
  set "GPP="
  goto :ask_gpp
)

:gpp_found

rem Extract MSYS_ROOT from GPP path
for %%D in ("!GPP!") do set "GPP_DIR=%%~dpD"
for %%R in ("!GPP_DIR!..") do set "MSYS_ROOT=%%~fR"

rem 2) SDL paths auto-detect
set "SDL_INC="
set "SDL_LIB="
if exist "C:\Program Files (x86)\ucrt64\include\SDL2\SDL.h" set "SDL_INC=C:\Program Files (x86)\ucrt64\include\SDL2"
if "!SDL_INC!"=="" if exist "C:\Program Files (x86)\mingw64\include\SDL2\SDL.h" set "SDL_INC=C:\Program Files (x86)\mingw64\include\SDL2"
if "!SDL_INC!"=="" if exist "!MSYS_ROOT!\include\SDL2\SDL.h" set "SDL_INC=!MSYS_ROOT!\include\SDL2"
if "!SDL_INC!"=="" if exist "C:\msys64\ucrt64\include\SDL2\SDL.h" set "SDL_INC=C:\msys64\ucrt64\include\SDL2"
if "!SDL_INC!"=="" if exist "C:\msys64\mingw64\include\SDL2\SDL.h" set "SDL_INC=C:\msys64\mingw64\include\SDL2"
if "!SDL_INC!"=="" if exist "D:\msys64\ucrt64\include\SDL2\SDL.h" set "SDL_INC=D:\msys64\ucrt64\include\SDL2"
if "!SDL_INC!"=="" if exist "D:\msys64\mingw64\include\SDL2\SDL.h" set "SDL_INC=D:\msys64\mingw64\include\SDL2"
if "!SDL_INC!"=="" if exist "E:\msys64\ucrt64\include\SDL2\SDL.h" set "SDL_INC=E:\msys64\ucrt64\include\SDL2"
if "!SDL_INC!"=="" if exist "E:\msys64\mingw64\include\SDL2\SDL.h" set "SDL_INC=E:\msys64\mingw64\include\SDL2"

if exist "C:\Program Files (x86)\ucrt64\lib" set "SDL_LIB=C:\Program Files (x86)\ucrt64\lib"
if "!SDL_LIB!"=="" if exist "C:\Program Files (x86)\mingw64\lib" set "SDL_LIB=C:\Program Files (x86)\mingw64\lib"
if "!SDL_LIB!"=="" if exist "!MSYS_ROOT!\lib" set "SDL_LIB=!MSYS_ROOT!\lib"
if "!SDL_LIB!"=="" if exist "C:\msys64\ucrt64\lib" set "SDL_LIB=C:\msys64\ucrt64\lib"
if "!SDL_LIB!"=="" if exist "C:\msys64\mingw64\lib" set "SDL_LIB=C:\msys64\mingw64\lib"
if "!SDL_LIB!"=="" if exist "D:\msys64\ucrt64\lib" set "SDL_LIB=D:\msys64\ucrt64\lib"
if "!SDL_LIB!"=="" if exist "D:\msys64\mingw64\lib" set "SDL_LIB=D:\msys64\mingw64\lib"
if "!SDL_LIB!"=="" if exist "E:\msys64\ucrt64\lib" set "SDL_LIB=E:\msys64\ucrt64\lib"
if "!SDL_LIB!"=="" if exist "E:\msys64\mingw64\lib" set "SDL_LIB=E:\msys64\mingw64\lib"

rem Search all drives if still not found
if "!SDL_INC!"=="" (
  echo [INFO] Searching for SDL2 installation...
  for %%D in (C D E F G H) do (
    if "!SDL_INC!"=="" if exist "%%D:\msys64\ucrt64\include\SDL2\SDL.h" set "SDL_INC=%%D:\msys64\ucrt64\include\SDL2"
    if "!SDL_INC!"=="" if exist "%%D:\msys64\mingw64\include\SDL2\SDL.h" set "SDL_INC=%%D:\msys64\mingw64\include\SDL2"
  )
)
if "!SDL_LIB!"=="" (
  for %%D in (C D E F G H) do (
    if "!SDL_LIB!"=="" if exist "%%D:\msys64\ucrt64\lib" set "SDL_LIB=%%D:\msys64\ucrt64\lib"
    if "!SDL_LIB!"=="" if exist "%%D:\msys64\mingw64\lib" set "SDL_LIB=%%D:\msys64\mingw64\lib"
  )
)

if "!SDL_INC!"=="" goto :ask_sdl_inc
goto :sdl_inc_found

:ask_sdl_inc
echo [INFO] Could not find SDL include. Enter folder containing SDL.h (e.g., C:\msys64\ucrt64\include\SDL2)
set /p "SDL_INC=SDL include path: "
if "!SDL_INC!"=="" goto :ask_sdl_inc

:sdl_inc_found
if "!SDL_LIB!"=="" goto :ask_sdl_lib
goto :sdl_lib_found

:ask_sdl_lib
echo [INFO] Could not find SDL lib. Enter folder path (e.g., C:\msys64\ucrt64\lib)
set /p "SDL_LIB=SDL lib path: "
if "!SDL_LIB!"=="" goto :ask_sdl_lib

:sdl_lib_found

echo Using g++ : !GPP!
echo SDL_INC  : !SDL_INC!
echo SDL_LIB  : !SDL_LIB!

rem 3) PATH and temp setup
set "PATH=!GPP_DIR!;%PATH%"
set "OPENBW_TMP=C:\OpenBWTemp"
if not exist "!OPENBW_TMP!" mkdir "!OPENBW_TMP!" >nul 2>&1
set "TEMP=!OPENBW_TMP!"
set "TMP=!OPENBW_TMP!"

rem 4) Validate SDL headers and libraries
if not exist "!SDL_INC!\SDL.h" (
  echo ERROR: SDL.h not found at "!SDL_INC!\SDL.h"
  goto :end
)
if not exist "!SDL_LIB!\libSDL2.dll.a" if not exist "!SDL_LIB!\SDL2.lib" (
  echo ERROR: SDL2 library not found in "!SDL_LIB!"
  goto :end
)

echo.
echo Start build...
"!GPP!" -o OpenBW_Game.exe player_game_complete.cpp sdl2.cpp -I.. -I. -I../ui -I"!SDL_INC!" -L"!SDL_LIB!" -std=c++17 -DOPENBW_ENABLE_UI -DOPENBW_NO_SDL_MIXER -O2 -lmingw32 -lSDL2main -lSDL2 -lSDL2_image -static-libgcc -static-libstdc++
if errorlevel 1 (
  echo.
  echo ========================================
  echo   Build FAILED
  echo ========================================
  goto :end
)

echo.
echo ========================================
echo   Build SUCCESS
echo ========================================
echo.
echo Running OpenBW_Game.exe ...
start "" OpenBW_Game.exe

:end
popd
