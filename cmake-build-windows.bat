@echo off
setlocal enabledelayedexpansion

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VSWHERE!" (
    echo Error: vswhere.exe not found. Please install Visual Studio or add msbuild to PATH.
    exit /b 1
)

set "MSBUILD="
for /f "usebackq delims=" %%i in (`"!VSWHERE!" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do (
    set "MSBUILD=%%i"
    goto :MSBuildFound
)
:MSBuildFound
if not defined MSBUILD (
    echo Error: No MSBuild found by vswhere.
    exit /b 1
)
echo MSBuild path "%MSBUILD%"

set "DEVENV="
for /f "usebackq delims=" %%i in (`"!VSWHERE!" -latest -find **\devenv.com`) do (
    set "DEVENV=%%i"
    goto :DevenvFound
)
:DevenvFound
if not defined DEVENV (
    echo Error: No devenv.com found by vswhere. Cannot upgrade solution.
    exit /b 1
)
echo Upgrading solution with "%DEVENV%"

"%DEVENV%" Litematic_V7_To_V6_DynamicLibrary.sln /Upgrade
del /Q UpgradeLog*.htm 2>nul

"%MSBUILD%" Litematic_V7_To_V6_DynamicLibrary.sln /p:Configuration=Release /p:Platform=x64 /m
if %errorlevel% neq 0 exit /b %errorlevel%

mkdir .\artifacts\windows-x64
copy /Y .\x64\Release\Litematic_V7_To_V6_DynamicLibrary.dll .\artifacts\windows-x64\
