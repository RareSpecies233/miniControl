@echo off
setlocal

echo ========================================
echo   MiniRemote Build Script
echo ========================================

:: Check if build directory exists
if not exist "build" mkdir build

cd build

:: Configure with CMake
echo.
echo [1/2] Configuring with CMake...
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ..

if %ERRORLEVEL% neq 0 (
    echo ERROR: CMake configuration failed!
    pause
    exit /b 1
)

:: Build
echo.
echo [2/2] Building...
cmake --build . -j%NUMBER_OF_PROCESSORS%

if %ERRORLEVEL% neq 0 (
    echo ERROR: Build failed!
    pause
    exit /b 1
)

echo.
echo ========================================
echo   Build successful!
echo   Output: build\MiniRemote.exe
echo ========================================
pause
