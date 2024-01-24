#include "backend.h"

namespace linux_page_fault_test {

  constexpr bool LoggingOn = false;

  template <typename... Args_t>
  void DebugPrint(const char *Format, const Args_t &...args) {
    if constexpr (LoggingOn) {
      fmt::print("linux_page_fault_test: ");
      fmt::print(fmt::runtime(Format), args...);
    }
  }


bool InsertTestcase(const uint8_t *Buffer, const size_t BufferSize) {
    return true;
}

bool Init(const Options_t &Opts, const CpuState_t &) {
    if (!g_Backend->SetCrashBreakpoint("asm_exc_page_fault")) {
        fmt::print("Failed to insert crash breakpoint.\n");
        return false;
    }

    if (!g_Backend->SetCrashBreakpoint("asm_exc_divide_error")) {
        fmt::print("Failed to insert crash breakpoint.\n");
        return false;
    }

    if (!g_Backend->SetCrashBreakpoint("force_sigsegv")) {
        fmt::print("Failed to insert crash breakpoint.\n");
        return false;
    }

    if (!g_Backend->SetCrashBreakpoint("page_fault_oops")) {
        fmt::print("Failed to insert crash breakpoint.\n");
        return false;
    }

    if (!g_Backend->SetBreakpoint("done_with_test", [](Backend_t *Backend) {
            Backend->Stop(Ok_t());
        })) {
        fmt::print("Failed to insert breakpoint.\n");
        return false;
    }

    return true;
}

Target_t linux_page_fault_test("linux_page_fault_test", Init, InsertTestcase);

}
