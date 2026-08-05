# Axel '0vercl0k' Souchet - April 18 2020
# CMakeList.txt : CMake project for kdmp-parser, include source and define
# project specific logic here.
#
cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_POSITION_INDEPENDENT_CODE True)

project(
    kdmp-parser
    DESCRIPTION "A Cross-Platform C++ parser library for Windows kernel minidumps."
    HOMEPAGE_URL https://github.com/0vercl0k/kdmp-parser
    VERSION 0.7.4
)

set(PROJECT_AUTHOR 0vercl0k)
set(PROJECT_LICENSE MIT)

option(BUILD_PARSER "Build the parser executable for kdmp-parser" ON)
option(BUILD_TESTS "Build the test suite for kdmp-parser" OFF)
option(BUILD_PYTHON_BINDING "Build the Python bindings for kdmp-parser" OFF)

add_subdirectory(src/lib)

if(BUILD_PARSER)
    add_subdirectory(src/parser)
endif(BUILD_PARSER)

if(BUILD_PYTHON_BINDING)
    add_subdirectory(src/python)
endif(BUILD_PYTHON_BINDING)

if(BUILD_TESTS)
    add_subdirectory(src/tests)
endif(BUILD_TESTS)
