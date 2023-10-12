// Axel '0vercl0k' Souchet - February 26 2020
#pragma once
#include "backend.h"
#include "globals.h"
#include "nt.h"
#include <cstdint>
#include <filesystem>
#include <fmt/format.h>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <tsl/robin_map.h>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
namespace chrono = std::chrono;
using span_u8 = std::span<uint8_t>;

const size_t StringMaxSize = 120;

//
// Hashing functions.
//

inline uint64_t SplitMix64(uint64_t Val) {
  Val ^= Val >> 30;
  Val *= 0xbf58476d1ce4e5b9U;
  Val ^= Val >> 27;
  Val *= 0x94d049bb133111ebU;
  Val ^= Val >> 31;
  return Val;
}

//
// Compare two file path by their sizes.
//

[[nodiscard]] bool CompareTwoFileBySize(const fs::path &A, const fs::path &B);

//
// Bytes to hex string.
//

std::string BytesToHexString(const uint8_t *Bytes, uint32_t Length);

//
// Hexdump function.
//

void Hexdump(const span_u8 Buffer);
void Hexdump(const uint64_t Address, const span_u8 Buffer);
void Hexdump(const uint64_t Address, const void *Buffer, size_t Len);

//
// Parse LAF allowed ranges cmdline argument.
//

std::vector<std::pair<Gva_t, Gva_t>>
ParseLafAllowedRanges(const std::string &input);
//
// Populate a bochscpu_state_t from a JSON file.
//

bool LoadCpuStateFromJSON(CpuState_t &CpuState, const fs::path &CpuStatePath);

//
// Sanitize the Cpu state before running.
//

bool SanitizeCpuState(CpuState_t &CpuState);

[[nodiscard]] std::unique_ptr<uint8_t[]> ReadFile(const fs::path &Path,
                                                  size_t &FileSize);

[[nodiscard]] std::string Blake3HexDigest(const uint8_t *Data,
                                          const size_t DataSize);

class HostObjectAttributes_t {
  OBJECT_ATTRIBUTES HostObjectAttributes_;

public:
  HostObjectAttributes_t() {
    memset(&HostObjectAttributes_, 0, sizeof(HostObjectAttributes_));
  }

  ~HostObjectAttributes_t() {
    //
    // If we have allocated memory for the UNICODE_STRING string buffer, it's
    // time to release it.
    //

    if (HostObjectAttributes_.ObjectName->Buffer) {
      delete[] HostObjectAttributes_.ObjectName->Buffer;
      HostObjectAttributes_.ObjectName->Buffer = nullptr;
    }

    delete HostObjectAttributes_.ObjectName;
    HostObjectAttributes_.ObjectName = nullptr;

    //
    // Same deal the SecurityQualityOfService.
    //

    if (HostObjectAttributes_.SecurityQualityOfService) {
      delete[] HostObjectAttributes_.SecurityQualityOfService;
      HostObjectAttributes_.SecurityQualityOfService = nullptr;
    }
  }

  HostObjectAttributes_t(const HostObjectAttributes_t &) = delete;
  HostObjectAttributes_t &operator=(const HostObjectAttributes_t &) = delete;

  bool ReadFromGuest(const Backend_t *Backend,
                     const Gva_t GuestObjectAttributes) {
    //
    // Read the OBJECT_ATTRIBUTES.
    //

    Backend->VirtReadStruct(GuestObjectAttributes, &HostObjectAttributes_);

    //
    // Allocate memory to back the UNICODE_STRING.
    //

    auto HostObjectName = new UNICODE_STRING;
    if (!HostObjectName) {
      fmt::print("Could not allocate a UNICODE_STRING.\n");
      return false;
    }

    //
    // Read the ObjectName.
    //

    const Gva_t ObjectName = Gva_t(uint64_t(HostObjectAttributes_.ObjectName));
    Backend->VirtReadStruct(ObjectName, HostObjectName);

    //
    // Allocate memory for the UNICODE_STRING buffer. Note that it is not
    // mandatory to have the buffer NULL terminated. This happens when Length ==
    // MaximumLength.
    //

    const bool NeedsNullByte =
        HostObjectName->MaximumLength == HostObjectName->Length;

    //
    // Don't forget here that the Length are not number of char16_t but are
    // buffer size. That is why we need to mulitply `NeedsNullByte` by the size
    // of char16_t.
    //

    HostObjectName->MaximumLength += uint64_t(NeedsNullByte) * sizeof(char16_t);

    //
    // Allocate the buffer.
    //

    auto HostObjectNameBuffer =
        (char16_t *)new uint8_t[HostObjectName->MaximumLength];
    if (!HostObjectNameBuffer) {
      fmt::print("Could not allocate the UNICODE_STRING buffer.\n");
      return false;
    }

    //
    // Read the UNICODE_STRING buffer.
    //

    const Gva_t Buffer = Gva_t(uint64_t(HostObjectName->Buffer));
    Backend->VirtRead(Buffer, (uint8_t *)HostObjectNameBuffer,
                      HostObjectName->MaximumLength);

    //
    // Fix the null byte if we need to.
    //

    if (NeedsNullByte) {
      const uint64_t NullByteOffset = HostObjectName->Length / sizeof(char16_t);
      HostObjectNameBuffer[NullByteOffset] = 0;
    }

    //
    // Replace the buffer (guest pointer) by the host backing.
    //

    HostObjectName->Buffer = HostObjectNameBuffer;

    //
    // Replace the ObjectName (guest pointer) by the host backing.
    //

    HostObjectAttributes_.ObjectName = HostObjectName;

    //
    // If we have a SecurityQualityOfService buffer, then grab it.
    //

    if (HostObjectAttributes_.SecurityQualityOfService) {

      //
      // Grab the guest pointer.
      //

      const Gva_t SecurityQualityOfService =
          Gva_t(uint64_t(HostObjectAttributes_.SecurityQualityOfService));

      //
      // The first DWORD is the size of the buffer.
      //

      const uint32_t QualityOfServiceSize =
          Backend->VirtRead4(SecurityQualityOfService);

      //
      // Allocate memory to read the guest buffer.
      //

      HostObjectAttributes_.SecurityQualityOfService =
          new uint8_t[QualityOfServiceSize];

      if (!HostObjectAttributes_.SecurityQualityOfService) {
        fmt::print("Could not allocate memory for SecurityQualityOfService.\n");
        return false;
      }

      //
      // Read the guest buffer.
      //

      Backend->VirtRead(
          SecurityQualityOfService,
          (uint8_t *)HostObjectAttributes_.SecurityQualityOfService,
          QualityOfServiceSize);
    }

    //
    // We're done!
    //

    return true;
  }

  [[nodiscard]] const char16_t *ObjectName() const {
    return HostObjectAttributes_.ObjectName->Buffer;
  }
};

const uint64_t _1KB = 1024;
const uint64_t _1MB = _1KB * _1KB;

//
// Decodes an encoded pointer like ntdll does it.
//

[[nodiscard]] const Gva_t DecodePointer(const uint64_t Cookie,
                                        const uint64_t Value);

//
// Converts a u16string to a string to be able to display it as I can't find
// anything able to display those...??
//

[[nodiscard]] std::string u16stringToString(const std::u16string &S);

//
// Parses a code coverage file to get all the addresses where we need to set
// breakpoints.
//

[[nodiscard]] std::optional<tsl::robin_map<Gva_t, Gpa_t>>
ParseCovFiles(const Backend_t &Backend, const fs::path &CovBreakpointFile);

//
// Save a file on disk.
//

[[nodiscard]] std::optional<bool>
SaveFile(const fs::path &Path, const uint8_t *Buffer, const size_t BufferSize);

//
// Utility to convert an exception code to a string.
//

[[nodiscard]] std::string_view ExceptionCodeToStr(const uint32_t ExceptionCode);