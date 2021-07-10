# Axel '0vercl0k' Souchet - April 18 2020
# CMakeList.txt : CMake project for kdmp-parser, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_POSITION_INDEPENDENT_CODE True)

OPTION(BUILD_TESTS "Build and run tests" OFF)

project(kdmp-parser)
add_compile_options(
    $<$<CXX_COMPILER_ID:MSVC>:/W4$<SEMICOLON>/WX>
    $<$<CXX_COMPILER_ID:GNU>:-Wall$<SEMICOLON>-Wextra$<SEMICOLON>-pedantic$<SEMICOLON>-Werror>
    $<$<CXX_COMPILER_ID:Clang>:-Wall$<SEMICOLON>-Wextra$<SEMICOLON>-pedantic$<SEMICOLON>-Werror>
)

add_subdirectory(src/lib)
add_subdirectory(src/parser)
add_subdirectory(src/testapp)
add_subdirectory(src/python)
if(BUILD_TESTS)
    add_subdirectory(src/tests)
endif()
