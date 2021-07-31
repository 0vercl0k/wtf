// Axel '0vercl0k' Souchet - July 31 2021
#include "backend.h"
#include "targets.h"
#include <fmt/format.h>

namespace fs = std::filesystem;

namespace Dummy {

bool InsertTestcase(const uint8_t *Buffer, const size_t BufferSize) {
  return true;
}

bool Init(const Options_t &Opts, const CpuState_t &) { return true; }

bool Restore() { return true; }

//
// Register the target.
//

Target_t Dummy("dummy", Init, InsertTestcase, Restore);

} // namespace Dummy