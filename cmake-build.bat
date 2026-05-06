@echo off

set SLN_FILE=Litematic_V7_To_V6_DynamicLibrary.sln
set CONFIG=Release
set PLATFORM=x64

for /f "delims=" %%i in ('"%VSWHERE%" -latest -requires Microsoft.Component.MSBuild -find MSBuild\Current\Bin\MSBuild.exe') do (
    set "MSBUILD_PATH=%%i"
)

if not defined MSBUILD_PATH (
    echo ERROR: MSBuild.exe not found.
    exit /b 1
)

"%MSBUILD_PATH%" %SLN_FILE% /p:Configuration=%CONFIG% /p:Platform=%PLATFORM% /p:PlatformToolset=v142 /m
if %errorlevel% neq 0 exit /b %errorlevel%
