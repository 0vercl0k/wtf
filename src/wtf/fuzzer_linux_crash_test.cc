#include "backend.h"

namespace linux_crash_test {


bool InsertTestcase(const uint8_t *Buffer, const size_t BufferSize) {

    if (BufferSize > 10) {
        return true;
    }

    if( !g_Backend->VirtWriteDirty(Gva_t(g_Backend->Rdi()), Buffer, BufferSize) ) {
        fmt::print("Failed to write payload.\n");
        return false;
    }

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
