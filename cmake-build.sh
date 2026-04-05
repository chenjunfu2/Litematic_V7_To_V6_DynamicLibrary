#!/bin/bash
set -e
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG -g0" -S .
cmake --build build
strip --strip-all ./build/Litematic_V7_To_V6_DynamicLibrary/Litematic_V7_To_V6_DynamicLibrary.so
