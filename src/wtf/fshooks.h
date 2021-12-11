// Axel '0vercl0k' Souchet - March 19 2020
#pragma once
#include "globals.h"
#include <cstdio>
#include <fmt/format.h>

constexpr bool FsHooksLoggingOn = false;

template <typename... Args_t>
void FsDebugPrint(const char *Format, const Args_t &...args) {
  if constexpr (FsHooksLoggingOn) {
    fmt::print("fs: ");
    fmt::print(Format, args...);
  }
}

bool SetupFilesystemHooks();
