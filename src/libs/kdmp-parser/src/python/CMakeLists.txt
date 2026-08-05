#
# This file is part of kdmp-parser project
#
# Released under MIT License, by 0vercl0k - 2023
#
# With contributions from:
# * masthoon - (github.com/masthoon)
# * hugsy - (github.com/hugsy)
#
cmake_minimum_required(VERSION 3.20)

project(
    kdmp-parser-python
    DESCRIPTION "A Cross-Platform C++ parser library for Windows kernel minidumps."
    HOMEPAGE_URL https://github.com/0vercl0k/kdmp-parser
    VERSION 0.7.4
)
set(PROJECT_AUTHOR 0vercl0k)
set(PROJECT_LICENSE MIT)
set(CMAKE_CXX_STANDARD 20)

find_package(Python 3
    REQUIRED COMPONENTS Interpreter Development.Module
    OPTIONAL_COMPONENTS Development.SABIModule
)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

set(KDMPPARSER_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(KDMPPARSER_INCLUDE_DIR "${KDMPPARSER_ROOT}/src/lib")
configure_file(${KDMPPARSER_INCLUDE_DIR}/kdmp-parser-version.h.in ${KDMPPARSER_INCLUDE_DIR}/kdmp-parser-version.h)

execute_process(
    COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
    OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE NB_DIR)
list(APPEND CMAKE_PREFIX_PATH "${NB_DIR}")

find_package(nanobind CONFIG REQUIRED)

nanobind_add_module(_kdmp_parser STABLE_ABI src/kdmp_parser.cc)

if(BUILD_PYTHON_PACKAGE)
    #
    # Those directives are only used when creating a standalone `kdmp_parser` python package
    # Usual case is when build with `pip install src/python`
    #
    target_include_directories(_kdmp_parser PRIVATE ${KDMPPARSER_INCLUDE_DIR})
    install(DIRECTORY kdmp_parser DESTINATION .)
    install(TARGETS _kdmp_parser LIBRARY DESTINATION kdmp_parser)
else()
    #
    # This is the general case, when built from the root cmakefile
    #
    target_include_directories(_kdmp_parser PRIVATE $<TARGET_PROPERTY:kdmp-parser,INTERFACE_INCLUDE_DIRECTORIES>)
    install(TARGETS _kdmp_parser DESTINATION bindings/python)

    if(MSVC)
        install(FILES $<TARGET_PDB_FILE:_kdmp_parser> DESTINATION . OPTIONAL)
    endif(MSVC)
endif()
