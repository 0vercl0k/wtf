// Axel '0vercl0k' Souchet - May 25 2020
#include "backend.h"
#include "debugger.h"
#include "nt.h"
#include "utils.h"
#include <fmt/format.h>
#include <string_view>

Backend_t *g_Backend = nullptr;

bool Backend_t::SetTraceFile(const fs::path &, const TraceType_t) {
  fmt::print("SetTraceFile not implemented.\n");
  return true;
}

bool Backend_t::PhysWrite(const Gpa_t Gpa, const uint8_t *Buffer,
                          const uint64_t BufferSize, const bool Dirty) {
  uint8_t *Dst = PhysTranslate(Gpa);
  memcpy(Dst, Buffer, BufferSize);

  if (Dirty) {
    const Gpa_t End = Gpa + Gpa_t(BufferSize);
    for (Gpa_t Cur = Gpa; Cur < End; Cur += Gpa_t(Page::Size)) {
      DirtyGpa(Cur);
    }
  }
  return true;
}

bool Backend_t::VirtRead(const Gva_t Gva, uint8_t *Buffer,
                         const uint64_t BufferSize) const {
  uint64_t Size = BufferSize;
  Gva_t CurrentGva = Gva;
  while (Size > 0) {
    Gpa_t Gpa;
    const bool Translate =
        VirtTranslate(CurrentGva, Gpa, MemoryValidate_t::ValidateRead);

    if (!Translate) {
      fmt::print("Translation of GVA {:#x} failed\n", CurrentGva);
      return false;
    }

    const Gva_t GvaOffset = CurrentGva.Offset();
    const uint64_t BytesReadable = Page::Size - GvaOffset.U64();
    const uint64_t Size2Read = std::min(Size, BytesReadable);
    const uint8_t *Hva = PhysTranslate(Gpa);
    memcpy(Buffer, Hva, Size2Read);
    Size -= Size2Read;
    CurrentGva += Gva_t(Size2Read);
    Buffer += Size2Read;
  }

  return true;
}

uint16_t Backend_t::VirtRead2(const Gva_t Gva) const {
  uint16_t Ret = 0;
  if (!VirtReadStruct(Gva, &Ret)) {
    __debugbreak();
  }
  return Ret;
}

uint32_t Backend_t::VirtRead4(const Gva_t Gva) const {
  uint32_t Ret = 0;
  if (!VirtReadStruct(Gva, &Ret)) {
    __debugbreak();
  }
  return Ret;
}

uint64_t Backend_t::VirtRead8(const Gva_t Gva) const {
  uint64_t Ret = 0;
  if (!VirtReadStruct(Gva, &Ret)) {
    __debugbreak();
  }
  return Ret;
}

Gva_t Backend_t::VirtReadGva(const Gva_t Gva) const {
  return Gva_t(VirtRead8(Gva));
}

Gpa_t Backend_t::VirtReadGpa(const Gva_t Gva) const {
  return Gpa_t(VirtRead8(Gva));
}

std::string Backend_t::VirtReadString(const Gva_t Gva,
                                      const uint64_t MaxLength) const {
  return VirtReadBasicString<std::string::value_type>(Gva, MaxLength);
}
std::u16string Backend_t::VirtReadWideString(const Gva_t Gva,
                                             const uint64_t MaxLength) const {
  return VirtReadBasicString<std::u16string::value_type>(Gva, MaxLength);
}

bool Backend_t::VirtWrite(const Gva_t Gva, const uint8_t *Buffer,
                          const uint64_t BufferSize, const bool Dirty) {
  uint64_t Size = BufferSize;
  Gva_t CurrentGva = Gva;
  while (Size > 0) {
    Gpa_t Gpa;
    // XXX: Reenable ValidateReadWrite when bug is figured out.
    const bool Translate = VirtTranslate(
        CurrentGva, Gpa, MemoryValidate_t::ValidateRead /*Write*/);

    if (!Translate) {
      fmt::print("Translation of GVA {:#x} failed\n", CurrentGva);
      __debugbreak();
      return false;
    }

    const Gva_t GvaOffset = CurrentGva.Offset();
    const uint64_t BytesWriteable = Page::Size - GvaOffset.U64();
    const uint64_t Size2Write = std::min(Size, BytesWriteable);
    uint8_t *Hva = PhysTranslate(Gpa);
    memcpy(Hva, Buffer, Size2Write);
    Size -= Size2Write;
    CurrentGva += Gva_t(Size2Write);
    Buffer += Size2Write;

    if (Dirty) {
      DirtyGpa(Gpa);
    }
  }

  return true;
}

bool Backend_t::VirtWriteDirty(const Gva_t Gva, const uint8_t *Buffer,
                               const uint64_t BufferSize) {
  return VirtWrite(Gva, Buffer, BufferSize, true);
}

bool Backend_t::SimulateReturnFromFunction(const uint64_t Return) {
  //
  // Set return value.
  //

  Rax(Return);

  const uint64_t Stack = Rsp();
  const uint64_t SavedReturnAddress = VirtRead8(Gva_t(Stack));

  //
  // Eat up the saved return address.
  //

  Rsp(Stack + 8);
  Rip(SavedReturnAddress);
  return true;
}

bool Backend_t::SimulateReturnFrom32bitFunction(
    const uint32_t Return, const uint32_t StdcallArgsCount) {
  //
  // Set return value.
  //

  Rax(Return);

  const uint64_t Stack = Rsp();
  const uint32_t SavedReturnAddress = VirtRead4(Gva_t(Stack));

  //
  // Eat up the saved return address.
  //

  Rsp(Stack + (4 + (4 * StdcallArgsCount)));
  Rip(SavedReturnAddress);
  return true;
}

Gva_t Backend_t::GetArgAddress(const uint64_t Idx) {
  if (Idx <= 3) {
    fmt::print("The first four arguments are stored in registers (@rcx, @rdx, "
               "@r8, @r9) which means you cannot get their addresses.\n");
    std::abort();
  }

  return Gva_t(Rsp() + (8 + (Idx * 8)));
}

uint64_t Backend_t::GetArg(const uint64_t Idx) {
  switch (Idx) {
  case 0:
    return Rcx();
  case 1:
    return Rdx();
  case 2:
    return R8();
  case 3:
    return R9();
  default: {
    return VirtRead8(GetArgAddress(Idx));
  }
  }
}

Gva_t Backend_t::GetArgGva(const uint64_t Idx) { return Gva_t(GetArg(Idx)); }

std::pair<uint64_t, Gva_t> Backend_t::GetArgAndAddress(const uint64_t Idx) {
  return {GetArg(Idx), GetArgAddress(Idx)};
}

std::pair<Gva_t, Gva_t> Backend_t::GetArgAndAddressGva(const uint64_t Idx) {
  return {GetArgGva(Idx), GetArgAddress(Idx)};
}

bool Backend_t::SaveCrash(const Gva_t ExceptionAddress,
                          const uint32_t ExceptionCode) {
  const auto ExceptionCodeStr = ExceptionCodeToStr(ExceptionCode);
  const std::string Filename =
      fmt::format("crash-{}-{:#x}", ExceptionCodeStr, ExceptionAddress);

  Stop(Crash_t(Filename));
  return true;
}

bool Backend_t::SetBreakpoint(const char *Symbol,
                              const BreakpointHandler_t Handler) {
  const Gva_t Gva = Gva_t(g_Dbg.GetSymbol(Symbol));
  if (Gva == Gva_t(0)) {
    fmt::print("Could not set a breakpoint at {}.\n", Symbol);
    return false;
  }

  return SetBreakpoint(Gva, Handler);
}

bool Backend_t::SetCrashBreakpoint(const Gva_t Gva) {
  auto CrashBreakpointHandler = [](Backend_t *Backend) {
    Backend->Stop(Crash_t());
  };

  return SetBreakpoint(Gva, CrashBreakpointHandler);
}

bool Backend_t::SetCrashBreakpoint(const char *Symbol) {
  auto CrashBreakpointHandler = [](Backend_t *Backend) {
    Backend->Stop(Crash_t());
  };

  return SetBreakpoint(Symbol, CrashBreakpointHandler);
}

uint64_t Backend_t::Rsp() { return GetReg(Registers_t::Rsp); }
void Backend_t::Rsp(const uint64_t Value) { SetReg(Registers_t::Rsp, Value); }
void Backend_t::Rsp(const Gva_t Value) { Rsp(Value.U64()); }

uint64_t Backend_t::Rbp() { return GetReg(Registers_t::Rbp); }
void Backend_t::Rbp(const uint64_t Value) { SetReg(Registers_t::Rbp, Value); }
void Backend_t::Rbp(const Gva_t Value) { Rbp(Value.U64()); }

uint64_t Backend_t::Rip() { return GetReg(Registers_t::Rip); }
void Backend_t::Rip(const uint64_t Value) { SetReg(Registers_t::Rip, Value); }
void Backend_t::Rip(const Gva_t Value) { Rip(Value.U64()); }

uint64_t Backend_t::Rax() { return GetReg(Registers_t::Rax); }
void Backend_t::Rax(const uint64_t Value) { SetReg(Registers_t::Rax, Value); }
void Backend_t::Rax(const Gva_t Value) { Rax(Value.U64()); }

uint64_t Backend_t::Rbx() { return GetReg(Registers_t::Rbx); }
void Backend_t::Rbx(const uint64_t Value) { SetReg(Registers_t::Rbx, Value); }
void Backend_t::Rbx(const Gva_t Value) { Rbx(Value.U64()); }

uint64_t Backend_t::Rcx() { return GetReg(Registers_t::Rcx); }
void Backend_t::Rcx(const uint64_t Value) { SetReg(Registers_t::Rcx, Value); }
void Backend_t::Rcx(const Gva_t Value) { Rcx(Value.U64()); }

uint64_t Backend_t::Rdx() { return GetReg(Registers_t::Rdx); }
void Backend_t::Rdx(const uint64_t Value) { SetReg(Registers_t::Rdx, Value); }
void Backend_t::Rdx(const Gva_t Value) { Rdx(Value.U64()); }

uint64_t Backend_t::Rsi() { return GetReg(Registers_t::Rsi); }
void Backend_t::Rsi(const uint64_t Value) { SetReg(Registers_t::Rsi, Value); }
void Backend_t::Rsi(const Gva_t Value) { Rsi(Value.U64()); }

uint64_t Backend_t::Rdi() { return GetReg(Registers_t::Rdi); }
void Backend_t::Rdi(const uint64_t Value) { SetReg(Registers_t::Rdi, Value); }
void Backend_t::Rdi(const Gva_t Value) { Rdi(Value.U64()); }

uint64_t Backend_t::R8() { return GetReg(Registers_t::R8); }
void Backend_t::R8(const uint64_t Value) { SetReg(Registers_t::R8, Value); }
void Backend_t::R8(const Gva_t Value) { R8(Value.U64()); }

uint64_t Backend_t::R9() { return GetReg(Registers_t::R9); }
void Backend_t::R9(const uint64_t Value) { SetReg(Registers_t::R9, Value); }
void Backend_t::R9(const Gva_t Value) { R9(Value.U64()); }

uint64_t Backend_t::R10() { return GetReg(Registers_t::R10); }
void Backend_t::R10(const uint64_t Value) { SetReg(Registers_t::R10, Value); }
void Backend_t::R10(const Gva_t Value) { R10(Value.U64()); }

uint64_t Backend_t::R11() { return GetReg(Registers_t::R11); }
void Backend_t::R11(const uint64_t Value) { SetReg(Registers_t::R11, Value); }
void Backend_t::R11(const Gva_t Value) { R11(Value.U64()); }

uint64_t Backend_t::R12() { return GetReg(Registers_t::R12); }
void Backend_t::R12(const uint64_t Value) { SetReg(Registers_t::R12, Value); }
void Backend_t::R12(const Gva_t Value) { R12(Value.U64()); }

uint64_t Backend_t::R13() { return GetReg(Registers_t::R13); }
void Backend_t::R13(const uint64_t Value) { SetReg(Registers_t::R13, Value); }
void Backend_t::R13(const Gva_t Value) { R13(Value.U64()); }

uint64_t Backend_t::R14() { return GetReg(Registers_t::R14); }
void Backend_t::R14(const uint64_t Value) { SetReg(Registers_t::R14, Value); }
void Backend_t::R14(const Gva_t Value) { R14(Value.U64()); }

uint64_t Backend_t::R15() { return GetReg(Registers_t::R15); }
void Backend_t::R15(const uint64_t Value) { SetReg(Registers_t::R15, Value); }
void Backend_t::R15(const Gva_t Value) { R15(Value.U64()); }

void Backend_t::PrintRegisters() {
  const uint64_t Rax = GetReg(Registers_t::Rax), Rbx = GetReg(Registers_t::Rbx),
                 Rcx = GetReg(Registers_t::Rcx);
  fmt::print("rax={:016x} rbx={:016x} rcx={:016x}\n", Rax, Rbx, Rcx);

  const uint64_t Rdx = GetReg(Registers_t::Rdx), Rsi = GetReg(Registers_t::Rsi),
                 Rdi = GetReg(Registers_t::Rdi);
  fmt::print("rdx={:016x} rsi={:016x} rdi={:016x}\n", Rdx, Rsi, Rdi);

  const uint64_t Rip = GetReg(Registers_t::Rip), Rsp = GetReg(Registers_t::Rsp),
                 Rbp = GetReg(Registers_t::Rbp);
  fmt::print("rip={:016x} rsp={:016x} rbp={:016x}\n", Rip, Rsp, Rbp);

  const uint64_t R8 = GetReg(Registers_t::R8), R9 = GetReg(Registers_t::R9),
                 R10 = GetReg(Registers_t::R10);
  fmt::print(" r8={:016x}  r9={:016x} r10={:016x}\n", R8, R9, R10);

  const uint64_t R11 = GetReg(Registers_t::R11), R12 = GetReg(Registers_t::R12),
                 R13 = GetReg(Registers_t::R13);
  fmt::print("r11={:016x} r12={:016x} r13={:016x}\n", R11, R12, R13);

  const uint64_t R14 = GetReg(Registers_t::R14), R15 = GetReg(Registers_t::R15);
  fmt::print("r14={:016x} r15={:016x}\n", R14, R15);
}
