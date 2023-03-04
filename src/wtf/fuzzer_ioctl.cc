// 1ndahous3 - March 4 2023
#include "backend.h"
#include "targets.h"
#include <fmt/format.h>

// 1. The target works with the state captured at the breakpoint at "ntdll!NtDeviceIoControlFile()".
// 2. It's better to capture a state with the maximum possible InputBufferLength.
// 3. For "MutateIoctl = true" tescases are manually created buffers: Ioctl (4 bytes) + InputBuffer ("InputBufferLength" bytes).

namespace Ioctl {

constexpr bool DebugLoggingOn = false;
constexpr bool MutateIoctl = true;

constexpr uint32_t IoctlSizeIfPresent = MutateIoctl ? sizeof(uint32_t) : 0;

template <typename... Args_t>
void Print(const char *Format, const Args_t &...args) {
  fmt::print("Ioctl: ");
  fmt::print(fmt::runtime(Format), args...);
}

template <typename... Args_t>
void DebugPrint(const char *Format, const Args_t &...args) {
  if constexpr (DebugLoggingOn) {
    fmt::print("Ioctl: ");
    fmt::print(fmt::runtime(Format), args...);
  }
}

bool InsertTestcase(const uint8_t *Buffer, const size_t BufferSize) {

  if constexpr (MutateIoctl) {
    if (BufferSize < sizeof(uint32_t)) {
      Print("The testcase buffer is too small: {} bytes\n", BufferSize);
      return false;
    }
  } else {
    if (BufferSize == 0) {
      Print("The testcase buffer is empty");
      return false;
    }
  }

  const uint8_t *IoctlBuffer = Buffer + IoctlSizeIfPresent;
  uint32_t IoctlBufferSize = BufferSize - IoctlSizeIfPresent;

  // ntdll!NtDeviceIoControlFile()
  // Ioctl:              @rsp + 0x30
  // InputBuffer:        @rsp + 0x38
  // InputBufferLength:  @rsp + 0x40

  const Gva_t Stack = Gva_t(g_Backend->Rsp());

  const Gva_t InputBufferLengthPtr = Stack + Gva_t(0x40);
  uint32_t CurrentIoctlBufferSize = g_Backend->VirtRead4(InputBufferLengthPtr);

  if (IoctlBufferSize > CurrentIoctlBufferSize) {
    DebugPrint("The testcase buffer is too large: {} ({:#x}) bytes, "
               "maximum for the state: {} ({:#x}) bytes \n",
               BufferSize, BufferSize,
               CurrentIoctlBufferSize + IoctlSizeIfPresent,
               CurrentIoctlBufferSize + IoctlSizeIfPresent);
    return true;
  }

  if (!g_Backend->VirtWriteStructDirty(InputBufferLengthPtr,
                                       &IoctlBufferSize)) {
    Print("VirtWriteDirty (InputBufferLength) failed\n");
    return false;
  }

   if constexpr (MutateIoctl) {

    const uint32_t Ioctl = *(uint32_t *)Buffer;
    const Gva_t IoctlPtr = Stack + Gva_t(0x30);

    if (!g_Backend->VirtWriteStructDirty(IoctlPtr, &Ioctl)) {
      Print("VirtWriteDirty (Ioctl) failed\n");
      return false;
    }
  }

  const Gva_t InputBufferPtr = g_Backend->VirtReadGva(Stack + Gva_t(0x38));
  if (!g_Backend->VirtWriteDirty(InputBufferPtr, IoctlBuffer, IoctlBufferSize)) {
    Print("VirtWriteDirty (InputBuffer) failed\n");
    return false;
  }

  return true;
}

bool Init(const Options_t &Opts, const CpuState_t &) {

  //
  // Stop the test-case once we return back from the call [NtDeviceIoControlFile]
  //

  const Gva_t Rip = Gva_t(g_Backend->Rip());
  const Gva_t AfterCall = Rip + Gva_t(0x14);
  if (!g_Backend->SetBreakpoint(AfterCall, [](Backend_t *Backend) {
        DebugPrint("Back from kernel!\n");
        Backend->Stop(Ok_t());
      })) {
    Print("Failed to SetBreakpoint AfterCall\n");
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
    Print("Failed to SetBreakpoint DbgPrintEx\n");
    return false;
  }

  //
  // Make ExGenRandom deterministic.
  //
  // kd> ub fffff805`3b8287c4 l1
  // nt!ExGenRandom+0xe0:
  // fffff805`3b8287c0 480fc7f2        rdrand  rdx
  const Gva_t ExGenRandom = Gva_t(g_Dbg.GetSymbol("nt!ExGenRandom") + 0xf3);
  if (g_Backend->VirtRead4(ExGenRandom) != 0xb8f2c70f) {
    Print("It seems that nt!ExGenRandom's code has changed, update the "
          "offset!\n");
    return false;
  }

  if (!g_Backend->SetBreakpoint(ExGenRandom, [](Backend_t *Backend) {
        DebugPrint("Hit ExGenRandom!\n");
        Backend->Rdx(Backend->Rdrand());
      })) {
    Print("Failed to SetBreakpoint ExGenRandom\n");
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
    Print("Failed to SetBreakpoint KeBugCheck2\n");
    return false;
  }

  //
  // Catch context-switches.
  //

  if (!g_Backend->SetBreakpoint("nt!SwapContext", [](Backend_t *Backend) {
        DebugPrint("nt!SwapContext\n");
        Backend->Stop(Cr3Change_t());
      })) {
    Print("Failed to SetBreakpoint SwapContext\n");
    return false;
  }


  return true;
}

//
// Register the target.
//

Target_t Ioctl("ioctl", Init, InsertTestcase);

} // namespace Ioctl