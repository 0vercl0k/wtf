// Axel '0vercl0k' Souchet - July 10 2021
#include "backend.h"
#include "targets.h"
#include <fmt/format.h>

namespace fs = std::filesystem;

namespace Hevd {

constexpr bool LoggingOn = false;

template <typename... Args_t>
void DebugPrint(const char *Format, const Args_t &...args) {
  if constexpr (LoggingOn) {
    fmt::print("Hevd: ");
    fmt::print(Format, args...);
  }
}

bool InsertTestcase(const uint8_t *Buffer, const size_t BufferSize) {
  if (BufferSize < sizeof(uint32_t)) {
    return true;
  }

  const uint32_t Ioctl = *(uint32_t *)Buffer;
  const size_t IoctlBufferSize = BufferSize - sizeof(uint32_t);
  const uint8_t *IoctlBuffer = Buffer + sizeof(uint32_t);
  if (IoctlBufferSize > 1024) {
    return true;
  }

  //  DeviceIoControl(
  //    H,
  //    0xdeadbeef,
  //    Buffer.data(),
  //    Buffer.size(),
  //    Buffer.data(),
  //    Buffer.size(),
  //    &Returned,
  //    nullptr
  //);

  g_Backend->Rdx(Ioctl);
  const Gva_t IoctlBufferPtr = Gva_t(g_Backend->R8());
  if (!g_Backend->VirtWriteDirty(IoctlBufferPtr, IoctlBuffer,
                                 IoctlBufferSize)) {
    DebugPrint("VirtWriteDirty failed\n");
    return false;
  }

  g_Backend->R9(IoctlBufferSize);
  const Gva_t Rsp = Gva_t(g_Backend->Rsp());
  const Gva_t OutBufferSizePtr = Rsp + Gva_t(4 * sizeof(uint64_t));
  if (!g_Backend->VirtWriteStructDirty(OutBufferSizePtr, &IoctlBufferSize)) {
    DebugPrint("VirtWriteStructDirty failed\n");
    return false;
  }

  return true;
}

bool Init(const Options_t &Opts, const CpuState_t &) {
  const Gva_t Rip = Gva_t(g_Backend->Rip());
  const Gva_t AfterCall = Rip + Gva_t(6);
  if (!g_Backend->SetBreakpoint(AfterCall, [](Backend_t *Backend) {
        DebugPrint("Back from kernel!\n");
        Backend->Stop(Ok_t());
      })) {
    DebugPrint("Failed to SetBreakpoint AfterCall\n");
    return false;
  }

  if (!g_Backend->SetBreakpoint("nt!DbgPrintEx", [](Backend_t *Backend) {
        const Gva_t FormatPtr = Gva_t(Backend->R8());
        const std::string &Format = Backend->VirtReadString(FormatPtr);
        DebugPrint("DbgPrintEx: {}", Format);
        Backend->SimulateReturnFromFunction(0);
      })) {
    DebugPrint("Failed to SetBreakpoint DbgPrintEx\n");
    return false;
  }

  // kd> ub fffff805`3b8287c4 l1
  // nt!ExGenRandom+0xe0:
  // fffff805`3b8287c0 480fc7f2        rdrand  rdx
  const Gva_t ExGenRandom = Gva_t(g_Dbg.GetSymbol("nt!ExGenRandom") + 0xe4);
  if (!g_Backend->SetBreakpoint(ExGenRandom, [](Backend_t *Backend) {
        DebugPrint("Hit ExGenRandom!\n");
        Backend->Rdx(Backend->Rdrand());
      })) {
    return false;
  }

  //
  // Avoid the fuzzer to spin out of control if we mess-up real bad.
  //

  if (!g_Backend->SetCrashBreakpoint("nt!KeBugCheck2")) {
    return false;
  }

  if (!g_Backend->SetBreakpoint("nt!SwapContext", [](Backend_t *Backend) {
        DebugPrint("nt!SwapContext\n");
        Backend->Stop(Cr3Change_t());
      })) {
    return false;
  }

  return true;
}

bool Restore() { return true; }

//
// Register the target.
//

Target_t Hevd("hevd", Init, InsertTestcase, Restore);

} // namespace Hevd