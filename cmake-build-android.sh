#!/bin/bash
set -e

ANDROID_ABI=${ANDROID_ABI:-arm64-v8a}
ANDROID_PLATFORM=${ANDROID_PLATFORM:-android-21}
BUILD_TYPE=${BUILD_TYPE:-Release}

cmake -B build-android \
    -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI="$ANDROID_ABI" \
    -DANDROID_PLATFORM="$ANDROID_PLATFORM" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG -g0" \
    -S .

cmake --build build-android --config "$BUILD_TYPE" -j $(nproc)

$ANDROID_NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip --strip-all ./build-android/Litematic_V7_To_V6_DynamicLibrary/libLitematic_V7_To_V6_DynamicLibrary.so

mkdir -p artifacts/android-arm64
cp -f ./build-android/Litematic_V7_To_V6_DynamicLibrary/Litematic_V7_To_V6_DynamicLibrary.so artifacts/android-arm64/
