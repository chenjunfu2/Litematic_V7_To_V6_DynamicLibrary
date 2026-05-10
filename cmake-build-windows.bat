@echo off
setlocal enabledelayedexpansion

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VSWHERE!" (
    echo Error: vswhere.exe not found. Please install Visual Studio or add msbuild to PATH.
    exit /b 1
)

for /f "usebackq delims=" %%i in (`"!VSWHERE!" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set "MSBUILD=%%i"
if not defined MSBUILD (
    echo Error: No MSBuild found by vswhere.
    exit /b 1
)
echo MSBuild path "%MSBUILD%"

for /f "usebackq delims=" %i in (`"!VSWHERE!" -latest -find **\devenv.com`) do "%i" Litematic_V7_To_V6_DynamicLibrary.sln /Upgrade
del /Q UpgradeLog*.htm 2>nul

"%MSBUILD%" Litematic_V7_To_V6_DynamicLibrary.sln /p:Configuration=Release /p:Platform=x64 /m
if %errorlevel% neq 0 exit /b %errorlevel%

mkdir .\artifacts\windows-x64
copy /Y .\x64\Release\Litematic_V7_To_V6_DynamicLibrary.dll .\artifacts\windows-x64\
