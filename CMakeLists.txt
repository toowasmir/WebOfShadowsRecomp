# wos_recomp - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(wos_recomp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# Sources
set(WOSRECOMP_SOURCES
    src/main.cpp
)

if(WIN32)
    add_executable(wos_recomp WIN32 ${WOSRECOMP_SOURCES})
else()
    add_executable(wos_recomp ${WOSRECOMP_SOURCES})
endif()

rexglue_setup_target(wos_recomp GPU_PLUGINS xenos)
target_compile_options(wos_recomp PRIVATE ${REXGLUE_RECOMP_OPTIONS})

