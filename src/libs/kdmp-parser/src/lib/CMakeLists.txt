# Axel '0vercl0k' Souchet - April 18 2020
add_library(kdmp-parser INTERFACE)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/kdmp-parser-version.h.in ${CMAKE_CURRENT_SOURCE_DIR}/kdmp-parser-version.h)
target_include_directories(kdmp-parser INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
