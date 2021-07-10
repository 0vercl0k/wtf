# Mastho - 2020
find_package(Python3 COMPONENTS Interpreter Development)

message("Python3_Interpreter_FOUND:${Python3_Interpreter_FOUND}")
message("Python3_Development_FOUND:${Python3_Development_FOUND}")
message("Python3_LIBRARY_DIRS:${Python3_LIBRARY_DIRS}")

# Python on Windows Debug build only load $_d.pyd and require python_d.exe
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND WIN32)
    set(PYMODULE kdmp_d CACHE INTERNAL "")
else()
    set(PYMODULE kdmp CACHE INTERNAL "")
endif()

# Require Python3 interpreter and development libs
if(NOT Python3_Interpreter_FOUND OR (NOT Python3_Development_FOUND) OR (CMAKE_BUILD_TYPE STREQUAL "Debug" AND WIN32 AND NOT Python3_LIBRARY_DEBUG))
    message(WARNING "Python3 development library not found")
    set(PYMODULE "" CACHE INTERNAL "")
    return()
endif()

add_library(${PYMODULE} SHARED python-kdmp.cc)
target_include_directories(${PYMODULE} PRIVATE ${Python3_INCLUDE_DIRS})
target_link_directories(${PYMODULE} PRIVATE ${Python3_LIBRARY_DIRS} kdmp-parser)
target_link_libraries(${PYMODULE} PRIVATE kdmp-parser)

# On Windows Python load $.pyd :: On Linux remove lib$ prefix
if(WIN32)
    set_target_properties(${PYMODULE} PROPERTIES SUFFIX ".pyd")
else()
    set_target_properties(${PYMODULE} PROPERTIES PREFIX "")
endif()

