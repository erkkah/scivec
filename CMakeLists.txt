cmake_minimum_required(VERSION 3.16)

# Set policy to avoid cmake warning. Remove when using CMake version >= 3.26
cmake_policy(SET CMP0135 NEW)
include(FetchContent)

project(scivec)
set(CMAKE_CXX_STANDARD 20)

add_executable(scivec
    src/main.cpp
    src/scipic.cpp
)

target_link_libraries(scivec PRIVATE
    pthread
    m
)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

target_link_libraries(scivec PRIVATE
    "-framework CoreFoundation"
)

endif()

include(ExternalProject)

# TIGR

externalproject_add(tigr
    GIT_REPOSITORY https://github.com/erkkah/tigr
    GIT_TAG "v3.1.0"

    USES_TERMINAL_DOWNLOAD TRUE
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    BYPRODUCTS "<SOURCE_DIR>/tigr.c"
)

add_dependencies(scivec tigr)
ExternalProject_Get_property(tigr SOURCE_DIR)
set(TIGR ${SOURCE_DIR})

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_link_libraries(scivec PRIVATE
        "-framework OpenGL"
        "-framework Cocoa"
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

# incbin

externalproject_add(incbin
    GIT_REPOSITORY https://github.com/graphitemaster/incbin
    GIT_TAG "main"

    USES_TERMINAL_DOWNLOAD TRUE
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    BYPRODUCTS "<SOURCE_DIR>/incbin.c"
)

add_dependencies(scivec incbin)
ExternalProject_Get_property(incbin SOURCE_DIR)
set(INCBIN ${SOURCE_DIR})


target_include_directories(scivec PUBLIC
    ${TIGR}
    ${INCBIN}
)

target_sources(scivec PUBLIC
    ${TIGR}/tigr.c
)
