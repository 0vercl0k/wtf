// Axel '0vercl0k' Souchet - August 23 2020
#include "targets.h"
#include <fmt/format.h>

//
// Everytime a target is constructed, it gets added into the |g_Targets|
// instance. This allows each fuzzer target to instantiate a target object in
// the global scope and have it register itself in |g_Targets|.
//

Target_t::Target_t(const std::string &_Name, const Init_t _Init,
                   const InsertTestcase_t _InsertTestcase,
                   const Restore_t _Restore,
                   const CreateMutator_t _CreateMutator)
    : Name(_Name), Init(_Init), InsertTestcase(_InsertTestcase),
      Restore(_Restore), CreateMutator(_CreateMutator) {
  Targets_t::Instance().Registers(*this);
}

void Targets_t::Registers(const Target_t &Target) {
  Targets.emplace_back(Target);
}

Target_t *Targets_t::Get(const std::string &Name) {
  for (auto &Target : Targets) {
    if (Name == Target.Name) {
      return &Target;
    }
  }
  return nullptr;
}

void Targets_t::DisplayRegisteredTargets() {
  fmt::print("Existing targets:\n");
  for (const auto &Target : Targets) {
    fmt::print("  - Name: {}\n", Target.Name);
  }
}
