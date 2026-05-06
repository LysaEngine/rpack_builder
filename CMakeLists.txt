#
# Copyright (c) 2025-present Henri Michelon
#
# This software is released under the MIT License.
# https://opensource.org/licenses/MIT
#
cmake_minimum_required(VERSION 3.16)
project(rpack_builder CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
        cxxopts
        GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
        GIT_TAG        v3.2.0
)
FetchContent_MakeAvailable(cxxopts)

add_executable(rpack_builder "src/rpack_builder.cpp")
target_link_libraries(rpack_builder PRIVATE cxxopts::cxxopts)

if (MSVC)
    target_compile_options(rpack_builder PRIVATE /W4)
else()
    target_compile_options(rpack_builder PRIVATE -Wall -Wextra -Wpedantic)
endif()
