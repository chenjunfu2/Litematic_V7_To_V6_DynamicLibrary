cmake_minimum_required(VERSION 3.13)
project(Litematic_V7_To_V6_DynamicLibrary LANGUAGES CXX)

add_library(Litematic_V7_To_V6_DynamicLibrary SHARED
    $<$<PLATFORM_ID:Windows>:dllmain.cpp>
    JNI_Exports.cpp
    Litematic_V7_To_V6.cpp
)

if(WIN32)
    set(JAVA_JVM_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/jni/lib/jvm.lib)
    set(JNI_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/jni/include)
elseif(UNIX AND NOT APPLE)
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
