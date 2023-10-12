// Theodor Arsenij 'm4drat' - May 23 2023

#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <array>
#include <cstdio>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint32_t FuzzingCoverageFeedbackTest(const char *buffer, uint32_t size) {
  if (size < 10) {
    return 0;
  }

  if (buffer[0] == 'F') {
    if (buffer[1] == 'U') {
      if (buffer[2] == 'Z') {
        if (buffer[3] == 'Z') {
          if (buffer[4] == 'I') {
            if (buffer[5] == 'N') {
              if (buffer[6] == 'G') {
                if (buffer[7] == '!') {
                  abort();
                }
              }
            }
          }
        }
      }
    }
  }

  return 1;
}

uint32_t FuzzingCompcovLafTest(const char *buffer, uint32_t size) {
  if (size < 112) {
    return 0;
  }

  // Check if we can solve 8-byte comparison.
  if (*(uint64_t *)buffer != 0x13375612DEADBEEF) {
    return 1;
  }
  buffer += sizeof(uint64_t);

  // Check if we can solve 4-byte comparison.
  if (*(uint32_t *)buffer != 0xDEADBEEF) {
    return 2;
  }
  buffer += sizeof(uint32_t);

  // Check if we can solve 2-byte comparison.
  if (*(uint16_t *)buffer != 0x1337) {
    return 3;
  }
  buffer += sizeof(uint16_t);

  // strcmp.
  const char str1[] = "Never gonna";
  // Here, and below we're calling these functions using volatile function
  // pointers. This way, the compiler should emit a library-call instead of
  // inlining the whole function.
  int (*volatile strcmp_ptr)(const char *_Str1, const char *_Str2) = strcmp;
  if (strcmp_ptr(buffer, str1) != 0) {
    return 4;
  }
  buffer += sizeof(str1);

  // strncmp.
  const char str2[] = "give you up";
  int (*volatile strncmp_ptr)(const char *_Str1, const char *_Str2,
                              size_t _MaxCount) = strncmp;
  if (strncmp_ptr(buffer, str2, sizeof(str2)) != 0) {
    return 5;
  }
  buffer += sizeof(str2);

  // memcmp.
  const char str3[] = "run around";
  int (*volatile memcmp_ptr)(const void *_Buf1, const void *_Buf2,
                             size_t _Size) = memcmp;
  if (memcmp_ptr(buffer, str3, sizeof(str3)) != 0) {
    return 6;
  }
  buffer += sizeof(str3);

  // CompareStringA.
  const char str4[] = "desert you";
  // Here, and below we're calling these functions using volatile function
  // pointers. This way, the compiler should emit a library-call instead of
  // inlining the whole function.
  int (*volatile compare_string_a_ptr)(
      LCID Locale, DWORD dwCmpFlags, PCNZCH lpString1, int cchCount1,
      PCNZCH lpString2, int cchCount) = CompareStringA;
  if (compare_string_a_ptr(LOCALE_USER_DEFAULT, 0, str4, sizeof(str4) - 1,
                           buffer, sizeof(str4) - 1) != CSTR_EQUAL) {
    return 1;
  }
  buffer += sizeof(str4);

  // CompareStringW.
  const wchar_t str5[] = L"make you cry";
  int (*volatile compare_string_w_ptr)(
      LCID Locale, DWORD dwCmpFlags, PCNZWCH lpString1, int cchCount1,
      PCNZWCH lpString2, int cchCount) = CompareStringW;
  if (compare_string_w_ptr(LOCALE_USER_DEFAULT, 0, str5, sizeof(str5) / 2 - 1,
                           (const wchar_t *)buffer,
                           sizeof(str5) / 2 - 1) != CSTR_EQUAL) {
    return 2;
  }

  abort();
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Usage: %s <test-mode> (compcov-laf, bb-coverage)\n", argv[0]);
    return 1;
  }

  std::array<uint8_t, 1024> Buffer;

  // This is a volatile call to memset, to force load the dll.
  volatile uint64_t res = memcmp(Buffer.data(), Buffer.data(), Buffer.size());

  // This is a volatile call to CompareStringA, to force load the dll.
  volatile uint64_t res1 =
      CompareStringA(LOCALE_USER_DEFAULT, 0, "a", 1, "b", 1);

  // This is a volatile call to CompareStringA, to force load the dll.
  volatile uint64_t res2 =
      CompareStringW(LOCALE_USER_DEFAULT, 0, L"a", 1, L"b", 1);

  // Allows us to execute the lockmem.exe.
  std::getc(stdin);

  if (getenv("BREAK") != nullptr) {
    __debugbreak();
  }

  if (strcmp(argv[1], "bb-coverage") == 0) {
    return FuzzingCoverageFeedbackTest((const char *)Buffer.data(),
                                       Buffer.size());
  } else if (strcmp(argv[1], "compcov-laf") == 0) {
    return FuzzingCompcovLafTest((const char *)Buffer.data(), Buffer.size());
  } else {
    printf("Usage: %s <test-mode> (compcov-laf, bb-coverage)\n", argv[0]);
    return 1;
  }

  return EXIT_SUCCESS;
}