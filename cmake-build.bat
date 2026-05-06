@echo off

msbuild Litematic_V7_To_V6_DynamicLibrary.sln /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v142 /m
if %errorlevel% neq 0 exit /b %errorlevel%
