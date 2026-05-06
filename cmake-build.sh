#!/bin/bash
set -e
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG -g0" -S .
cmake --build build

if [[ "$(uname)" == "Darwin" ]]; then
    strip -Sx ./build/Litematic_V7_To_V6_DynamicLibrary/Litematic_V7_To_V6_DynamicLibrary.dylib
elif [[ "$(uname)" == "Linux" ]]; then
    strip --strip-all ./build/Litematic_V7_To_V6_DynamicLibrary/Litematic_V7_To_V6_DynamicLibrary.so
else
    echo "Unsupported OS: $(uname)"
    exit 1
fi
