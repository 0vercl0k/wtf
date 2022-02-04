// Axel '0vercl0k' Souchet - July 10 2021
#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <array>
#include <cstdio>

int main() {
  HANDLE H =
      CreateFileA(R"(\\.\GLOBALROOT\Device\HackSysExtremeVulnerableDriver)",
                  GENERIC_ALL, 0, nullptr, OPEN_EXISTING, 0, nullptr);
  if (H == INVALID_HANDLE_VALUE) {
    printf("CreateFileA failed, bailing.\n");
    return EXIT_FAILURE;
  }

  std::array<uint8_t, 1024> Buffer;
  if (getenv("BREAK") != nullptr) {
    __debugbreak();
  }

  DWORD Returned = 0;
  DeviceIoControl(H, 0xdeadbeef, Buffer.data(), Buffer.size(), Buffer.data(),
                  Buffer.size(), &Returned, nullptr);
  CloseHandle(H);
  return EXIT_SUCCESS;
}