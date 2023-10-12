# Axel '0vercl0k' Souchet - April 27 2020
cmake_minimum_required(VERSION 3.16.0)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

project(wtf)
include_directories(${CMAKE_CURRENT_LIST_DIR}/libs/bochscpu-bins/include)
include_directories(${CMAKE_CURRENT_LIST_DIR}/libs/robin-map/include)
include_directories(${CMAKE_CURRENT_LIST_DIR}/libs/kdmp-parser/src/lib)
include_directories(${CMAKE_CURRENT_LIST_DIR}/libs/libfuzzer)
include_directories(${CMAKE_CURRENT_LIST_DIR}/libs/readerwriterqueue)
include_directories(${CMAKE_CURRENT_LIST_DIR}/libs/json/single_include)
include_directories(${CMAKE_CURRENT_LIST_DIR}/libs/BLAKE3/c)
include_directories(${CMAKE_CURRENT_LIST_DIR}/libs/CLI11/include)
include_directories(${CMAKE_CURRENT_LIST_DIR}/libs/fmt/include)
include_directories(${CMAKE_CURRENT_LIST_DIR}/libs/yas/include)

file(
    GLOB_RECURSE
    wtf_srcfiles
    wtf/*.cc
)

file(
    GLOB_RECURSE
    kdmp_srcfiles
    ${CMAKE_CURRENT_LIST_DIR}/libs/kdmp-parser/src/lib/*.cc
)

file(
    GLOB_RECURSE
    libfuzzer_srcfiles
    ${CMAKE_CURRENT_LIST_DIR}/libs/libfuzzer/*.cpp
)

set(
    blake3_srcfiles
    ${CMAKE_CURRENT_LIST_DIR}/libs/BLAKE3/c/blake3.c
    ${CMAKE_CURRENT_LIST_DIR}/libs/BLAKE3/c/blake3_dispatch.c
    ${CMAKE_CURRENT_LIST_DIR}/libs/BLAKE3/c/blake3_portable.c
)

add_executable(
    wtf
    ${wtf_srcfiles}
    ${kdmp_srcfiles}
    ${libfuzzer_srcfiles}
    ${blake3_srcfiles}
)

add_compile_definitions(FMT_HEADER_ONLY)

if (WIN32)
    enable_language(ASM_MASM)
    # This is for winsock / windows.h; without this windows.h imports socket.
    # https://docs.microsoft.com/en-us/windows/win32/iphlp/creating-a-basic-ip-helper-application
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
    target_sources(
        wtf
        PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/libs/BLAKE3/c/blake3_sse41_x86-64_windows_msvc.asm
        ${CMAKE_CURRENT_LIST_DIR}/libs/BLAKE3/c/blake3_sse2_x86-64_windows_msvc.asm
        ${CMAKE_CURRENT_LIST_DIR}/libs/BLAKE3/c/blake3_avx2_x86-64_windows_msvc.asm
        ${CMAKE_CURRENT_LIST_DIR}/libs/BLAKE3/c/blake3_avx512_x86-64_windows_msvc.asm
    )

    # https://docs.microsoft.com/en-us/cpp/build/reference/zc-cplusplus
    target_compile_options(
        wtf
        PRIVATE
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
        /NODEFAULTLIB:libcmt
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

    add_executable(
        fuzzy_goat
        fuzzy_goat/fuzzy_goat.cc
    )
else (WIN32)
    enable_language(ASM)
    find_package(Threads)
    target_sources(
        wtf
        PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/libs/BLAKE3/c/blake3_sse41_x86-64_unix.S
        ${CMAKE_CURRENT_LIST_DIR}/libs/BLAKE3/c/blake3_sse2_x86-64_unix.S
        ${CMAKE_CURRENT_LIST_DIR}/libs/BLAKE3/c/blake3_avx2_x86-64_unix.S
        ${CMAKE_CURRENT_LIST_DIR}/libs/BLAKE3/c/blake3_avx512_x86-64_unix.S
    )

    target_link_libraries(
        wtf
        ${CMAKE_THREAD_LIBS_INIT}
        dl
    )
endif(WIN32)

target_precompile_headers(
    wtf
    PRIVATE
    wtf/pch.h
)

target_link_libraries(
    wtf
    ${CMAKE_CURRENT_LIST_DIR}/libs/bochscpu-bins/lib/${CMAKE_SHARED_MODULE_PREFIX}bochscpu_ffi${CMAKE_STATIC_LIBRARY_SUFFIX}
)
