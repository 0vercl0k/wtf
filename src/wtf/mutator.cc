// Axel '0vercl0k' Souchet - April 19 2020
#include "mutator.h"

namespace fuzzer {
ExternalFunctions *EF = new ExternalFunctions();
}

std::unique_ptr<Mutator_t> LibfuzzerMutator_t::Create(std::mt19937_64 &Rng) {
  return std::make_unique<LibfuzzerMutator_t>(Rng);
}

LibfuzzerMutator_t::LibfuzzerMutator_t(std::mt19937_64 &Rng)
    : Rand_(Rng()), Mut_(Rand_, fuzzer::FuzzingOptions()) {
  ScratchBuffer__ = std::make_unique<uint8_t[]>(_1MB);
  ScratchBuffer_ = {ScratchBuffer__.get(), _1MB};
}

std::string LibfuzzerMutator_t::GetNewTestcase(const Corpus_t &Corpus) {
  //
  // If we get here, it means that we are ready to mutate.
  // First thing we do is to grab a seed.
  //

  const Testcase_t *Testcase = Corpus.PickTestcase();
  if (!Testcase) {
    fmt::print("The corpus is empty, exiting\n");
    std::abort();
  }

#if 0
  //
  // If the testcase is too big, abort as this should not happen.
  //

  if (Testcase->BufferSize_ > Opts_.TestcaseBufferMaxSize) {
    fmt::print("The testcase buffer len is bigger than the testcase buffer max "
               "size.\n");
    std::abort();
  }
#endif

  //
  // Copy the input in a buffer we're going to mutate.
  //

  memcpy(ScratchBuffer_.data(), Testcase->Buffer_.get(), Testcase->BufferSize_);
  const size_t NewTestcaseSize =
      Mut_.Mutate(ScratchBuffer_.data(), Testcase->BufferSize_,
                  ScratchBuffer_.size_bytes());

  std::string NewTestcase;
  NewTestcase.resize(NewTestcaseSize);
  memcpy(NewTestcase.data(), ScratchBuffer_.data(), NewTestcaseSize);
  return NewTestcase;
}

void LibfuzzerMutator_t::RegisterCustomMutator(const CustomMutatorFunc_t F) {
  fuzzer::EF->LLVMFuzzerCustomMutator = F;
}

void LibfuzzerMutator_t::OnNewCoverage(const Testcase_t &Testcase) {
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
  ScratchBuffer__ = std::make_unique<uint8_t[]>(_1MB);
  ScratchBuffer_ = {ScratchBuffer__.get(), _1MB};
}

std::string HonggfuzzMutator_t::GetNewTestcase(const Corpus_t &Corpus) {
  //
  // If we get here, it means that we are ready to mutate.
  // First thing we do is to grab a seed.
  //

  const Testcase_t *Testcase = Corpus.PickTestcase();
  if (!Testcase) {
    fmt::print("The corpus is empty, exiting\n");
    std::abort();
  }

#if 0
  //
  // If the testcase is too big, abort as this should not happen.
  //

  if (Testcase->BufferSize_ > Opts_.TestcaseBufferMaxSize) {
    fmt::print("The testcase buffer len is bigger than the testcase buffer max "
               "size.\n");
    std::abort();
  }
#endif

  //
  // Copy the input in a buffer we're going to mutate.
  //

  memcpy(ScratchBuffer_.data(), Testcase->Buffer_.get(), Testcase->BufferSize_);
  const size_t NewTestcaseSize =
      Mutate(ScratchBuffer_.data(), Testcase->BufferSize_,
             ScratchBuffer_.size_bytes());

  std::string NewTestcase;
  NewTestcase.resize(NewTestcaseSize);
  memcpy(NewTestcase.data(), ScratchBuffer_.data(), NewTestcaseSize);
  return NewTestcase;
}

size_t HonggfuzzMutator_t::Mutate(uint8_t *Data, const size_t DataLen,
                                  const size_t MaxSize) {
  Global_.mutate.maxInputSz = MaxSize;
  DynFile_.data = Data;
  DynFile_.size = DataLen;

  honggfuzz::mangle_mangleContent(&Run_, Global_.mutate.mutationsPerRun);
  return DynFile_.size;
}

void HonggfuzzMutator_t::OnNewCoverage(const Testcase_t &Testcase) {
  Run_.RandomBuffer = std::make_unique<uint8_t[]>(Testcase.BufferSize_);
  Run_.RandomBufferSize = Testcase.BufferSize_;
  memcpy(Run_.RandomBuffer.get(), Testcase.Buffer_.get(),
         Run_.RandomBufferSize);
}
