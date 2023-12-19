# Axel '0vercl0k' Souchet - April 18 2020
enable_testing()

#
# Build and link Catch2 library dependency automatically if not already found
#
include(FetchContent)
FetchContent_Declare(
    Catch2
    URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.4.0.zip
    URL_HASH MD5=c426e77d4ee0055410bc930182959ae5
)
FetchContent_MakeAvailable(Catch2)

#
# Build all test files
#
list(APPEND TEST_SOURCE_FILES
    tests_version.cc
    tests_parser.cc
)

foreach(SOURCE_FILE ${TEST_SOURCE_FILES})
    cmake_path(GET SOURCE_FILE STEM TEST_EXECUTABLE_NAME)
    add_executable(${TEST_EXECUTABLE_NAME} ${SOURCE_FILE})
    target_link_libraries(${TEST_EXECUTABLE_NAME} PUBLIC kdmp-parser Catch2::Catch2WithMain)
    add_test(NAME ${TEST_EXECUTABLE_NAME} COMMAND $<TARGET_FILE:${TEST_EXECUTABLE_NAME}>)
    install(FILES $<TARGET_FILE:${TEST_EXECUTABLE_NAME}> DESTINATION tests)
endforeach()
