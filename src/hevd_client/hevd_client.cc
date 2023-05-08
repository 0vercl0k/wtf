// Axel '0vercl0k' Souchet - July 10 2021
#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <array>
#include <cstdio>

int main(int argc, char *argv[]) {
  HANDLE H =
      CreateFileA(R"(\\.\GLOBALROOT\Device\HackSysExtremeVulnerableDriver)",
                  GENERIC_ALL, 0, nullptr, OPEN_EXISTING, 0, nullptr);
  if (H == INVALID_HANDLE_VALUE) {
    printf("CreateFileA failed, bailing.\n");
    return EXIT_FAILURE;
  }

  std::array<uint8_t, 1024> BufferBacking = {};
  if (getenv("BREAK") != nullptr) {
    __debugbreak();
  }

  DWORD Returned = 0;
  DWORD IoctlCode = 0xdeadbeef;
  PVOID Buffer = BufferBacking.data();
  size_t BufferSize = BufferBacking.size();
  if (argc > 1) {
    FILE *File = fopen(argv[1], "rb");
    if (File == nullptr) {
      printf("fopen failed, bailing.\n");
      return EXIT_FAILURE;
    }

    if (fseek(File, 0, SEEK_END) != 0) {
      printf("fseek to the end failed, bailing.\n");
      fclose(File);
      return EXIT_FAILURE;
    }

    const long R = ftell(File);
    if (R == -1 || R < 0) {
      printf("ftell failed, bailing.\n");
      fclose(File);
      return EXIT_FAILURE;
    }

    const auto TotalSize = uint32_t(R);
    if (fseek(File, 0, SEEK_SET) != 0) {
      printf("fseek back to the beginning failed, bailing.\n");
      fclose(File);
      return EXIT_FAILURE;
    }

    if (fread(&IoctlCode, sizeof(IoctlCode), 1, File) != 1) {
      printf("fread failed when reading ioctl code, bailing.\n");
      fclose(File);
      return EXIT_FAILURE;
    }

    BufferSize = TotalSize - sizeof(uint32_t);
    Buffer = calloc(BufferSize, 1);
    if (Buffer == nullptr) {
      printf("calloc failed, bailing.\n");
      fclose(File);
      return EXIT_FAILURE;
    }

    if (fread(Buffer, BufferSize, 1, File) != 1) {
      printf("fread failed when reading buffer, bailing.\n");
      fclose(File);
      free(Buffer);
      return EXIT_FAILURE;
    }

    fclose(File);
  }

  DeviceIoControl(H, IoctlCode, Buffer, BufferSize, Buffer, BufferSize,
                  &Returned, nullptr);
  CloseHandle(H);

  if (Buffer != BufferBacking.data()) {
    free(Buffer);
  }

  return EXIT_SUCCESS;
}