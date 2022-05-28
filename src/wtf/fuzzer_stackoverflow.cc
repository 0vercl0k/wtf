// Axel '0vercl0k' Souchet - July 31 2021
#include "backend.h"
#include "targets.h"
#include <fmt/format.h>

namespace fs = std::filesystem;

namespace Stackoverflow {

bool LoggingOn = true;

template <typename... Args_t>
void DebugPrint(const char *Format, const Args_t &...args) {
    fmt::print(Format, args...);
}

template <typename... Args_t>
void DebugPrintOnce(const char *Format, const Args_t &...args) {
  if(LoggingOn){
    fmt::print(Format, args...);
    LoggingOn = false;
  }
}

bool InsertTestcase(const uint8_t *Buffer, const size_t BufferSize) {
  
  const Gva_t Rdi = Gva_t(g_Backend->GetReg(Registers_t::Rdi));

  if (!g_Backend->VirtWrite(Rdi, Buffer, BufferSize, true)) {
    DebugPrint("Failed to write next testcase!");
    return false;
  }

  g_Backend->SetReg(Registers_t::Rsi, BufferSize);

  return true;
}

bool Init(const Options_t &Opts, const CpuState_t &) {

  if (!g_Backend->SetBreakpoint("stop", [](Backend_t *Backend) {
        Backend->Stop(Ok_t());
      })) {
    DebugPrint("Failed to SetBreakpoint stop\n");
    return false;
  }

  if (!g_Backend->SetBreakpoint("stack_chk_failed", [](Backend_t *Backend) {
        Backend->Stop(Crash_t("crash"));

      })) {
    DebugPrint("Failed to SetBreakpoint stack_chk_failed\n");
    return false;
  }
  
  return true;
}

bool Restore() { return true; }

//
// Register the target.
//

Target_t Stackoverflow("stackoverflow", Init, InsertTestcase, Restore);

} // namespace Stackoverflow