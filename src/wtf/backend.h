// Axel '0vercl0k' Souchet - May 16 2020
#pragma once
#include "globals.h"
#include "platform.h"
#include "ram.h"
#include "tsl/robin_set.h"
#include <fmt/format.h>
#include <optional>
#include <string>
#include <variant>

struct Ok_t {
  constexpr std::string_view Name() const { return "ok"; }
};

struct Timedout_t {
  constexpr std::string_view Name() const { return "timedout"; }
};

struct Cr3Change_t {
  constexpr std::string_view Name() const { return "cr3"; }
};

struct Crash_t {
  std::string CrashName;
  Crash_t() = default;
  explicit Crash_t(const std::string &CrashName_) : CrashName(CrashName_) {}
  std::string_view Name() const { return "crash"; }
};

using TestcaseResult_t = std::variant<Ok_t, Timedout_t, Cr3Change_t, Crash_t>;

//
// Page faults error bits.
//

enum PfError_t {
  ErrorPresent = 1 << 0,
  ErrorWrite = 1 << 1,
  ErrorUser = 1 << 2,
  ErrorReservedWrite = 1 << 3,
  ErrorInstructionFetch = 1 << 4
};

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
  };

  uint64_t AsUINT64;

  MMPTE_HARDWARE(const uint64_t Value) : AsUINT64(Value) {}

  void Print() {
    fmt::print("PTE: {:#x}\n", AsUINT64);
    fmt::print("PTE.Present: {:#x}\n", uint64_t(Present));
    fmt::print("PTE.Write: {:#x}\n", uint64_t(Write));
    fmt::print("PTE.UserAccessible: {:#x}\n", uint64_t(UserAccessible));
    fmt::print("PTE.WriteThrough: {:#x}\n", uint64_t(WriteThrough));
    fmt::print("PTE.CacheDisable: {:#x}\n", uint64_t(CacheDisable));
    fmt::print("PTE.Accessed: {:#x}\n", uint64_t(Accessed));
    fmt::print("PTE.Dirty: {:#x}\n", uint64_t(Dirty));
    fmt::print("PTE.LargePage: {:#x}\n", uint64_t(LargePage));
    fmt::print("PTE.Available: {:#x}\n", uint64_t(Available));
    fmt::print("PTE.PageFrameNumber: {:#x}\n", uint64_t(PageFrameNumber));
    fmt::print("PTE.ReservedForHardware: {:#x}\n",
               uint64_t(ReservedForHardware));
    fmt::print("PTE.ReservedForSoftware: {:#x}\n",
               uint64_t(ReservedForSoftware));
    fmt::print("PTE.NoExecute: {:#x}\n", uint64_t(NoExecute));
  }
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
  };
  uint64_t AsUINT64;
  VIRTUAL_ADDRESS(const uint64_t Value) : AsUINT64(Value) {}
};

static_assert(sizeof(MMPTE_HARDWARE) == 8);

class Backend_t;
using BreakpointHandler_t = void (*)(Backend_t *);

//
// When doing memory translation, we can ask for validating certains
// permissions.
//

enum class MemoryValidate_t : uint32_t {
  ValidateRead = 1,
  ValidateWrite = 2,
  ValidateExecute = 4,
  ValidateReadWrite = ValidateRead | ValidateWrite,
  ValidateReadExecute = ValidateRead | ValidateExecute
};

inline bool operator&(const MemoryValidate_t &A, const MemoryValidate_t &B) {
  return (uint32_t(A) & uint32_t(B)) != 0;
}

//
// Registers.
//

enum class Registers_t : uint32_t {
  Rax,
  Rbx,
  Rcx,
  Rdx,
  Rsi,
  Rdi,
  Rip,
  Rsp,
  Rbp,
  R8,
  R9,
  R10,
  R11,
  R12,
  R13,
  R14,
  R15,
  Rflags,
  Cr2,
  Cr3
};

//
// The backend interface. A backend runs test-cases in a ~deterministic
// environment. It can be in a VM, an emulator, etc.
//

class Backend_t {
public:
  virtual ~Backend_t() = default;

  //
  // Initialize the backend. A CPU state is provided in order for the backend to
  // be able to set-up memory accesses / translations so that the callers are
  // able to set breakpoints or read memory before starting fuzzing.
  //

  virtual bool Initialize(const Options_t &Opts,
                          const CpuState_t &CpuState) = 0;

  //
  // Run a test case.
  //

  virtual std::optional<TestcaseResult_t> Run(const uint8_t *Buffer,
                                              const uint64_t BufferSize) = 0;

  //
  // Restore state.
  //

  virtual bool Restore(const CpuState_t &CpuState) = 0;

  //
  // Stop the current test case from executing (can be used by breakpoints).
  //

  virtual void Stop(const TestcaseResult_t &Res) = 0;

  //
  // Set a limit to avoid infinite loops test cases. The limit depends on the
  // backend for now, for example the Bochscpu backend interprets the limit as
  // an instruction limit but the WinHV backend interprets it as a time limit.
  //

  virtual void SetLimit(const uint64_t Limit) = 0;

  //
  // Registers.
  //

  virtual uint64_t GetReg(const Registers_t Reg) = 0;
  virtual uint64_t SetReg(const Registers_t Reg, const uint64_t Value) = 0;

  //
  // Non-determinism.
  //

  virtual uint64_t Rdrand() = 0;

  //
  // Some backend collect stats for a test case run; this displays it.
  //

  virtual void PrintRunStats() = 0;

  //
  // Tracing.
  //

  virtual bool SetTraceFile(const fs::path &TestcaseTracePath,
                            const TraceType_t TraceType);

  //
  // Breakpoints.
  //

  virtual bool SetBreakpoint(const Gva_t Gva,
                             const BreakpointHandler_t Handler) = 0;

  //
  // Virtual memory access.
  //

  //
  // Dirty a GPA.
  //

  virtual bool DirtyGpa(const Gpa_t Gpa) = 0;

  //
  // GVA->GPA translation.
  //

  virtual bool VirtTranslate(const Gva_t Gva, Gpa_t &Gpa,
                             const MemoryValidate_t Validate) const = 0;

  //
  // GPA->HVA translation.
  //

  virtual uint8_t *PhysTranslate(const Gpa_t Gpa) const = 0;

  //
  // Page faults a GVA range. This basically injects a #PF in the guest.
  //

  virtual bool PageFaultsMemoryIfNeeded(const Gva_t Gva,
                                        const uint64_t Size) = 0;

  //
  // Save the current test-case as a crashing input.
  //

  bool SaveCrash(const Gva_t ExceptionAddress, const uint32_t ExceptionCode);

  //
  // The rest is the facilities we derive from the above.
  //

  //
  // Set a breakpoint on a symbol.
  //

  bool SetBreakpoint(const char *Symbol, const BreakpointHandler_t Handler);

  //
  // Set a crash breakpoint on a symbol.
  //

  bool SetCrashBreakpoint(const char *Symbol);

  //
  // Set a crash breakpoint on an address.
  //

  bool SetCrashBreakpoint(const Gva_t Gva);

  //
  // Write in physical memory. Optionally track dirtiness on the memory range.
  //

  bool PhysWrite(const Gpa_t Gpa, const uint8_t *Buffer,
                 const uint64_t BufferSize, const bool Dirty = false);

  //
  // Read virtual memory.
  //

  bool VirtRead(const Gva_t Gva, uint8_t *Buffer,
                const uint64_t BufferSize) const;

  //
  // Read structured data stored in virtual memory.
  //

  template <typename _Ty>
  bool VirtReadStruct(const Gva_t Gva, const _Ty *Buffer) const {
    return VirtRead(Gva, (uint8_t *)Buffer, sizeof(_Ty));
  }

  //
  // Read a uint32_t.
  //

  uint32_t VirtRead4(const Gva_t Gva) const;

  //
  // Read a uint64_t.
  //

  [[nodiscard]] uint64_t VirtRead8(const Gva_t Gva) const;
  [[nodiscard]] Gva_t VirtReadGva(const Gva_t Gva) const;
  [[nodiscard]] Gpa_t VirtReadGpa(const Gva_t Gva) const;

  //
  // Read a basic string.
  //

  template <typename _Ty>
  [[nodiscard]] std::basic_string<_Ty>
  VirtReadBasicString(const Gva_t StringGva, const uint64_t MaxLength) const {
    using BasicString_t = std::basic_string<_Ty>;
    using ValueType_t = typename BasicString_t::value_type;

    BasicString_t S;
    uint64_t Size = MaxLength;
    Gva_t Gva = StringGva;
    std::optional<uint8_t> Remainder;
    while (Size > 0) {

      //
      // Starting by getting a GPA off the current GVA.
      //

      Gpa_t Gpa;
      const bool Translate =
          VirtTranslate(Gva, Gpa, MemoryValidate_t::ValidateRead);

      if (!Translate) {
        fmt::print("VirtTranslate failed for GVA:{:#x}\n", Gva.U64());
        __debugbreak();
      }

      //
      // We now need to calculate how many bytes we should be reading. At most,
      // we read the entire page if Size allows us to.
      //

      const Gva_t GvaOffset = Gva.Offset();
      const uint64_t BytesReadable = Page::Size - GvaOffset.U64();
      const uint64_t Size2Read = std::min(Size, BytesReadable);

      //
      // Get the HVA.
      //

      const auto *Hva = (ValueType_t *)PhysTranslate(Gpa);

      //
      // Now read the physical memory, and populate the string.
      //

      if (Remainder) {
        const auto &HvaByte = (uint8_t *)Hva - 1;
        const ValueType_t Straddle =
            (HvaByte[0] << 8) | ValueType_t(*Remainder);
        S.push_back(Straddle);
        Remainder = {};
      }

      const uint64_t Characters = Size2Read / sizeof(ValueType_t);
      for (uint64_t Idx = 0; Idx < Characters; Idx++) {
        const auto &C = Hva[Idx];
        if (C == 0) {
          return S;
        }

        S.push_back(C);
      }

      //
      // Move forward!
      //

      Size -= Size2Read;
      Gva += Gva_t(Size2Read);

      //
      // The below takes care of an edge-case that can happen when |ValueType_t|
      // is |char16_t|. Basically, if you are trying to read a two-byte
      // character that straddles two virtual pages.
      // The way we handle that is that we store the last byte on this page in
      // |Remainder| and we offset |Gva| of one byte.
      //
      // We offset |Gva| of one byte to make it point to the beginning of a wide
      // character again. We'll just read one byte before to get the second byte
      // of the wide character that straddles the two pages.
      //

      const bool HasRemainder = (Size2Read % sizeof(ValueType_t)) != 0;
      if (HasRemainder) {

        //
        // Read the left-over byte which is right after all the characters we
        // read.
        //

        const auto &HvaByte = (uint8_t *)Hva;
        Remainder = HvaByte[Characters * sizeof(ValueType_t)];
        Gva += Gva_t(1);
      }
    }

    return S;
  }

  //
  // Read a basic_string<char>.
  //

  [[nodiscard]] std::string
  VirtReadString(const Gva_t Gva, const uint64_t MaxLength = 256) const;

  //
  // Read a basic_string<char16_t> (used to read wchar_t* in Windows guests).
  //

  [[nodiscard]] std::u16string
  VirtReadWideString(const Gva_t Gva, const uint64_t MaxLength = 256) const;

  //
  // Write in virtual memory. Optionally track dirtiness on the memory range.
  //

  bool VirtWrite(const Gva_t Gva, const uint8_t *Buffer,
                 const uint64_t BufferSize, const bool Dirty = false);

  //
  // Write structured data in virtual memory.
  //

  template <typename _Ty>
  bool VirtWriteStruct(const Gva_t Gva, const _Ty *Buffer) {
    return VirtWrite(Gva, (uint8_t *)Buffer, sizeof(_Ty));
  }

  //
  // Write in virtual memory with dirty tracking.
  //

  bool VirtWriteDirty(const Gva_t Gva, const uint8_t *Buffer,
                      const uint64_t BufferSize);

  //
  // Write structured data in virtual memory with dirty tracking.
  //

  template <typename _Ty>
  bool VirtWriteStructDirty(const Gva_t Gva, const _Ty *Buffer) {
    return VirtWriteDirty(Gva, (uint8_t *)Buffer, sizeof(_Ty));
  }

  //
  // Utility function to simulate a return from a function. This is useful from
  // a breakpoint to fake a return from a function that we want to skip / fake
  // the return result.
  //

  bool SimulateReturnFromFunction(const uint64_t Return);
  bool SimulateReturnFrom32bitFunction(const uint32_t Return,
                                       const uint32_t StdcallArgsCount = 0);

  //
  // Utility function that grabs function arguments according to the Windows x64
  // calling convention.
  //

  [[nodiscard]] uint64_t GetArg(const uint64_t Idx);
  [[nodiscard]] Gva_t GetArgGva(const uint64_t Idx);

  //
  // Utility function to get the address of a function argument. Oftentimes, you
  // need to overwrite an argument that isn't stored in registers which means
  // you need to calculate yourself where it is stored on the stack. This
  // function gives you its address.
  //

  [[nodiscard]] Gva_t GetArgAddress(const uint64_t Idx);
  [[nodiscard]] std::pair<uint64_t, Gva_t> GetArgAndAddress(const uint64_t Idx);
  [[nodiscard]] std::pair<Gva_t, Gva_t> GetArgAndAddressGva(const uint64_t Idx);


  //
  // Shortcuts to grab / set some registers.
  //

  [[nodiscard]] uint64_t Rsp();
  void Rsp(const uint64_t Value);
  void Rsp(const Gva_t Value);

  [[nodiscard]] uint64_t Rbp();
  void Rbp(const uint64_t Value);
  void Rbp(const Gva_t Value);

  [[nodiscard]] uint64_t Rip();
  void Rip(const uint64_t Value);
  void Rip(const Gva_t Value);

  [[nodiscard]] uint64_t Rax();
  void Rax(const uint64_t Value);
  void Rax(const Gva_t Value);

  [[nodiscard]] uint64_t Rbx();
  void Rbx(const uint64_t Value);
  void Rbx(const Gva_t Value);

  [[nodiscard]] uint64_t Rcx();
  void Rcx(const uint64_t Value);
  void Rcx(const Gva_t Value);

  [[nodiscard]] uint64_t Rdx();
  void Rdx(const uint64_t Value);
  void Rdx(const Gva_t Value);

  [[nodiscard]] uint64_t Rsi();
  void Rsi(const uint64_t Value);
  void Rsi(const Gva_t Value);

  [[nodiscard]] uint64_t Rdi();
  void Rdi(const uint64_t Value);
  void Rdi(const Gva_t Value);

  [[nodiscard]] uint64_t R8();
  void R8(const uint64_t Value);
  void R8(const Gva_t Value);

  [[nodiscard]] uint64_t R9();
  void R9(const uint64_t Value);
  void R9(const Gva_t Value);

  [[nodiscard]] uint64_t R10();
  void R10(const uint64_t Value);
  void R10(const Gva_t Value);

  [[nodiscard]] uint64_t R11();
  void R11(const uint64_t Value);
  void R11(const Gva_t Value);

  [[nodiscard]] uint64_t R12();
  void R12(const uint64_t Value);
  void R12(const Gva_t Value);

  [[nodiscard]] uint64_t R13();
  void R13(const uint64_t Value);
  void R13(const Gva_t Value);

  [[nodiscard]] uint64_t R14();
  void R14(const uint64_t Value);
  void R14(const Gva_t Value);

  [[nodiscard]] uint64_t R15();
  void R15(const uint64_t Value);
  void R15(const Gva_t Value);

  //
  // Gets the last new coverage generated by the last executed test-case.
  //

  virtual const tsl::robin_set<Gva_t> &LastNewCoverage() const = 0;

  //
  // Revokes code coverage.
  //

  virtual bool RevokeLastNewCoverage() = 0;

  //
  // Print the registers.
  //

  void PrintRegisters();
};

//
// This is the global backend instance.
//

extern Backend_t *g_Backend;
