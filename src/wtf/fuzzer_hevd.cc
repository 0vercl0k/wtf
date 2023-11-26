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
    fmt::print(fmt::runtime(Format), args...);
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
    return false;
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
  const auto &OutBufferSizePtr = g_Backend->GetArgAddress(5);
  if (!g_Backend->VirtWriteStructDirty(OutBufferSizePtr, &IoctlBufferSize)) {
    DebugPrint("VirtWriteStructDirty failed\n");
    return false;
  }

  return true;
}

bool Init(const Options_t &Opts, const CpuState_t &) {
  //
  // Stop the test-case once we return back from the call [DeviceIoControl]
  //

  const Gva_t Rip = Gva_t(g_Backend->Rip());
  const Gva_t AfterCall = Rip + Gva_t(6);
  if (!g_Backend->SetBreakpoint(AfterCall, [](Backend_t *Backend) {
        DebugPrint("Back from kernel!\n");
        Backend->Stop(Ok_t());
      })) {
    DebugPrint("Failed to SetBreakpoint AfterCall\n");
    return false;
  }

  //
  // NOP the calls to DbgPrintEx.
  //

  if (!g_Backend->SetBreakpoint("nt!DbgPrintEx", [](Backend_t *Backend) {
        const Gva_t FormatPtr = Backend->GetArgGva(2);
        const std::string &Format = Backend->VirtReadString(FormatPtr);
        DebugPrint("DbgPrintEx: {}", Format);
        Backend->SimulateReturnFromFunction(0);
      })) {
    DebugPrint("Failed to SetBreakpoint DbgPrintEx\n");
    return false;
  }

  //
  // Make ExGenRandom deterministic.
  //
  // kd> ub fffff805`3b8287c4 l1
  // nt!ExGenRandom+0xe0:
  // fffff805`3b8287c0 480fc7f2        rdrand  rdx
  const Gva_t ExGenRandom = Gva_t(g_Dbg.GetSymbol("nt!ExGenRandom") + 0xe0 + 4);
  if (g_Backend->VirtRead4(ExGenRandom - Gva_t(4)) != 0xf2c70f48) {
    fmt::print("It seems that nt!ExGenRandom's code has changed, update the "
               "offset!\n");
    return false;
  }

  if (!g_Backend->SetBreakpoint(ExGenRandom, [](Backend_t *Backend) {
        DebugPrint("Hit ExGenRandom!\n");
        Backend->Rdx(Backend->Rdrand());
      })) {
    return false;
  }

  //
  // Catch bugchecks.
  //

  if (!g_Backend->SetBreakpoint("nt!KeBugCheck2", [](Backend_t *Backend) {
        const uint64_t BCode = Backend->GetArg(0);
        const uint64_t B0 = Backend->GetArg(1);
        const uint64_t B1 = Backend->GetArg(2);
        const uint64_t B2 = Backend->GetArg(3);
        const uint64_t B3 = Backend->GetArg(4);
        const uint64_t B4 = Backend->GetArg(5);
        const std::string Filename =
            fmt::format("crash-{:#x}-{:#x}-{:#x}-{:#x}-{:#x}-{:#x}", BCode, B0,
                        B1, B2, B3, B4);
        DebugPrint("KeBugCheck2: {}\n", Filename);
        Backend->Stop(Crash_t(Filename));
      })) {
    return false;
  }

  //
  // Catch context-switches.
  //

  if (!g_Backend->SetBreakpoint("nt!SwapContext", [](Backend_t *Backend) {
        DebugPrint("nt!SwapContext\n");
        Backend->Stop(Cr3Change_t());
      })) {
    return false;
  }

  return true;
}

//
// Register the target.
//

Target_t Hevd("hevd", Init, InsertTestcase);

} // namespace Hevd