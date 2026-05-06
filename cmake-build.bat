@echo off

set SLN_FILE=Litematic_V7_To_V6_DynamicLibrary.sln
set CONFIG=Release
set PLATFORM=x64

msbuild %SLN_FILE% /p:Configuration=%CONFIG% /p:Platform=%PLATFORM% /p:PlatformToolset=v142 /m
if %errorlevel% neq 0 exit /b %errorlevel%
