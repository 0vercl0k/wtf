// Axel '0vercl0k' Souchet - April 19 2020
#include "mutator.h"

namespace fuzzer {
ExternalFunctions *EF = new ExternalFunctions();
}

std::unique_ptr<Mutator_t> LibfuzzerMutator_t::Create(std::mt19937_64 &Rng) {
  return std::make_unique<LibfuzzerMutator_t>(Rng);
}

LibfuzzerMutator_t::LibfuzzerMutator_t(std::mt19937_64 &Rng)
    : Rand_(Rng()), Mut_(Rand_, fuzzer::FuzzingOptions()) {}

size_t LibfuzzerMutator_t::Mutate(uint8_t *Data, const size_t DataLen,
                                  const size_t MaxSize) {
  return Mut_.Mutate(Data, DataLen, MaxSize);
}

void LibfuzzerMutator_t::RegisterCustomMutator(const CustomMutatorFunc_t F) {
  fuzzer::EF->LLVMFuzzerCustomMutator = F;
}

void LibfuzzerMutator_t::SetCrossOverWith(const Testcase_t &Testcase) {
  CrossOverWith_ = std::make_unique<fuzzer::Unit>(
      Testcase.Buffer_.get(), Testcase.Buffer_.get() + Testcase.BufferSize_);
  Mut_.SetCrossOverWith(CrossOverWith_.get());
}

std::unique_ptr<Mutator_t> HonggfuzzMutator_t::Create(std::mt19937_64 &Rng) {
  return std::make_unique<HonggfuzzMutator_t>(Rng);
}

HonggfuzzMutator_t::HonggfuzzMutator_t(std::mt19937_64 &Rng)
    : Rng_(Rng), Run_(Rng_) {
  Run_.dynfile = &DynFile_;
  Run_.global = &Global_;
  Run_.mutationsPerRun = 5;
  Global_.mutate.mutationsPerRun = Run_.mutationsPerRun;
  Global_.timing.lastCovUpdate = time(nullptr);
}

size_t HonggfuzzMutator_t::Mutate(uint8_t *Data, const size_t DataLen,
                                  const size_t MaxSize) {
  Global_.mutate.maxInputSz = MaxSize;
  DynFile_.data = Data;
  DynFile_.size = DataLen;

  honggfuzz::mangle_mangleContent(&Run_, Global_.mutate.mutationsPerRun);
  return DynFile_.size;
}

void HonggfuzzMutator_t::SetCrossOverWith(const Testcase_t &Testcase) {
  Run_.RandomBuffer = std::make_unique<uint8_t[]>(Testcase.BufferSize_);
  Run_.RandomBufferSize = Testcase.BufferSize_;
  memcpy(Run_.RandomBuffer.get(), Testcase.Buffer_.get(),
         Run_.RandomBufferSize);
}
