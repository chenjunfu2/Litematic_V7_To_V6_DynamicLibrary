@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

echo =============================================
echo    Litematic V7 To V6 DynamicLibrary Builder
echo =============================================
echo.

echo [1/3] Cleaning old build directory...
if exist build (
    echo Deleting build directory...
    rmdir /s /q build
    if exist build (
        echo [Warning] Failed to fully delete build directory. Please close programs using it manually.
        pause
    )
) else (
    echo [Info] No old build directory found.
)

echo.
echo [2/3] Configuring CMake
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG -g0" -S .
if %errorlevel% neq 0 (
    echo.
    echo [Error] CMake configuration failed!
    echo Please ensure Visual Studio 2022 is installed with "Desktop development with C++" workload.
    pause
    exit /b 1
)

echo.
echo [3/3] Starting build (Release)...
cmake --build build
if %errorlevel% neq 0 (
    echo.
    echo [Error] Build failed!
    pause
    exit /b 1
)

echo.
echo ========================================
echo    ✓ Build successful!
echo ========================================
echo.

pause