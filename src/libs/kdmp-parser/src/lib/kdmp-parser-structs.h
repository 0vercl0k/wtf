// Axel '0vercl0k' Souchet - February 15 2019
#pragma once

#include "platform.h"
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <type_traits>

namespace kdmpparser {

//
// We need a way to represent 128-bits integers so here goes.
//

struct uint128_t {
  uint64_t Low;
  uint64_t High;
};

static_assert(sizeof(uint128_t) == 16, "uint128_t's size looks wrong.");

enum class DumpType_t : uint32_t { FullDump = 1, KernelDump = 2, BMPDump = 5 };

//
// Save off the alignement setting and disable
// alignement.
//

#pragma pack(push)
#pragma pack(1)

//
// Field is a pointer inside the this object and this function
// returns the offset of Field in the object via pointer arithmetic.
//

constexpr uint64_t OffsetFromThis(const uintptr_t This, const uintptr_t Field) {
  return uint64_t(Field) - uint64_t(This);
}

static void DisplayHeader(const uint32_t Prefix, const char *FieldName,
                          const void *This, const void *Field) {
  printf("%*s+0x%04" PRIx64 ": %-25s", Prefix, "",
         OffsetFromThis(uintptr_t(This), uintptr_t(Field)), FieldName);
}

//
// This is the macro we use to get the field name via the preprocessor.
//

#define DISPLAY_FIELD(FieldName)                                               \
  DisplayField(Prefix + 2, #FieldName, this, &FieldName)

#define DISPLAY_FIELD_OFFSET(FieldName)                                        \
  DisplayHeader(Prefix + 2, #FieldName, this, &FieldName);                     \
  printf("\n")

//
// This takes care of displaying basic types.
//

template <typename Field_t>
static void DisplayField(const uint32_t Prefix, const char *FieldName,
                         const void *This, const Field_t *Field) {
  DisplayHeader(Prefix, FieldName, This, Field);
  if constexpr (std::is_same<Field_t, uint8_t>::value) {
    printf(": 0x%02x.\n", *Field);
  } else if constexpr (std::is_same<Field_t, uint16_t>::value) {
    printf(": 0x%04x.\n", *Field);
  } else if constexpr (std::is_same<Field_t, uint32_t>::value) {
    printf(": 0x%08x.\n", *Field);
  } else if constexpr (std::is_same<Field_t, uint64_t>::value) {
    printf(": 0x%016" PRIx64 ".\n", *Field);
  } else if constexpr (std::is_same<Field_t, uint128_t>::value) {
    printf(": 0x%016" PRIx64 "%016" PRIx64 ".\n", Field->High, Field->Low);
  } else if constexpr (std::is_same<Field_t, DumpType_t>::value) {
    switch (*Field) {
    case DumpType_t::KernelDump: {
      printf(": Kernel Dump.\n");
      return;
    }

    case DumpType_t::FullDump: {
      printf(": Full Dump.\n");
      return;
    }
    case DumpType_t::BMPDump: {
      printf(": BMP Dump.\n");
      return;
    }
    }
    printf(": Unknown.\n");
  } else {

    //
    // We use std::is_same<> here because otherwise the static_assert fires
    // immediately on g++/clang++ without even instantiating FieldType_t.
    // So we kind of trick the compiler into doing what we want.
    //

    static_assert(std::is_same<Field_t, uint8_t>::value,
                  "DisplayField: Unknown type trying to be displayed.");
  }
}

//
// Display the header of a dump section.
//

#define DISPLAY_HEADER(Name) printf("%*s" Name "\n", Prefix, "")

//
// All credit goes to the rekall project for the RE of the file format.
// https://github.com/google/rekall/blob/master/rekall-core/rekall/plugins/overlays/windows/crashdump.py
//

struct PHYSMEM_RUN {
  uint64_t BasePage;
  uint64_t PageCount;

  void Show(const uint32_t Prefix = 0) const {
    DISPLAY_HEADER("PHYSMEM_RUN");
    DISPLAY_FIELD(BasePage);
    DISPLAY_FIELD(PageCount);
  }
};

static_assert(sizeof(PHYSMEM_RUN) == 0x10, "PHYSMEM_RUN's size looks wrong.");

struct PHYSMEM_DESC {
  uint32_t NumberOfRuns;
  uint32_t Padding0;
  uint64_t NumberOfPages;
  PHYSMEM_RUN Run[1];

  void Show(const uint32_t Prefix = 0) const {
    DISPLAY_HEADER("PHYSMEM_DESC");
    DISPLAY_FIELD(NumberOfRuns);
    DISPLAY_FIELD(NumberOfPages);
    DISPLAY_FIELD_OFFSET(Run);
    if (!LooksGood()) {
      return;
    }

    for (uint32_t RunIdx = 0; RunIdx < NumberOfRuns; RunIdx++) {
      Run[RunIdx].Show(Prefix + 2);
    }
  }

  constexpr bool LooksGood() const {
    if (NumberOfRuns == 0x45474150 || NumberOfPages == 0x4547415045474150ULL) {
      return false;
    }

    return true;
  }
};

static_assert(sizeof(PHYSMEM_DESC) == 0x20,
              "PHYSICAL_MEMORY_DESCRIPTOR's size looks wrong.");

struct BMP_HEADER64 {
  static constexpr uint32_t ExpectedSignature = 0x504D4453;  // 'PMDS'
  static constexpr uint32_t ExpectedSignature2 = 0x504D4446; // 'PMDF'
  static constexpr uint32_t ExpectedValidDump = 0x504D5544;  // 'PMUD'

  //
  // Should be FDMP.
  //

  uint32_t Signature;

  //
  // Should be DUMP.
  //

  uint32_t ValidDump;

  //
  // According to rekall there's a gap there:
  // 'ValidDump': [0x4, ['String', dict(
  //    length=4,
  //    term=None,
  //    )]],
  // # The offset of the first page in the file.
  // 'FirstPage': [0x20, ['unsigned long long']],
  //

  uint8_t Padding0[0x20 - (0x4 + sizeof(ValidDump))];

  //
  // The offset of the first page in the file.
  //

  uint64_t FirstPage;

  //
  // Total number of pages present in the bitmap.
  //
  uint64_t TotalPresentPages;

  //
  // Total number of pages in image.This dictates the total size of the
  // bitmap.This is not the same as the TotalPresentPages which is only
  // the sum of the bits set to 1.
  //

  uint64_t Pages;

  uint8_t Bitmap[1];

  bool LooksGood() const {

    //
    // Integrity check the headers.
    //

    if (Signature != ExpectedSignature && Signature != ExpectedSignature2) {
      printf("BMP_HEADER64::Signature looks wrong.\n");
      return false;
    }

    if (ValidDump != ExpectedValidDump) {
      printf("BMP_HEADER64::ValidDump looks wrong.\n");
      return false;
    }

    return true;
  }

  void Show(const uint32_t Prefix = 0) const {
    DISPLAY_HEADER("BMP_HEADER64");
    DISPLAY_FIELD(Signature);
    DISPLAY_FIELD(ValidDump);
    DISPLAY_FIELD(FirstPage);
    DISPLAY_FIELD(TotalPresentPages);
    DISPLAY_FIELD(Pages);
    DISPLAY_FIELD_OFFSET(Bitmap);
  }
};

static_assert(offsetof(BMP_HEADER64, FirstPage) == 0x20,
              "First page offset looks wrong.");

struct CONTEXT {

  //
  // Note that the below definition has been stolen directly from the windows
  // headers. Why you might ask? Well the structure comes with DECLSPEC_ALIGN
  // that was preventing me from layoung the Context structure at the offset I
  // wanted. Maybe there's a cleaner way to do this, if so let me know :)
  //

  //
  // Register parameter home addresses.
  //
  // N.B. These fields are for convience - they could be used to extend the
  //      context record in the future.
  //

  uint64_t P1Home;
  uint64_t P2Home;
  uint64_t P3Home;
  uint64_t P4Home;
  uint64_t P5Home;
  uint64_t P6Home;

  //
  // Control flags.
  //

  uint32_t ContextFlags;
  uint32_t MxCsr;

  //
  // Segment Registers and processor flags.
  //

  uint16_t SegCs;
  uint16_t SegDs;
  uint16_t SegEs;
  uint16_t SegFs;
  uint16_t SegGs;
  uint16_t SegSs;
  uint32_t EFlags;

  //
  // Debug registers
  //

  uint64_t Dr0;
  uint64_t Dr1;
  uint64_t Dr2;
  uint64_t Dr3;
  uint64_t Dr6;
  uint64_t Dr7;

  //
  // Integer registers.
  //

  uint64_t Rax;
  uint64_t Rcx;
  uint64_t Rdx;
  uint64_t Rbx;
  uint64_t Rsp;
  uint64_t Rbp;
  uint64_t Rsi;
  uint64_t Rdi;
  uint64_t R8;
  uint64_t R9;
  uint64_t R10;
  uint64_t R11;
  uint64_t R12;
  uint64_t R13;
  uint64_t R14;
  uint64_t R15;

  //
  // Program counter.
  //

  uint64_t Rip;

  //
  // Floating point state.
  //

  uint16_t ControlWord;
  uint16_t StatusWord;
  uint8_t TagWord;
  uint8_t Reserved1;
  uint16_t ErrorOpcode;
  uint32_t ErrorOffset;
  uint16_t ErrorSelector;
  uint16_t Reserved2;
  uint32_t DataOffset;
  uint16_t DataSelector;
  uint16_t Reserved3;
  uint32_t MxCsr2;
  uint32_t MxCsr_Mask;
  uint128_t FloatRegisters[8];
  uint128_t Xmm0;
  uint128_t Xmm1;
  uint128_t Xmm2;
  uint128_t Xmm3;
  uint128_t Xmm4;
  uint128_t Xmm5;
  uint128_t Xmm6;
  uint128_t Xmm7;
  uint128_t Xmm8;
  uint128_t Xmm9;
  uint128_t Xmm10;
  uint128_t Xmm11;
  uint128_t Xmm12;
  uint128_t Xmm13;
  uint128_t Xmm14;
  uint128_t Xmm15;

  //
  // Vector registers.
  //

  uint128_t VectorRegister[26];
  uint64_t VectorControl;

  //
  // Special debug control registers.
  //

  uint64_t DebugControl;
  uint64_t LastBranchToRip;
  uint64_t LastBranchFromRip;
  uint64_t LastExceptionToRip;
  uint64_t LastExceptionFromRip;

  bool LooksGood() const {

    //
    // Integrity check the CONTEXT record.
    //

    if (MxCsr != MxCsr2) {
      printf("CONTEXT::MxCsr doesn't match MxCsr2.\n");
      return false;
    }

    return true;
  }

  void Show(const uint32_t Prefix = 0) const {
    DISPLAY_HEADER("CONTEXT");
    DISPLAY_FIELD(P1Home);
    DISPLAY_FIELD(P2Home);
    DISPLAY_FIELD(P3Home);
    DISPLAY_FIELD(P4Home);
    DISPLAY_FIELD(P5Home);
    DISPLAY_FIELD(P6Home);

    //
    // Control flags.
    //

    DISPLAY_FIELD(ContextFlags);
    DISPLAY_FIELD(MxCsr);

    //
    // Segment Registers and processor flags.
    //

    DISPLAY_FIELD(SegCs);
    DISPLAY_FIELD(SegDs);
    DISPLAY_FIELD(SegEs);
    DISPLAY_FIELD(SegFs);
    DISPLAY_FIELD(SegGs);
    DISPLAY_FIELD(SegSs);
    DISPLAY_FIELD(EFlags);

    //
    // Debug registers.
    // XXX: Figure out what they don't look right.
    //

    DISPLAY_FIELD(Dr0);
    DISPLAY_FIELD(Dr1);
    DISPLAY_FIELD(Dr2);
    DISPLAY_FIELD(Dr3);
    DISPLAY_FIELD(Dr6);
    DISPLAY_FIELD(Dr7);

    //
    // Integer registers.
    //

    DISPLAY_FIELD(Rax);
    DISPLAY_FIELD(Rcx);
    DISPLAY_FIELD(Rdx);
    DISPLAY_FIELD(Rbx);
    DISPLAY_FIELD(Rsp);
    DISPLAY_FIELD(Rbp);
    DISPLAY_FIELD(Rsi);
    DISPLAY_FIELD(Rdi);
    DISPLAY_FIELD(R8);
    DISPLAY_FIELD(R9);
    DISPLAY_FIELD(R10);
    DISPLAY_FIELD(R11);
    DISPLAY_FIELD(R12);
    DISPLAY_FIELD(R13);
    DISPLAY_FIELD(R14);
    DISPLAY_FIELD(R15);

    //
    // Program counter.
    //

    DISPLAY_FIELD(Rip);

    //
    // Floating point state.
    //

    DISPLAY_FIELD(ControlWord);
    DISPLAY_FIELD(StatusWord);
    DISPLAY_FIELD(TagWord);
    DISPLAY_FIELD(ErrorOpcode);
    DISPLAY_FIELD(ErrorOffset);
    DISPLAY_FIELD(ErrorSelector);
    DISPLAY_FIELD(DataOffset);
    DISPLAY_FIELD(DataSelector);
    DISPLAY_FIELD(MxCsr2);
    DISPLAY_FIELD(MxCsr_Mask);
    DISPLAY_FIELD(FloatRegisters[0]);
    DISPLAY_FIELD(FloatRegisters[1]);
    DISPLAY_FIELD(FloatRegisters[2]);
    DISPLAY_FIELD(FloatRegisters[3]);
    DISPLAY_FIELD(FloatRegisters[4]);
    DISPLAY_FIELD(FloatRegisters[5]);
    DISPLAY_FIELD(FloatRegisters[6]);
    DISPLAY_FIELD(FloatRegisters[7]);
    DISPLAY_FIELD(Xmm0);
    DISPLAY_FIELD(Xmm1);
    DISPLAY_FIELD(Xmm2);
    DISPLAY_FIELD(Xmm3);
    DISPLAY_FIELD(Xmm4);
    DISPLAY_FIELD(Xmm5);
    DISPLAY_FIELD(Xmm6);
    DISPLAY_FIELD(Xmm7);
    DISPLAY_FIELD(Xmm8);
    DISPLAY_FIELD(Xmm9);
    DISPLAY_FIELD(Xmm10);
    DISPLAY_FIELD(Xmm11);
    DISPLAY_FIELD(Xmm12);
    DISPLAY_FIELD(Xmm13);
    DISPLAY_FIELD(Xmm14);
    DISPLAY_FIELD(Xmm15);

    //
    // Vector registers.
    //

    // M128A VectorRegister[26];
    DISPLAY_FIELD(VectorControl);

    //
    // Special debug control registers.
    //

    DISPLAY_FIELD(DebugControl);
    DISPLAY_FIELD(LastBranchToRip);
    DISPLAY_FIELD(LastBranchFromRip);
    DISPLAY_FIELD(LastExceptionToRip);
    DISPLAY_FIELD(LastExceptionFromRip);
  }
};

static_assert(offsetof(CONTEXT, Xmm0) == 0x1a0,
              "The offset of Xmm0 looks wrong.");

struct EXCEPTION_RECORD64 {
  uint32_t ExceptionCode;
  uint32_t ExceptionFlags;
  uint64_t ExceptionRecord;
  uint64_t ExceptionAddress;
  uint32_t NumberParameters;
  uint32_t __unusedAlignment;
  uint64_t ExceptionInformation[15];

  void Show(const uint32_t Prefix = 0) const {
    DISPLAY_HEADER("KDMP_PARSER_EXCEPTION_RECORD64");
    DISPLAY_FIELD(ExceptionCode);
    DISPLAY_FIELD(ExceptionFlags);
    DISPLAY_FIELD(ExceptionRecord);
    DISPLAY_FIELD(ExceptionAddress);
    DISPLAY_FIELD(NumberParameters);
    DISPLAY_FIELD(ExceptionInformation[0]);
    DISPLAY_FIELD(ExceptionInformation[1]);
    DISPLAY_FIELD(ExceptionInformation[2]);
    DISPLAY_FIELD(ExceptionInformation[3]);
    DISPLAY_FIELD(ExceptionInformation[4]);
    DISPLAY_FIELD(ExceptionInformation[5]);
    DISPLAY_FIELD(ExceptionInformation[6]);
    DISPLAY_FIELD(ExceptionInformation[7]);
    DISPLAY_FIELD(ExceptionInformation[8]);
    DISPLAY_FIELD(ExceptionInformation[9]);
    DISPLAY_FIELD(ExceptionInformation[10]);
    DISPLAY_FIELD(ExceptionInformation[11]);
    DISPLAY_FIELD(ExceptionInformation[12]);
    DISPLAY_FIELD(ExceptionInformation[13]);
    DISPLAY_FIELD(ExceptionInformation[14]);
  }
};

static_assert(sizeof(EXCEPTION_RECORD64) == 0x98,
              "KDMP_PARSER_EXCEPTION_RECORD64's size looks wrong.");

struct HEADER64 {
  static const uint32_t ExpectedSignature = 0x45474150; // 'EGAP'
  static const uint32_t ExpectedValidDump = 0x34365544; // '46UD'

  uint32_t Signature;
  uint32_t ValidDump;
  uint32_t MajorVersion;
  uint32_t MinorVersion;
  uint64_t DirectoryTableBase;
  uint64_t PfnDatabase;
  uint64_t PsLoadedModuleList;
  uint64_t PsActiveProcessHead;
  uint32_t MachineImageType;
  uint32_t NumberProcessors;
  uint32_t BugCheckCode;

  //
  // According to rekall there's a gap here:
  // 'BugCheckCode' : [0x38, ['unsigned long']],
  // 'BugCheckCodeParameter' : [0x40, ['array', 4, ['unsigned long long']]],
  //

  uint8_t Padding0[0x40 - (0x38 + sizeof(BugCheckCode))];
  uint64_t BugCheckCodeParameter[4];

  //
  // According to rekall there's a gap here:
  // 'BugCheckCodeParameter' : [0x40, ['array', 4, ['unsigned long long']]],
  // 'KdDebuggerDataBlock' : [0x80, ['unsigned long long']],
  //

  uint8_t Padding1[0x80 - (0x40 + sizeof(BugCheckCodeParameter))];
  uint64_t KdDebuggerDataBlock;
  PHYSMEM_DESC PhysicalMemoryBlockBuffer;

  //
  // According to rekall there's a gap here:
  // 'PhysicalMemoryBlockBuffer' : [0x88, ['_PHYSICAL_MEMORY_DESCRIPTOR']],
  // 'ContextRecord' : [0x348, ['array', 3000, ['unsigned char']]],
  //

  uint8_t Padding2[0x348 - (0x88 + sizeof(PhysicalMemoryBlockBuffer))];
  CONTEXT ContextRecord;

  //
  // According to rekall there's a gap here:
  // 'ContextRecord' : [0x348, ['array', 3000, ['unsigned char']]],
  // 'Exception' : [0xf00, ['_EXCEPTION_RECORD64']],
  //

  uint8_t Padding3[0xf00 - (0x348 + sizeof(ContextRecord))];
  EXCEPTION_RECORD64 Exception;
  DumpType_t DumpType;

  //
  // According to rekall there's a gap here:
  // 'DumpType' : [0xf98, ['unsigned long']],
  // 'RequiredDumpSpace' : [0xfa0, ['unsigned long long']],
  //
  uint8_t Padding4[0xfa0 - (0xf98 + sizeof(DumpType))];
  uint64_t RequiredDumpSpace;
  uint64_t SystemTime;
  uint8_t Comment[128];
  uint64_t SystemUpTime;
  uint32_t MiniDumpFields;
  uint32_t SecondaryDataState;
  uint32_t ProductType;
  uint32_t SuiteMask;
  uint32_t WriterStatus;
  uint8_t Unused1;
  uint8_t KdSecondaryVersion;
  uint8_t Unused[2];
  uint8_t _reserved0[4016];
  BMP_HEADER64 BmpHeader;

  bool LooksGood() const {

    //
    // Integrity check the headers.
    //

    if (Signature != ExpectedSignature) {
      printf("HEADER64::Signature looks wrong.\n");
      return false;
    }

    if (ValidDump != ExpectedValidDump) {
      printf("HEADER64::ValidDump looks wrong.\n");
      return false;
    }

    //
    // Make sure it's a dump type we know how to handle.
    //

    if (DumpType == DumpType_t::FullDump) {
      if (!PhysicalMemoryBlockBuffer.LooksGood()) {
        printf("The PhysicalMemoryBlockBuffer looks wrong.\n");
        return false;
      }
    } else if (DumpType == DumpType_t::BMPDump) {
      if (!BmpHeader.LooksGood()) {
        printf("The BmpHeader looks wrong.\n");
        return false;
      }
    }

    //
    // Integrity check the CONTEXT record.
    //

    if (!ContextRecord.LooksGood()) {
      return false;
    }

    return true;
  }

  void Show(const uint32_t Prefix = 0) const {
    DISPLAY_HEADER("HEADER64");
    DISPLAY_FIELD(Signature);
    DISPLAY_FIELD(ValidDump);
    DISPLAY_FIELD(MajorVersion);
    DISPLAY_FIELD(MinorVersion);
    DISPLAY_FIELD(DirectoryTableBase);
    DISPLAY_FIELD(PfnDatabase);
    DISPLAY_FIELD(PsLoadedModuleList);
    DISPLAY_FIELD(PsActiveProcessHead);
    DISPLAY_FIELD(MachineImageType);
    DISPLAY_FIELD(NumberProcessors);
    DISPLAY_FIELD(BugCheckCode);
    DISPLAY_FIELD_OFFSET(BugCheckCodeParameter);
    DISPLAY_FIELD(KdDebuggerDataBlock);
    DISPLAY_FIELD_OFFSET(PhysicalMemoryBlockBuffer);
    PhysicalMemoryBlockBuffer.Show(Prefix + 2);
    DISPLAY_FIELD_OFFSET(ContextRecord);
    ContextRecord.Show(Prefix + 2);
    DISPLAY_FIELD_OFFSET(Exception);
    Exception.Show(Prefix + 2);
    DISPLAY_FIELD(DumpType);
    DISPLAY_FIELD(RequiredDumpSpace);
    DISPLAY_FIELD(SystemTime);
    DISPLAY_FIELD_OFFSET(Comment);
    DISPLAY_FIELD(SystemUpTime);
    DISPLAY_FIELD(MiniDumpFields);
    DISPLAY_FIELD(SecondaryDataState);
    DISPLAY_FIELD(ProductType);
    DISPLAY_FIELD(SuiteMask);
    DISPLAY_FIELD(WriterStatus);
    DISPLAY_FIELD(KdSecondaryVersion);
    if (DumpType == DumpType_t::BMPDump) {
      DISPLAY_FIELD_OFFSET(BmpHeader);
      BmpHeader.Show();
    }
  }
};

//
// Restore the default alignement setting.
//

#pragma pack(pop)

//
// Prevent the user to play around with those.
//

#undef DISPLAY_HEADER
#undef DISPLAY_FIELD

//
// Those asserts are the results of a lot of frustration getting the right
// layout, so hopefully they prevent any regressions regarding the layout.
//

static_assert(offsetof(HEADER64, BugCheckCodeParameter) == 0x40,
              "The offset of KdDebuggerDataBlock looks wrong.");

static_assert(offsetof(HEADER64, KdDebuggerDataBlock) == 0x80,
              "The offset of KdDebuggerDataBlock looks wrong.");

static_assert(offsetof(HEADER64, ContextRecord) == 0x348,
              "The offset of ContextRecord looks wrong.");

static_assert(offsetof(HEADER64, Exception) == 0xf00,
              "The offset of Exception looks wrong.");

static_assert(offsetof(HEADER64, Comment) == 0xfb0,
              "The offset of Comment looks wrong.");

static_assert(offsetof(HEADER64, BmpHeader) == 0x2000,
              "The offset of BmpHeaders looks wrong.");

namespace Page {

//
// Page size.
//

constexpr uint64_t Size = 0x1000;

//
// Page align an address.
//

constexpr uint64_t Align(const uint64_t Address) { return Address & ~0xfff; }

//
// Extract the page offset off an address.
//

constexpr uint64_t Offset(const uint64_t Address) { return Address & 0xfff; }
} // namespace Page

//
// Structure for parsing a PTE.
//

union MMPTE_HARDWARE {
  struct {
    uint64_t Present : 1;
    uint64_t Write : 1;
    uint64_t UserAccessible : 1;
    uint64_t WriteThrough : 1;
    uint64_t CacheDisable : 1;
    uint64_t Accessed : 1;
    uint64_t Dirty : 1;
    uint64_t LargePage : 1;
    uint64_t Available : 4;
    uint64_t PageFrameNumber : 36;
    uint64_t ReservedForHardware : 4;
    uint64_t ReservedForSoftware : 11;
    uint64_t NoExecute : 1;
  } u;
  uint64_t AsUINT64;
  constexpr MMPTE_HARDWARE(const uint64_t Value) : AsUINT64(Value) {}
};

//
// Structure to parse a virtual address.
//

union VIRTUAL_ADDRESS {
  struct {
    uint64_t Offset : 12;
    uint64_t PtIndex : 9;
    uint64_t PdIndex : 9;
    uint64_t PdPtIndex : 9;
    uint64_t Pml4Index : 9;
    uint64_t Reserved : 16;
  } u;
  uint64_t AsUINT64;
  constexpr VIRTUAL_ADDRESS(const uint64_t Value) : AsUINT64(Value) {}
};

static_assert(sizeof(MMPTE_HARDWARE) == 8);
static_assert(sizeof(VIRTUAL_ADDRESS) == 8);
} // namespace kdmpparser