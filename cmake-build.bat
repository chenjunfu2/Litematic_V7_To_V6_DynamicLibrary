@echo off

cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG -g0" -S .
if %errorlevel% neq 0 exit /b %errorlevel%

cmake --build build
if %errorlevel% neq 0 exit /b %errorlevel%

strip --strip-all ./build/Litematic_V7_To_V6_DynamicLibrary/Litematic_V7_To_V6_DynamicLibrary.dll
if %errorlevel% neq 0 exit /b %errorlevel%
