#!/bin/bash
set -e

cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG -g0" -S .
cmake --build build

strip -Sx ./build/Litematic_V7_To_V6_DynamicLibrary/Litematic_V7_To_V6_DynamicLibrary.dylib

mkdir -p artifacts/macos-x64
cp -f ./build/Litematic_V7_To_V6_DynamicLibrary/libLitematic_V7_To_V6_DynamicLibrary.dylib artifacts/macos-x64/
