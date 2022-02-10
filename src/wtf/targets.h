// Axel '0vercl0k' Souchet - August 23 2020
#pragma once
#include "corpus.h"
#include "globals.h"
#include "mutator.h"
#include <cstdint>
#include <vector>

//
// This describes a fuzzer target which is basically a name and a bunch of
// callbacks.
//

struct Target_t {
  using Init_t = bool (*)(const Options_t &, const CpuState_t &);
  using InsertTestcase_t = bool (*)(const uint8_t *, const size_t);
  using Restore_t = bool (*)();
  using CreateMutator_t = std::unique_ptr<Mutator_t> (*)(std::mt19937_64 &,
                                                         const size_t);

  explicit Target_t(
      const std::string &_Name, const Init_t _Init,
      const InsertTestcase_t _InsertTestcase,
      const Restore_t _Restore = []() { return true; },
      const CreateMutator_t _CreateMutator = LibfuzzerMutator_t::Create);

  std::string Name;
  Init_t Init = nullptr;
  InsertTestcase_t InsertTestcase = nullptr;
  Restore_t Restore = nullptr;
  CreateMutator_t CreateMutator = nullptr;
};

//
// This is where we store all the targets in.
//

struct Targets_t {
  std::vector<Target_t> Targets;

  Target_t *Get(const std::string &Name);
  void DisplayRegisteredTargets();
  void Registers(const Target_t &Target);
  static Targets_t &Instance() {
    static Targets_t Targets;
    return Targets;
  }
};
