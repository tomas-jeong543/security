@echo off
setlocal

cd /d "%~dp0"

where clang++ >nul 2>nul
if errorlevel 1 (
    echo [ERROR] clang++ is not in PATH.
    echo Run this file from "x64 Native Tools Command Prompt for VS 2022".
    pause
    exit /b 1
)

where lld-link >nul 2>nul
if errorlevel 1 (
    echo [ERROR] lld-link is not in PATH.
    echo Run this file from "x64 Native Tools Command Prompt for VS 2022".
    pause
    exit /b 1
)

if not exist "src\main.cpp" (
    echo [ERROR] src\main.cpp was not found.
    pause
    exit /b 1
)

if not exist build mkdir build

echo [1/4] Compiling Release...
clang++ -std=c++17 -O2 -DNDEBUG -fno-exceptions -fno-rtti -fno-stack-protector -ffreestanding -c "src\main.cpp" -o "build\PenguinRift.obj"
if errorlevel 1 goto :fail

echo [2/4] Linking Release...
lld-link "build\PenguinRift.obj" kernel32.lib user32.lib ^
  /out:"PenguinRift.exe" ^
  /implib:"PenguinRift.lib" ^
  /entry:mainCRTStartup ^
  /subsystem:windows ^
  /machine:x64 ^
  /nodefaultlib ^
  /opt:ref ^
  /opt:icf
if errorlevel 1 goto :fail

echo [3/4] Compiling Training...
clang++ -std=c++17 -O0 -gcodeview -fno-exceptions -fno-rtti -fno-stack-protector -ffreestanding -c "src\main.cpp" -o "build\PenguinRift_Training.obj"
if errorlevel 1 goto :fail

echo [4/4] Linking Training...
lld-link "build\PenguinRift_Training.obj" kernel32.lib user32.lib ^
  /out:"PenguinRift_Training.exe" ^
  /implib:"PenguinRift_Training.lib" ^
  /entry:mainCRTStartup ^
  /subsystem:windows ^
  /machine:x64 ^
  /nodefaultlib ^
  /debug ^
  /pdb:"PenguinRift_Training.pdb" ^
  /map:"PenguinRift_Training.map"
if errorlevel 1 goto :fail

echo.
echo Build complete: PenguinRift.exe and PenguinRift_Training.exe
echo.
pause
exit /b 0

:fail
echo.
echo [ERROR] Build failed. Exit code: %ERRORLEVEL%
echo Make sure the game EXE is not currently running.
echo.
pause
exit /b 1
