# Axel '0vercl0k' Souchet - April 27 2020
cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

project(wtf)
set(WTF_LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs")

include_directories(${WTF_LIBS_DIR}/bochscpu-bins/include)
include_directories(${WTF_LIBS_DIR}/robin-map/include)
add_subdirectory(${WTF_LIBS_DIR}/kdmp-parser/)
include_directories(${WTF_LIBS_DIR}/libfuzzer)
include_directories(${WTF_LIBS_DIR}/readerwriterqueue)
include_directories(${WTF_LIBS_DIR}/json/single_include)
include_directories(${WTF_LIBS_DIR}/BLAKE3/c)
include_directories(${WTF_LIBS_DIR}/CLI11/include)
include_directories(${WTF_LIBS_DIR}/fmt/include)
include_directories(${WTF_LIBS_DIR}/yas/include)

file(
    GLOB_RECURSE
    wtf_srcfiles
    wtf/*.cc
)

file(
    GLOB_RECURSE
    libfuzzer_srcfiles
    ${WTF_LIBS_DIR}/libfuzzer/*.cpp
)

add_library(
    blake3 STATIC
    ${WTF_LIBS_DIR}/BLAKE3/c/blake3.c
    ${WTF_LIBS_DIR}/BLAKE3/c/blake3_dispatch.c
    ${WTF_LIBS_DIR}/BLAKE3/c/blake3_portable.c
)

add_executable(
    wtf
    ${wtf_srcfiles}
    ${libfuzzer_srcfiles}
)

add_compile_definitions(FMT_HEADER_ONLY)

# Reject unsupported architectures early.
if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64|x86_64|AMD64")
    message(FATAL_ERROR "Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

if(WIN32)
    set_property(TARGET wtf PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
    enable_language(ASM_MASM)
    # This is for winsock / windows.h; without this windows.h imports socket.
    # https://docs.microsoft.com/en-us/windows/win32/iphlp/creating-a-basic-ip-helper-application
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
    target_sources(
        wtf
        PRIVATE
        ${WTF_LIBS_DIR}/BLAKE3/c/blake3_sse41_x86-64_windows_msvc.asm
        ${WTF_LIBS_DIR}/BLAKE3/c/blake3_sse2_x86-64_windows_msvc.asm
        ${WTF_LIBS_DIR}/BLAKE3/c/blake3_avx2_x86-64_windows_msvc.asm
        ${WTF_LIBS_DIR}/BLAKE3/c/blake3_avx512_x86-64_windows_msvc.asm
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

    target_link_libraries(
        wtf
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

    add_executable(
        hevd_client
        hevd_client/hevd_client.cc
    )
elseif(UNIX)
    enable_language(ASM)
    find_package(Threads)
    target_link_libraries(wtf ${CMAKE_THREAD_LIBS_INIT})
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_libraries(wtf dl)
    endif()

    # `ld: warning: object file (/Users/runner/work/wtf/wtf/src/libs/bochscpu-bins/lib/libbochscpu_ffi_Darwin_arm64.a[2](bochscpu_ffi.bochscpu_ffi.2bcf5603cd78585e-cgu.0.rcgu.o)) was built for newer 'macOS' version (26.3.1) than being linked (26.0)`
    if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        execute_process(
            COMMAND sw_vers -productVersion
            OUTPUT_VARIABLE CMAKE_OSX_DEPLOYMENT_TARGET
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    endif()

    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
        target_sources(
            wtf
            PRIVATE
            ${WTF_LIBS_DIR}/BLAKE3/c/blake3_sse41_x86-64_unix.S
            ${WTF_LIBS_DIR}/BLAKE3/c/blake3_sse2_x86-64_unix.S
            ${WTF_LIBS_DIR}/BLAKE3/c/blake3_avx2_x86-64_unix.S

            # libbochscpu_ffi.a already links this so will lead to multiple definition errors like in the below:
            #    ```
            #   `blake3_compress_in_place_avx512':
            #   (.text+0x3780): multiple definition of `_blake3_compress_in_place_avx512'; CMakeFiles/wtf.dir/libs/BLAKE3/c/blake3_avx512_x86-64_unix.S.o:(.text+0x3740): first defined here
            #   /usr/bin/ld: /home/runner/work/wtf/wtf/src/libs/bochscpu-bins/lib/libbochscpu_ffi.a(b8423798394d5395-blake3_avx512_x86-64_unix.o)
            #   ```
            # ${WTF_LIBS_DIR}/BLAKE3/c/blake3_avx512_x86-64_unix.S
        )
    endif()
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
    target_compile_definitions(blake3 PRIVATE BLAKE3_USE_NEON=1)
    # libbochscpu_ffi.a bundles blake3_compress_in_place_portable.
    # Rename our copy to avoid duplicate symbols at link time.
    target_compile_definitions(blake3 PRIVATE
        blake3_compress_in_place_portable=_wtf_blake3_compress_in_place_portable
    )
endif()

target_precompile_headers(
    wtf
    PRIVATE
    wtf/pch.h
)

target_link_libraries(
    wtf
    blake3
    ${WTF_LIBS_DIR}/bochscpu-bins/lib/${CMAKE_SHARED_MODULE_PREFIX}bochscpu_ffi_${CMAKE_SYSTEM_NAME}_${CMAKE_SYSTEM_PROCESSOR}${CMAKE_STATIC_LIBRARY_SUFFIX}
    kdmp-parser
)
