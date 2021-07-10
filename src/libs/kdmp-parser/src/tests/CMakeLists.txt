# Axel '0vercl0k' Souchet - July 25 2020
find_package(Python3 COMPONENTS Interpreter)
if(NOT Python3_Interpreter_FOUND)
    message(FATAL_ERROR "Python3 interpreter could not be located")
endif()

add_custom_target(
    NativeTests ALL
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests.py --bindir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} --pymodule ${PYMODULE}
    DEPENDS testapp ${PYMODULE}
)