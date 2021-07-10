// Axel '0vercl0k' Souchet - April 18 2020
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
#else
#error An error occured
#endif

#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0

#if defined ARCH_X86
#define LINUX_X86
#elif defined ARCH_X64
#define LINUX_X64
#endif

#else
#error Platform not supported.
#endif
