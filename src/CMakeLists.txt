# Axel '0vercl0k' Souchet - April 27 2020
cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

project(wtf)
set(WTF_LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs")

# Reject unsupported architectures early.
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" WTF_SYSTEM_PROCESSOR)
if(WTF_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64)$")
    set(WTF_ARCH x86_64)
elseif(WTF_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    set(WTF_ARCH arm64)
else()
    message(FATAL_ERROR "Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

# `ld: warning: object file (/Users/runner/work/wtf/wtf/src/libs/bochscpu-bins/lib/libbochscpu_ffi_Darwin_arm64.a[2](bochscpu_ffi.bochscpu_ffi.2bcf5603cd78585e-cgu.0.rcgu.o)) was built for newer 'macOS' version (26.3.1) than being linked (26.0)`
if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    execute_process(
        COMMAND sw_vers -productVersion
        OUTPUT_VARIABLE CMAKE_OSX_DEPLOYMENT_TARGET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()

# bxcpu-ffi
if(WIN32)
    if(NOT MSVC)
        message(FATAL_ERROR "The Windows build requires MSVC")
    elseif(NOT WTF_ARCH STREQUAL "x86_64")
        message(FATAL_ERROR "The Windows build only supports x86_64")
    endif()
    set(BOCHSCPU_LIBRARY "${WTF_LIBS_DIR}/bochscpu-bins/lib/bochscpu_ffi_Windows_AMD64.lib")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(WTF_ARCH STREQUAL "x86_64")
        set(BOCHSCPU_LIBRARY "${WTF_LIBS_DIR}/bochscpu-bins/lib/libbochscpu_ffi_Linux_x86_64.a")
    else()
        set(BOCHSCPU_LIBRARY "${WTF_LIBS_DIR}/bochscpu-bins/lib/libbochscpu_ffi_Linux_aarch64.a")
    endif()
elseif(APPLE)
    if(NOT WTF_ARCH STREQUAL "arm64")
        message(FATAL_ERROR "The macOS build only supports arm64")
    endif()
    set(BOCHSCPU_LIBRARY "${WTF_LIBS_DIR}/bochscpu-bins/lib/libbochscpu_ffi_Darwin_arm64.a")
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

add_library(bochscpu_ffi STATIC IMPORTED)
set_target_properties(
    bochscpu_ffi
    PROPERTIES
        IMPORTED_LOCATION "${BOCHSCPU_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${WTF_LIBS_DIR}/bochscpu-bins/include"
)

# blake3
set(
    blake3_srcfiles
    ${WTF_LIBS_DIR}/BLAKE3/c/blake3.c
    ${WTF_LIBS_DIR}/BLAKE3/c/blake3_dispatch.c
    ${WTF_LIBS_DIR}/BLAKE3/c/blake3_portable.c
)

if(WIN32)
    list(
        APPEND
        blake3_srcfiles
        "${WTF_LIBS_DIR}/BLAKE3/c/blake3_sse2_x86-64_windows_msvc.asm"
        "${WTF_LIBS_DIR}/BLAKE3/c/blake3_sse41_x86-64_windows_msvc.asm"
        "${WTF_LIBS_DIR}/BLAKE3/c/blake3_avx2_x86-64_windows_msvc.asm"
        "${WTF_LIBS_DIR}/BLAKE3/c/blake3_avx512_x86-64_windows_msvc.asm"
     )
elseif(WTF_ARCH STREQUAL "x86_64")
    enable_language(ASM)
    list(
        APPEND
        blake3_srcfiles
        "${WTF_LIBS_DIR}/BLAKE3/c/blake3_sse2_x86-64_unix.S"
        "${WTF_LIBS_DIR}/BLAKE3/c/blake3_sse41_x86-64_unix.S"
        "${WTF_LIBS_DIR}/BLAKE3/c/blake3_avx2_x86-64_unix.S"
     )
endif()

add_library(blake3 STATIC ${blake3_srcfiles})
target_include_directories(
    blake3
    PUBLIC
    ${WTF_LIBS_DIR}/BLAKE3/c
)

# kdmp-parser
add_subdirectory(${WTF_LIBS_DIR}/kdmp-parser/)

# libfuzzer
file(
    GLOB_RECURSE
    libfuzzer_srcfiles
    ${WTF_LIBS_DIR}/libfuzzer/*.cpp
)

# wtf
file(
    GLOB_RECURSE
    wtf_srcfiles
    wtf/*.cc
)

add_executable(
    wtf
    ${wtf_srcfiles}
    ${libfuzzer_srcfiles}
)

target_compile_definitions(
    wtf
    PRIVATE
    FMT_HEADER_ONLY
)

if(WTF_ARCH STREQUAL "arm64")
    target_compile_definitions(
        blake3
        PRIVATE
        BLAKE3_USE_NEON=1
        # bochscpu_ffi also exports this symbol on arm64.
        blake3_compress_in_place_portable=wtf_blake3_compress_in_place_portable
    )
endif()

target_include_directories(
    wtf
    PRIVATE
    ${WTF_LIBS_DIR}/robin-map/include
    ${WTF_LIBS_DIR}/libfuzzer
    ${WTF_LIBS_DIR}/readerwriterqueue
    ${WTF_LIBS_DIR}/json/single_include
    ${WTF_LIBS_DIR}/CLI11/include
    ${WTF_LIBS_DIR}/fmt/include
    ${WTF_LIBS_DIR}/yas/include
)

target_precompile_headers(
    wtf
    PRIVATE
    wtf/pch.h
)

set(WTF_PLATFORM_LIBRARIES)
if(WIN32)
    set_property(TARGET wtf blake3 PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
    target_compile_definitions(
        wtf
        PRIVATE
        NOMINMAX
        # This is for winsock / windows.h; without this windows.h imports socket.
        # https://docs.microsoft.com/en-us/windows/win32/iphlp/creating-a-basic-ip-helper-application
        WIN32_LEAN_AND_MEAN
    )

    target_compile_options(
        wtf
        PRIVATE
        # /utf-8 for fmt
        "$<$<COMPILE_LANGUAGE:CXX>:/utf-8>"
        # https://docs.microsoft.com/en-us/cpp/build/reference/zc-cplusplus
        "$<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>"
    )

    # Delay loading dbgeng.dll:
    #   If the user doesn't have dbghelp/symsrv.dll there is code to find a copy of the
    #   dlls on the system and copy them in the directory and then carrying on and
    #   trying to use the apis. If we don't delayload, dbgeng will be already loaded and
    #   already resolved dbghelp/symsrv.dll. The new dlls will get picked up if the program
    #   is retarded. Delay loading the dll means that we don't have to restart it!
    target_link_options(
        wtf
        PRIVATE
        /DELAYLOAD:dbgeng.dll
    )

    list(
        APPEND
        WTF_PLATFORM_LIBS
        ws2_32.lib
        userenv.lib
        dbgeng.lib
        winhvplatform.lib
        delayimp.lib
        bcrypt.lib
        ntdll.lib
    )

    add_executable(
        tlv_server
        tlv_server/tlv_server.cc
    )

    target_compile_definitions(
        tlv_server
        PRIVATE
        # This is for winsock / windows.h; without this windows.h imports socket.
        # https://docs.microsoft.com/en-us/windows/win32/iphlp/creating-a-basic-ip-helper-application
        WIN32_LEAN_AND_MEAN
    )

    add_executable(
        hevd_client
        hevd_client/hevd_client.cc
    )
else()
    find_package(Threads)
    list(
        APPEND
        WTF_PLATFORM_LIBS
        Threads::Threads
    )

endif()

target_link_libraries(
    wtf
    blake3
    bochscpu_ffi
    kdmp-parser
    ${WTF_PLATFORM_LIBS}
)
