// Jason Crowder - February 2024
#include "backend.h"

namespace linux_crash_test {
Crash_t GetCrashTestcaseName(const char *Prefix, Backend_t *Backend) {
  return Crash_t(fmt::format("crash-{}-{:#x}", Prefix, Backend->Cr2()));
}

bool InsertTestcase(const uint8_t *Buffer, const size_t BufferSize) {
  if (BufferSize > 10) {
    return true;
  }

  if (!g_Backend->VirtWriteDirty(Gva_t(g_Backend->Rdi()), Buffer, BufferSize)) {
    fmt::print("Failed to write payload.\n");
    return false;
  }

  return true;
}

bool Init(const Options_t &Opts, const CpuState_t &) {

  if (!g_Backend->SetBreakpoint("asm_exc_page_fault", [](Backend_t *Backend) {
        Backend->Stop(GetCrashTestcaseName("asm_exc_page_fault", Backend));
      })) {
    fmt::print("Failed to insert crash breakpoint.\n");
    return false;
  }

  if (!g_Backend->SetBreakpoint("asm_exc_divide_error", [](Backend_t *Backend) {
        Backend->Stop(GetCrashTestcaseName("asm_exc_divide_error", Backend));
      })) {
    fmt::print("Failed to insert crash breakpoint.\n");
    return false;
  }

  if (!g_Backend->SetBreakpoint("force_sigsegv", [](Backend_t *Backend) {
        Backend->Stop(GetCrashTestcaseName("force_sigsegv", Backend));
      })) {
    fmt::print("Failed to insert crash breakpoint.\n");
    return false;
  }

  if (!g_Backend->SetBreakpoint("page_fault_oops", [](Backend_t *Backend) {
        Backend->Stop(GetCrashTestcaseName("page_fault_oops", Backend));
      })) {
    fmt::print("Failed to insert crash breakpoint.\n");
    return false;
  }

  if (!g_Backend->SetBreakpoint("end_crash_test", [](Backend_t *Backend) {
        Backend->Stop(Ok_t());
      })) {
    return false;
  }

  return true;
}

//
// Register the target.
//

Target_t linux_crash_test("linux_crash_test", Init, InsertTestcase);

} // namespace linux_crash_test
