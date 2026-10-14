@echo off
setlocal
echo ========================================================
echo   Rogue Legacy C++ - Windows Build Script
echo ========================================================

where cmake >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake is not found in PATH!
    echo Please install CMake from https://cmake.org/download/
    pause
    exit /b 1
)

echo [1/2] Configuring CMake for Release...
cmake -B build -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake configuration failed!
    echo Ensure Visual Studio C++ build tools or MinGW are installed.
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo [2/2] Building rogue_legacy_cpp.exe...
cmake --build build --config Release -j
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build failed!
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo ========================================================
echo   BUILD SUCCESSFUL!
echo ========================================================
if exist build\Release\rogue_legacy_cpp.exe (
    echo Executable is ready at: build\Release\rogue_legacy_cpp.exe
    copy build\Release\rogue_legacy_cpp.exe rogue_legacy.exe >nul
    echo Copied to root as: rogue_legacy.exe
) else if exist build\rogue_legacy_cpp.exe (
    echo Executable is ready at: build\rogue_legacy_cpp.exe
    copy build\rogue_legacy_cpp.exe rogue_legacy.exe >nul
    echo Copied to root as: rogue_legacy.exe
)

echo.
echo You can now launch rogue_legacy.exe to play!
pause
