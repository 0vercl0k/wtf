// 1ndahous3 - March 4 2023
#include "backend.h"
#include "targets.h"
#include <fmt/format.h>

//
// This fuzzing module expects a snapshot made at nt!NtDeviceIoControlFile.
// It is recommended to grab a snapshot with the biggest InputBufferLength
// possible.
//

namespace Ioctl {

constexpr bool DebugLoggingOn = false;
constexpr bool MutateIoctl = true;

template <typename... Args_t>
void DebugPrint(const char *Format, const Args_t &...args) {
  if constexpr (DebugLoggingOn) {
    fmt::print("Ioctl: ");
    fmt::print(fmt::runtime(Format), args...);
  }
}

bool InsertTestcase(const uint8_t *Buffer, const size_t BufferSize) {

  //
  // If we are mutating the IoControlCode, we expect at least 4 bytes.
  //

  if constexpr (MutateIoctl) {
    if (BufferSize < sizeof(uint32_t)) {
      return true;
    }
  }

  //
  // If we're mutating the IoControlCode, then the first 4 bytes are
  // that.
  //

  constexpr uint32_t IoctlSizeIfPresent = MutateIoctl ? sizeof(uint32_t) : 0;

  //
  // We can only insert testcases that are smaller or equal to the
  // current size; otherwise we'll corrupt memory. To work around
  // this, we truncate it if it's larger.
  // We also modify the InputBuffer pointer to push it as close as possible from
  // the end of the buffer.
  //

  //
  // __kernel_entry NTSTATUS
  // NtDeviceIoControlFile(
  //  [in]  HANDLE           FileHandle,
  //  [in]  HANDLE           Event,
  //  [in]  PIO_APC_ROUTINE  ApcRoutine,
  //  [in]  PVOID            ApcContext,
  //  [out] PIO_STATUS_BLOCK IoStatusBlock,
  //  [in]  ULONG            IoControlCode,
  //  [in]  PVOID            InputBuffer,
  //  [in]  ULONG            InputBufferLength,
  //  [out] PVOID            OutputBuffer,
  //  [in]  ULONG            OutputBufferLength
  // );
  //

  const uint32_t TotalInputBufferSize = BufferSize - IoctlSizeIfPresent;
  const auto MutatedIoControlCodePtr = (uint32_t *)Buffer;
  const uint8_t *MutatedInputBufferPtr = Buffer + IoctlSizeIfPresent;

  //
  // Calculate the maximum size we can inject into the target. Either we can
  // inject it all, or we need to truncate it.
  //

  const auto &[InputBufferSize, InputBufferSizePtr] =
      g_Backend->GetArgAndAddress(7);
  const uint32_t MutatedInputBufferSize =
      std::min(TotalInputBufferSize, uint32_t(InputBufferSize));

  //
  // Calculate the new InputBuffer address by pushing the mutated buffer as
  // close as possible from its end.
  //

  const auto &[InputBuffer, InputBufferPtr] = g_Backend->GetArgAndAddress(6);
  const auto NewInputBuffer =
      Gva_t(InputBuffer + InputBufferSize - MutatedInputBufferSize);

  //
  // Fix up InputBufferLength.
  //

  if (!g_Backend->VirtWriteStructDirty(InputBufferSizePtr,
                                       &MutatedInputBufferSize)) {
    fmt::print("Failed to fix up the InputBufferSize\n");
    std::abort();
  }

  //
  // Fix up InputBuffer.
  //

  if (!g_Backend->VirtWriteStructDirty(InputBufferPtr, &NewInputBuffer)) {
    fmt::print("Failed to fix up the InputBuffer\n");
    std::abort();
  }

  //
  // Insert the testcase at the new InputBuffer.
  //

  if (!g_Backend->VirtWriteDirty(NewInputBuffer, MutatedInputBufferPtr,
                                 MutatedInputBufferSize)) {
    fmt::print("Failed to insert the testcase\n");
    std::abort();
  }

  //
  // Are we mutating IoControlCode as well?
  //

  if constexpr (MutateIoctl) {
    const auto MutatedIoControlCode = *MutatedIoControlCodePtr;
    const auto &IoControlCodePtr = g_Backend->GetArgAddress(5);
    if (!g_Backend->VirtWriteStructDirty(IoControlCodePtr,
                                         &MutatedIoControlCode)) {
      fmt::print("Failed to VirtWriteStructDirty (Ioctl) failed\n");
      std::abort();
    }
  }

  return true;
}

bool Init(const Options_t &Opts, const CpuState_t &) {

  //
  // Break on nt!NtDeviceIoControlFile. This is at that moment that we'll insert
  // the testcase.
  //

  if (!g_Backend->SetBreakpoint(
          "nt!NtDeviceIoControlFile", [](Backend_t *Backend) {
            //
            // The first time we hit this breakpoint, we
            // grab the return address and we set a
            // breakpoint there to finish the testcase.
            //

            static bool SetExitBreakpoint = false;
            if (!SetExitBreakpoint) {
              SetExitBreakpoint = true;
              const auto ReturnAddress =
                  Backend->VirtReadGva(Gva_t(Backend->Rsp()));
              if (!Backend->SetBreakpoint(
                      ReturnAddress, [](Backend_t *Backend) {
                        //
                        // Ok we're done!
                        //

                        DebugPrint("Hit return breakpoint!\n");
                        Backend->Stop(Ok_t());
                      })) {
                fmt::print("Failed to set breakpoint on return\n");
                std::abort();
              }
            }
          })) {
    fmt::print("Failed to SetBreakpoint NtDeviceIoControlFile\n");
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
    fmt::print("Failed to SetBreakpoint DbgPrintEx\n");
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
    fmt::print("Failed to SetBreakpoint ExGenRandom\n");
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
    fmt::print("Failed to SetBreakpoint KeBugCheck2\n");
    return false;
  }

  //
  // Catch context-switches.
  //

  if (!g_Backend->SetBreakpoint("nt!SwapContext", [](Backend_t *Backend) {
        DebugPrint("nt!SwapContext\n");
        Backend->Stop(Cr3Change_t());
      })) {
    fmt::print("Failed to SetBreakpoint SwapContext\n");
    return false;
  }

  return true;
}

//
// Register the target.
//

Target_t Ioctl("ioctl", Init, InsertTestcase);

} // namespace Ioctl