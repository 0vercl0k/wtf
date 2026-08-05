// Axel '0vercl0k' Souchet - February 15 2019
#pragma once

#include "platform.h"
#include <array>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string_view>
#include <type_traits>
#include <variant>

namespace kdmpparser {

//
// We need a way to represent 128-bits integers so here goes.
//

struct uint128_t {
  uint64_t Low;
  uint64_t High;
};

static_assert(sizeof(uint128_t) == 16, "uint128_t's size looks wrong.");

enum class DumpType_t : uint32_t {
  // Old dump types from dbgeng.dll
  FullDump = 0x1,
  KernelDump = 0x2,
  BMPDump = 0x5,

  // New stuff
  MiniDump = 0x4,             // Produced by `.dump /m`
  LiveKernelBitmapDump = 0x6, // (22h2+) Produced by TaskMgr > System > Create
                              // Live Kernel Memory Dump
  KernelMemoryDump = 0x8,     // Produced by `.dump /k`
  KernelAndUserMemoryDump = 0x9, // Produced by `.dump /ka`
  CompleteMemoryDump = 0xa,      // Produced by `.dump /f`
};

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

constexpr std::string_view DumpTypeToString(const DumpType_t Type) {
  switch (Type) {
  // Old dump types from dbgeng.dll
  case DumpType_t::FullDump:
    return "FullDump";
  case DumpType_t::KernelDump:
    return "KernelDump";
  case DumpType_t::BMPDump:
    return "BMPDump";

  // New stuff
  case DumpType_t::MiniDump:
    return "MiniDump";
  case DumpType_t::LiveKernelBitmapDump:
    return "LiveKernelBitmapDump";
  case DumpType_t::KernelMemoryDump:
    return "KernelMemoryDump";
  case DumpType_t::KernelAndUserMemoryDump:
    return "KernelAndUserMemoryDump";
  case DumpType_t::CompleteMemoryDump:
    return "CompleteMemoryDump";
  }

  return "Unknown";
}

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
  } else if constexpr (std::is_same<Field_t, int64_t>::value) {
    printf(": 0x%016" PRIx64 ".\n", *Field);
  } else if constexpr (std::is_same<Field_t, uint128_t>::value) {
    printf(": 0x%016" PRIx64 "%016" PRIx64 ".\n", Field->High, Field->Low);
  } else if constexpr (std::is_same<Field_t, DumpType_t>::value) {
    printf(": %s.\n", DumpTypeToString(*Field).data());
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
  static constexpr uint32_t ExpectedSignature = 0x50'4D'44'53;  // 'PMDS'
  static constexpr uint32_t ExpectedSignature2 = 0x50'4D'44'46; // 'PMDF'
  static constexpr uint32_t ExpectedValidDump = 0x50'4D'55'44;  // 'PMUD'

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

  std::array<uint8_t, 0x20 - (0x4 + sizeof(ValidDump))> Padding0;

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

  std::array<uint8_t, 1> Bitmap;

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

struct RDMP_HEADER64 {
  static constexpr uint32_t ExpectedMarker = 0x40;
  static constexpr uint32_t ExpectedSignature = 0x50'4D'44'52; // 'PMDR'
  static constexpr uint32_t ExpectedValidDump = 0x50'4D'55'44; // 'PMUD'

  uint32_t Marker;
  uint32_t Signature;
  uint32_t ValidDump;
  uint32_t __Unused;
  uint64_t MetadataSize;
  uint64_t FirstPageOffset;

  bool LooksGood() const {
    if (Marker != ExpectedMarker) {
      return false;
    }

    if (Signature != RDMP_HEADER64::ExpectedSignature) {
      return false;
    }

    if (ValidDump != RDMP_HEADER64::ExpectedValidDump) {
      return false;
    }

    if (MetadataSize - 0x20 !=
        FirstPageOffset -
            0x20'40) { // sizeof(HEADER64) + sizeof(RDMP_HEADERS64)
      return false;
    }

    return true;
  }

  void Show(const uint32_t Prefix = 0) const {
    DISPLAY_HEADER("RDMP_HEADER64");
    DISPLAY_FIELD(Signature);
    DISPLAY_FIELD(ValidDump);
    DISPLAY_FIELD(FirstPageOffset);
    DISPLAY_FIELD(MetadataSize);
  }
};

static_assert(sizeof(RDMP_HEADER64) == 0x20, "Invalid size for RDMP_HEADER64");

struct KERNEL_RDMP_HEADER64 {
  RDMP_HEADER64 Hdr;
  uint64_t __Unknown1;
  uint64_t __Unknown2;
  std::array<uint8_t, 1> Bitmap;
};

static_assert(sizeof(KERNEL_RDMP_HEADER64) == 0x30 + 1,
              "Invalid size for KERNEL_RDMP_HEADER64");

static_assert(offsetof(KERNEL_RDMP_HEADER64, Bitmap) == 0x30,
              "Invalid offset for KERNEL_RDMP_HEADER64");

struct FULL_RDMP_HEADER64 {
  RDMP_HEADER64 Hdr;
  uint32_t NumberOfRanges;
  uint16_t __Unknown1;
  uint16_t __Unknown2;
  uint64_t TotalNumberOfPages;
  std::array<uint8_t, 1> Bitmap;
};

static_assert(sizeof(FULL_RDMP_HEADER64) == 0x30 + 1,
              "Invalid size for FULL_RDMP_HEADER64");

static_assert(offsetof(FULL_RDMP_HEADER64, Bitmap) == 0x30,
              "Invalid offset for FULL_RDMP_HEADER64");

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
  std::array<uint128_t, 8> FloatRegisters;
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

  std::array<uint128_t, 26> VectorRegister;
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
  std::array<uint64_t, 15> ExceptionInformation;

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

union DUMP_FILE_ATTRIBUTES {
  struct DUMP_FILE_ATTRIBUTES_0 {
    uint32_t _bitfield;
  } Anonymous;
  uint32_t Attributes;
};

//
// Adjusted C struct for `DUMP_HEADERS64` from MS Rust docs. Padding
// adjustment added from reversing `nt!IoFillDumpHeader`.
//
// @link
// https://microsoft.github.io/windows-docs-rs/doc/windows/Win32/System/Diagnostics/Debug/struct.DUMP_HEADER64.html#structfield.DumpType
//

struct HEADER64 {
  static constexpr uint32_t ExpectedSignature = 0x45474150; // 'EGAP'
  static constexpr uint32_t ExpectedValidDump = 0x34365544; // '46UD'

  /* 0x0000 */ uint32_t Signature;
  /* 0x0004 */ uint32_t ValidDump;
  /* 0x0008 */ uint32_t MajorVersion;
  /* 0x000c */ uint32_t MinorVersion;
  /* 0x0010 */ uint64_t DirectoryTableBase;
  /* 0x0018 */ uint64_t PfnDatabase;
  /* 0x0020 */ uint64_t PsLoadedModuleList;
  /* 0x0028 */ uint64_t PsActiveProcessHead;
  /* 0x0030 */ uint32_t MachineImageType;
  /* 0x0034 */ uint32_t NumberProcessors;
  /* 0x0038 */ uint32_t BugCheckCode;
  /* 0x003c */ uint32_t __Padding0;
  /* 0x0040 */ std::array<uint64_t, 4> BugCheckCodeParameters;
  /* 0x0060 */ std::array<uint8_t, 32> VersionUser;
  /* 0x0080 */ uint64_t KdDebuggerDataBlock;
  /* 0x0088 */ union DUMP_HEADER64_0 {
    PHYSMEM_DESC PhysicalMemoryBlock;
    std::array<uint8_t, 700> PhysicalMemoryBlockBuffer;
  } u1;
  /* 0x0344 */ uint32_t __Padding1;
  /* 0x0348 */ union CONTEXT_RECORD64_0 {
    CONTEXT ContextRecord;
    std::array<uint8_t, 3000> ContextRecordBuffer;
  } u2;
  /* 0x0f00 */ EXCEPTION_RECORD64 Exception;
  /* 0x0f98 */ DumpType_t DumpType;
  /* 0x0f9c */ uint32_t __Padding2;
  /* 0x0fa0 */ int64_t RequiredDumpSpace;
  /* 0x0fa8 */ int64_t SystemTime;
  /* 0x0fb0 */ std::array<uint8_t, 128> Comment;
  /* 0x1030 */ int64_t SystemUpTime;
  /* 0x1038 */ uint32_t MiniDumpFields;
  /* 0x103c */ uint32_t SecondaryDataState;
  /* 0x1040 */ uint32_t ProductType;
  /* 0x1044 */ uint32_t SuiteMask;
  /* 0x1048 */ uint32_t WriterStatus;
  /* 0x104c */ uint8_t Unused1;
  /* 0x104d */ uint8_t KdSecondaryVersion;
  /* 0x104e */ std::array<uint8_t, 2> Unused;
  /* 0x1050 */ DUMP_FILE_ATTRIBUTES Attributes;
  /* 0x1054 */ uint32_t BootId;
  /* 0x1058 */ std::array<uint8_t, 4008> _reserved0;

  union {
    // When dealing with a `DumpType_t::FullDump`, the content of the physical
    // memory directly follows, there's no extra header like with the BMP/RDMP
    // dumps (see `KernelDumpParser::BuildPhysmemFullDump`).
    BMP_HEADER64 BmpHeader;
    KERNEL_RDMP_HEADER64 RdmpHeader;
    FULL_RDMP_HEADER64 FullRdmpHeader;
  } u3;

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

    switch (DumpType) {
    case DumpType_t::FullDump: {
      if (!u1.PhysicalMemoryBlock.LooksGood()) {
        printf("The PhysicalMemoryBlockBuffer looks wrong.\n");
        return false;
      }
      break;
    }

    case DumpType_t::LiveKernelBitmapDump:
    case DumpType_t::BMPDump: {
      if (!u3.BmpHeader.LooksGood()) {
        printf("The BmpHeader looks wrong.\n");
        return false;
      }
      break;
    }

    case DumpType_t::KernelAndUserMemoryDump:
    case DumpType_t::KernelMemoryDump: {
      if (!u3.RdmpHeader.Hdr.LooksGood()) {
        printf("The RdmpHeader looks wrong.\n");
        return false;
      }
      break;
    }

    case DumpType_t::CompleteMemoryDump: {
      if (!u3.FullRdmpHeader.Hdr.LooksGood()) {
        printf("The RdmpHeader looks wrong.\n");
        return false;
      }
      break;
    }

    case DumpType_t::MiniDump: {
      printf("Unsupported type %s (%#x).\n", DumpTypeToString(DumpType).data(),
             uint32_t(DumpType));
      return false;
    }

    default: {
      printf("Unknown Type %#x.\n", uint32_t(DumpType));
      return false;
    }
    }

    //
    // Integrity check the CONTEXT record.
    //

    if (!u2.ContextRecord.LooksGood()) {
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
    DISPLAY_FIELD_OFFSET(BugCheckCodeParameters);
    DISPLAY_FIELD(KdDebuggerDataBlock);
    DISPLAY_FIELD_OFFSET(u1.PhysicalMemoryBlockBuffer);
    u1.PhysicalMemoryBlock.Show(Prefix + 2);
    DISPLAY_FIELD_OFFSET(u2.ContextRecordBuffer);
    u2.ContextRecord.Show(Prefix + 2);
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
      DISPLAY_FIELD_OFFSET(u3.BmpHeader);
      u3.BmpHeader.Show();
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

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif //__GNUC__
static_assert(offsetof(HEADER64, Signature) == 0x00,
              "The offset of KdDebuggerDataBlock looks wrong.");

static_assert(offsetof(HEADER64, BugCheckCodeParameters) == 0x40,
              "The offset of KdDebuggerDataBlock looks wrong.");

static_assert(offsetof(HEADER64, KdDebuggerDataBlock) == 0x80,
              "The offset of KdDebuggerDataBlock looks wrong.");

static_assert(offsetof(HEADER64, u2.ContextRecord) == 0x348,
              "The offset of ContextRecord looks wrong.");

static_assert(offsetof(HEADER64, Exception) == 0xf00,
              "The offset of Exception looks wrong.");

static_assert(offsetof(HEADER64, Comment) == 0xfb0,
              "The offset of Comment looks wrong.");

static_assert(offsetof(HEADER64, u3.BmpHeader) == 0x2000,
              "The offset of BmpHeaders looks wrong.");
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif //__GNUC__

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