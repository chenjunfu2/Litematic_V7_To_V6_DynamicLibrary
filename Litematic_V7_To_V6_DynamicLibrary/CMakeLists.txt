cmake_minimum_required(VERSION 3.13)
project(Litematic_V7_To_V6_DynamicLibrary LANGUAGES CXX)

add_library(Litematic_V7_To_V6_DynamicLibrary SHARED
    $<$<PLATFORM_ID:Windows>:dllmain.cpp>
    JNI_Exports.cpp
    Litematic_V7_To_V6.cpp
)

if(WIN32)
    set(JVM_LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/jni/lib/jvm.lib)
elseif(UNIX AND NOT APPLE)
    set(JVM_LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/jni/lib/libjvm.so)
endif()

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        ${JVM_LIB_PATH}
)
