// 1ndahous3 - March 4 2023
#include "backend.h"
#include "targets.h"
#include <fmt/format.h>

namespace Ioctl {

constexpr bool DebugLoggingOn = false;
constexpr bool MutateIoctl = true;

#define Print(Fmt, ...) fmt::print("ioctl: " Fmt, __VA_ARGS__)

template <typename... Args_t>
void DebugPrint(const char *Format, const Args_t &...args) {
  if constexpr (DebugLoggingOn) {
    fmt::print("ioctl: ");
    fmt::print(fmt::runtime(Format), args...);
  }
}

std::unique_ptr<uint8_t[]> g_LastBuffer;
size_t g_LastBufferSize = 0;

bool InsertTestcase(const uint8_t *Buffer, const size_t BufferSize) {

  //
  // If we have a testcase still alive, it means we haven't hit the return
  // breakpoint which is suspicious.
  //

  static bool FirstTime = true;
  if (!FirstTime) {
    if (!g_LastBuffer || g_LastBufferSize == 0) {
      Print("The testcase hasn't been reset; something is wrong\n");
      std::abort();
    }
  } else {
    FirstTime = false;
  }

  //
  // If we are mutating the IoControlCode, we expect at least 4 bytes.
  //

  if constexpr (MutateIoctl) {
    if (BufferSize < sizeof(uint32_t)) {
      Print("The testcase buffer is too small: {} bytes\n", BufferSize);
      return false;
    }
  }

  //
  // Copy the testcase; we'll inject it later if we hit NtDeviceIoControlFile.
  //

  g_LastBufferSize = BufferSize;
  g_LastBuffer = std::make_unique<uint8_t[]>(BufferSize);
  std::memcpy(g_LastBuffer.get(), Buffer, BufferSize);
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
              const auto ReturnAddress =
                  Backend->VirtReadGva(Gva_t(Backend->Rsp()));
              if (!Backend->SetBreakpoint(
                      ReturnAddress, [](Backend_t *Backend) {
                        //
                        // Ok we're done!
                        //

                        DebugPrint("Hit return breakpoint!\n");
                        Backend->Stop(Ok_t());
                        g_LastBuffer = nullptr;
                        g_LastBufferSize = 0;
                      })) {
                Print("Failed to set breakpoint on return\n");
                std::abort();
              }

              SetExitBreakpoint = true;
            }

            //
            // If we don't have a testcase, then things are in a broken state,
            // so abort.
            //

            if (!g_LastBuffer || g_LastBufferSize == 0) {
              Print("Hit NtDeviceIoControlFile w/o a testcase..?\n");
              std::abort();
            }

            //
            // If we're mutating the IoControlCode, then the first 4 bytes are
            // that.
            //

            constexpr uint32_t IoctlSizeIfPresent =
                MutateIoctl ? sizeof(uint32_t) : 0;

            //
            // We can only insert testcases that are smaller or equal to the
            // current size; otherwise we'll corrupt memory. To work around
            // this, we truncate it if it's larger.
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

            const uint32_t TotalInputBufferSize =
                g_LastBufferSize - IoctlSizeIfPresent;
            const auto MutatedIoControlCodePtr = (uint32_t *)g_LastBuffer.get();
            const uint8_t *MutatedInputBufferPtr =
                g_LastBuffer.get() + IoctlSizeIfPresent;
            Gva_t InputBufferSizePtr;
            const auto InputBufferSize =
                uint32_t(Backend->GetArg(7, InputBufferSizePtr));
            const uint32_t MutatedInputBufferSize =
                std::min(TotalInputBufferSize, InputBufferSize);

            //
            // Fix up InputBufferLength.
            //

            if (!Backend->VirtWriteStructDirty(InputBufferSizePtr,
                                               &MutatedInputBufferSize)) {
              Print("Failed to fix up the InputBufferSize\n");
              std::abort();
            }

            //
            // Insert the testcase in the InputBuffer.
            // XXX: Ideally, we'd push up the testcase to the
            // boundary of the buffer to have a better chance to
            // catch OOBs and update the InputBuffer pointer.
            //

            if (!Backend->VirtWriteDirty(Backend->GetArgGva(8),
                                         MutatedInputBufferPtr,
                                         MutatedInputBufferSize)) {
              Print("Failed to insert the testcase\n");
              std::abort();
            }

            //
            // Are we mutating IoControlCode as well?
            //

            if constexpr (MutateIoctl) {
              const auto MutatedIoControlCode = *MutatedIoControlCodePtr;
              Gva_t IoControlCodePtr;
              Backend->GetArgGva(5, IoControlCodePtr);
              if (!Backend->VirtWriteStructDirty(IoControlCodePtr,
                                                 &MutatedIoControlCode)) {
                Print("Failed to VirtWriteStructDirty (Ioctl) failed\n");
                std::abort();
              }
            }
          })) {
    Print("Failed to SetBreakpoint NtDeviceIoControlFile\n");
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
  const Gva_t ExGenRandom = Gva_t(g_Dbg.GetSymbol("nt!ExGenRandom") + 0xe0 + 4);
  if (g_Backend->VirtRead4(ExGenRandom - Gva_t(4)) != 0xf2c70f48) {
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