cmake_minimum_required(VERSION 3.16)

# Set policy to avoid cmake warning. Remove when using CMake version >= 3.26
cmake_policy(SET CMP0135 NEW)
include(FetchContent)

set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "" FORCE)

project(scivec)
set(CMAKE_CXX_STANDARD 20)

add_executable(scivec
    src/image.cpp
    src/main.cpp
    src/palette.cpp
    src/scipicparser.cpp
    src/scipicvectorizer.cpp
    src/scipicencoder.cpp
)

target_link_libraries(scivec PRIVATE
    pthread
    m
)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_link_libraries(scivec PRIVATE
        "-framework CoreFoundation"
        "-framework OpenGL"
        "-framework Cocoa"
    )
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    target_link_libraries(scivec PRIVATE
        GLU
        GL
        X11
    )
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    target_link_libraries(scivec PRIVATE
        opengl32
        gdi32
        winmm
    )
    target_link_options(scivec PRIVATE "-static")
endif()

# TIGR
FetchContent_Declare(
    tigr
    GIT_REPOSITORY https://github.com/erkkah/tigr
    GIT_TAG "v3.1.0"
)

# incbin
FetchContent_Declare(
    incbin
    GIT_REPOSITORY https://github.com/graphitemaster/incbin
    GIT_TAG "main"
)

FetchContent_MakeAvailable(tigr incbin)

set(TIGR ${tigr_SOURCE_DIR})
set(INCBIN ${incbin_SOURCE_DIR})

target_include_directories(scivec PUBLIC
    ${TIGR}
    ${INCBIN}
    ${CMAKE_SOURCE_DIR}/ext
)

target_sources(scivec PUBLIC
    ${TIGR}/tigr.c
)
