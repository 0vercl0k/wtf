// Axel '0vercl0k' Souchet - April 27 2020
#pragma once

#if defined(__i386__) || defined(_M_IX86)
#define ARCH_X86
#elif defined(__amd64__) || defined(_M_X64)
#define ARCH_X64
#else
#error Platform not supported.
#endif

#if defined(WIN32) || defined(WIN64) || defined(_WIN32) || defined(_WIN64)
#define WINDOWS
#define SYSTEM_PLATFORM "Windows"

#include <windows.h>
using ssize_t = SSIZE_T;
#define __builtin_bswap16 _byteswap_ushort
#define __builtin_bswap32 _byteswap_ulong
#define __builtin_bswap64 _byteswap_uint64
#define aligned_alloc(a, b) _aligned_malloc(a, b)
#define aligned_free(x) _aligned_free(x)
#if defined ARCH_X86
#define WINDOWS_X86
#elif defined ARCH_X64
#define WINDOWS_X64
#endif
#elif defined(linux) || defined(__linux) || defined(__FreeBSD__) ||            \
    defined(__FreeBSD_kernel__) || defined(__MACH__)
#define LINUX

#if defined(linux) || defined(__linux)
#define SYSTEM_PLATFORM "Linux"

#include <cstdlib>

#define __debugbreak() __asm__("int $3")
#define ExitProcess(x) exit(x)
#define aligned_free(x) free(x)

#else
#error An error occured
#endif

#if defined ARCH_X86
#define LINUX_X86
#elif defined ARCH_X64
#define LINUX_X64
#endif

#else
#error Platform not supported.
#endif