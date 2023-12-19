// Axel '0vercl0k' Souchet - 2023
#define CATCH_CONFIG_MAIN

#include "kdmp-parser.h"
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <unordered_map>

struct TestCaseValues {
  std::string_view File;
  kdmpparser::DumpType_t Type;
  uint64_t Size = 0;
  uint64_t ReadAddress = 0;
  std::array<uint8_t, 16> Bytes;
  uint64_t Rax = 0;
  uint64_t Rbx = 0;
  uint64_t Rcx = 0;
  uint64_t Rdx = 0;
  uint64_t Rsi = 0;
  uint64_t Rdi = 0;
  uint64_t Rip = 0;
  uint64_t Rsp = 0;
  uint64_t Rbp = 0;
  uint64_t R8 = 0;
  uint64_t R9 = 0;
  uint64_t R10 = 0;
  uint64_t R11 = 0;
  uint64_t R12 = 0;
  uint64_t R13 = 0;
  uint64_t R14 = 0;
  uint64_t R15 = 0;
};

constexpr TestCaseValues TestCaseBmp{
    //
    // kd> r
    // rax=0000000000000003 rbx=fffff8050f4e9f70 rcx=0000000000000001
    // rdx=fffff805135684d0 rsi=0000000000000100 rdi=fffff8050f4e9f80
    // rip=fffff805108776a0 rsp=fffff805135684f8 rbp=fffff80513568600
    // r8=0000000000000003  r9=fffff805135684b8 r10=0000000000000000
    // r11=ffffa8848825e000 r12=fffff8050f4e9f80 r13=fffff80510c3c958
    // r14=0000000000000000 r15=0000000000000052
    // iopl=0         nv up ei pl nz na pe nc
    // cs=0010  ss=0018  ds=002b  es=002b  fs=0053  gs=002b efl=00040202
    //
    "bmp.dmp",
    kdmpparser::DumpType_t::BMPDump,
    0x54'4b,
    0x6d'4d'22,
    {0x6d, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x63, 0x88, 0x75, 0x00, 0x00, 0x00,
     0x00, 0x0a, 0x63, 0x98},
    0x00000000'00000003ULL,
    0xfffff805'0f4e9f70ULL,
    0x00000000'00000001ULL,
    0xfffff805'135684d0ULL,
    0x00000000'00000100ULL,
    0xfffff805'0f4e9f80ULL,
    0xfffff805'108776a0ULL,
    0xfffff805'135684f8ULL,
    0xfffff805'13568600ULL,
    0x00000000'00000003ULL,
    0xfffff805'135684b8ULL,
    0x00000000'00000000ULL,
    0xffffa884'8825e000ULL,
    0xfffff805'0f4e9f80ULL,
    0xfffff805'10c3c958ULL,
    0x00000000'00000000ULL,
    0x00000000'00000052ULL,
};

constexpr TestCaseValues TestCaseFull{
    "full.dmp",
    kdmpparser::DumpType_t::FullDump,
    0x03'fb'e6,
    0x6d'4d'22,
    {0x6d, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x63, 0x88, 0x75, 0x00, 0x00, 0x00,
     0x00, 0x0a, 0x63, 0x98},
    0x00000000'00000003ULL,
    0xfffff805'0f4e9f70ULL,
    0x00000000'00000001ULL,
    0xfffff805'135684d0ULL,
    0x00000000'00000100ULL,
    0xfffff805'0f4e9f80ULL,
    0xfffff805'108776a0ULL,
    0xfffff805'135684f8ULL,
    0xfffff805'13568600ULL,
    0x00000000'00000003ULL,
    0xfffff805'135684b8ULL,
    0x00000000'00000000ULL,
    0xffffa884'8825e000ULL,
    0xfffff805'0f4e9f80ULL,
    0xfffff805'10c3c958ULL,
    0x00000000'00000000ULL,
    0x00000000'00000052ULL,
};

constexpr TestCaseValues TestCaseKernelDump{
    "kerneldump.dmp",
    kdmpparser::DumpType_t::KernelMemoryDump,
    0xa0'2e,
    0x02'58'92'f0,
    {0x10, 0x8c, 0x24, 0x50, 0x0c, 0xc0, 0xff, 0xff, 0xa0, 0x19, 0x38, 0x51,
     0x0c, 0xc0, 0xff, 0xff},
    0x00000000'00007a01ULL,
    0xffffc00c'5191e010ULL,
    0x00000000'00000001ULL,
    0x00000012'00000000ULL,
    0xffffc00c'51907bb0ULL,
    0x00000000'00000002ULL,
    0xfffff803'f2c35470ULL,
    0xfffff803'f515ec28ULL,
    0x00000000'0c1c9800ULL,
    0x00000000'000000b0ULL,
    0xffffc00c'502ff000ULL,
    0x00000000'00000057ULL,
    0xfffff803'f3a04500ULL,
    0xfffff803'f515ee60ULL,
    0x00000000'00000003ULL,
    0xfffff803'f1e9a180ULL,
    0x00000000'0000001fULL,
};

constexpr TestCaseValues TestCaseKernelUserDump{
    "kerneluserdump.dmp",
    kdmpparser::DumpType_t::KernelAndUserMemoryDump,
    0x01'f7'c7,
    0x02'58'92'f0,
    {0x10, 0x8c, 0x24, 0x50, 0x0c, 0xc0, 0xff, 0xff, 0xa0, 0x19, 0x38, 0x51,
     0x0c, 0xc0, 0xff, 0xff},
    0x00000000'00007a01ULL,
    0xffffc00c'5191e010ULL,
    0x00000000'00000001ULL,
    0x00000012'00000000ULL,
    0xffffc00c'51907bb0ULL,
    0x00000000'00000002ULL,
    0xfffff803'f2c35470ULL,
    0xfffff803'f515ec28ULL,
    0x00000000'0c1c9800ULL,
    0x00000000'000000b0ULL,
    0xffffc00c'502ff000ULL,
    0x00000000'00000057ULL,
    0xfffff803'f3a04500ULL,
    0xfffff803'f515ee60ULL,
    0x00000000'00000003ULL,
    0xfffff803'f1e9a180ULL,
    0x00000000'0000001fULL,
};

constexpr TestCaseValues TestCaseCompleteDump{
    "completedump.dmp",
    kdmpparser::DumpType_t::CompleteMemoryDump,
    0x01'fb'f9,
    0x02'58'92'f0,
    {0x10, 0x8c, 0x24, 0x50, 0x0c, 0xc0, 0xff, 0xff, 0xa0, 0x19, 0x38, 0x51,
     0x0c, 0xc0, 0xff, 0xff},
    0x00000000'00007a01ULL,
    0xffffc00c'5191e010ULL,
    0x00000000'00000001ULL,
    0x00000012'00000000ULL,
    0xffffc00c'51907bb0ULL,
    0x00000000'00000002ULL,
    0xfffff803'f2c35470ULL,
    0xfffff803'f515ec28ULL,
    0x00000000'0c1c9800ULL,
    0x00000000'000000b0ULL,
    0xffffc00c'502ff000ULL,
    0x00000000'00000057ULL,
    0xfffff803'f3a04500ULL,
    0xfffff803'f515ee60ULL,
    0x00000000'00000003ULL,
    0xfffff803'f1e9a180ULL,
    0x00000000'0000001fULL,
};

constexpr std::array Testcases{
    TestCaseBmp,          TestCaseFull,
    TestCaseKernelDump,   TestCaseKernelUserDump,
    TestCaseCompleteDump,
};

TEST_CASE("kdmp-parser", "parser") {
  SECTION("Test minidump exists") {
    for (const auto &Testcase : Testcases) {
      REQUIRE(std::filesystem::exists(Testcase.File));
    }
  }

  SECTION("Basic parsing") {
    for (const auto &Testcase : Testcases) {
      kdmpparser::KernelDumpParser Dmp;
      REQUIRE(Dmp.Parse(Testcase.File.data()));
      CHECK(Dmp.GetDumpType() == Testcase.Type);
      const auto &Physmem = Dmp.GetPhysmem();
      CHECK(Physmem.size() == Testcase.Size);
    }
  }

  SECTION("Context values") {
    for (const auto &Testcase : Testcases) {
      kdmpparser::KernelDumpParser Dmp;
      REQUIRE(Dmp.Parse(Testcase.File.data()));
      const auto &Context = Dmp.GetContext();
      CHECK(Context.Rax == Testcase.Rax);
      CHECK(Context.Rbx == Testcase.Rbx);
      CHECK(Context.Rcx == Testcase.Rcx);
      CHECK(Context.Rdx == Testcase.Rdx);
      CHECK(Context.Rsi == Testcase.Rsi);
      CHECK(Context.Rdi == Testcase.Rdi);
      CHECK(Context.Rip == Testcase.Rip);
      CHECK(Context.Rsp == Testcase.Rsp);
      CHECK(Context.Rbp == Testcase.Rbp);
      CHECK(Context.R8 == Testcase.R8);
      CHECK(Context.R9 == Testcase.R9);
      CHECK(Context.R10 == Testcase.R10);
      CHECK(Context.R11 == Testcase.R11);
      CHECK(Context.R12 == Testcase.R12);
      CHECK(Context.R13 == Testcase.R13);
      CHECK(Context.R14 == Testcase.R14);
      CHECK(Context.R15 == Testcase.R15);
    }
  }

  SECTION("Memory access") {
    for (const auto &Testcase : Testcases) {
      kdmpparser::KernelDumpParser Dmp;
      REQUIRE(Dmp.Parse(Testcase.File.data()));
      const uint64_t Address = Testcase.ReadAddress;
      const uint64_t AddressAligned = kdmpparser::Page::Align(Address);
      const uint64_t AddressOffset = kdmpparser::Page::Offset(Address);
      const auto &ExpectedContent = Testcase.Bytes;
      const uint8_t *Page = Dmp.GetPhysicalPage(AddressAligned);
      REQUIRE(Page != nullptr);
      CHECK(memcmp(Page + AddressOffset, ExpectedContent.data(),
                   sizeof(ExpectedContent)) == 0);
    }
  }
}
