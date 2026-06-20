cmake_minimum_required(VERSION 3.13)
project(Litematic_V7_To_V6_DynamicLibrary LANGUAGES CXX)

add_library(Litematic_V7_To_V6_DynamicLibrary SHARED
    $<$<PLATFORM_ID:Windows>:dllmain.cpp>
    JNI_Exports.cpp
)

if(WIN32)
    set_target_properties(Litematic_V7_To_V6_DynamicLibrary PROPERTIES
        PREFIX ""
    )
endif()

set_release_static_link(Litematic_V7_To_V6_DynamicLibrary)

if(WIN32)
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        set(JNI_ARCH "x64")
    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "ARM64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
        set(JNI_ARCH "ARM64")
    else()
        message(FATAL_ERROR "Unsupported Windows architecture: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()

    set(JAVA_JVM_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/jni/${JNI_ARCH}/lib/jvm.lib)
    set(JNI_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/jni/${JNI_ARCH}/include)
elseif(UNIX)
    find_package(JNI REQUIRED)
endif()

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${JNI_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        ${JAVA_JVM_LIBRARY}
)
