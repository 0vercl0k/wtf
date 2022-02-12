// Axel '0vercl0k' Souchet - March 8 2020
#pragma once
#include "FuzzerExtFunctions.h"
#include "FuzzerMutate.h"
#include "corpus.h"
#include "honggfuzz.h"
#include <memory>
#include <string>

class Mutator_t {
public:
  Mutator_t() = default;
  virtual ~Mutator_t() = default;
  virtual std::string GetNewTestcase(const Corpus_t &Corpus) {
    fmt::print("You need to override Mutator_t::Mutate!\n");
    std::abort();
  }

  virtual void OnNewCoverage(const Testcase_t &Testcase) {}
};

class LibfuzzerMutator_t : public Mutator_t {
  using CustomMutatorFunc_t =
      decltype(fuzzer::ExternalFunctions::LLVMFuzzerCustomMutator);
  std::unique_ptr<uint8_t[]> ScratchBuffer__;
  span_u8 ScratchBuffer_;
  fuzzer::Random Rand_;
  fuzzer::MutationDispatcher Mut_;
  std::unique_ptr<fuzzer::Unit> CrossOverWith_;
  size_t TestcaseMaxSize_ = 0;

  public:
  static std::unique_ptr<Mutator_t> Create(std::mt19937_64 &Rng,
                                           const size_t TestcaseMaxSize);
  explicit LibfuzzerMutator_t(std::mt19937_64 &Rng,
                              const size_t TestcaseMaxSize);
  std::string GetNewTestcase(const Corpus_t &Corpus) override;
  void OnNewCoverage(const Testcase_t &Testcase) override;

private:
  void RegisterCustomMutator(const CustomMutatorFunc_t F);
  void SetCrossOverWith(const Testcase_t &Testcase);
};

class HonggfuzzMutator_t : public Mutator_t {
  std::unique_ptr<uint8_t[]> ScratchBuffer__;
  span_u8 ScratchBuffer_;
  honggfuzz::dynfile_t DynFile_;
  honggfuzz::honggfuzz_t Global_;
  std::mt19937_64 &Rng_;
  honggfuzz::run_t Run_;
  size_t TestcaseMaxSize_ = 0;

public:
  static std::unique_ptr<Mutator_t> Create(std::mt19937_64 &Rng,
                                           const size_t TestcaseMaxSize);
  explicit HonggfuzzMutator_t(std::mt19937_64 &Rng,
                              const size_t TestcaseMaxSize);
  std::string GetNewTestcase(const Corpus_t &Corpus) override;
  void OnNewCoverage(const Testcase_t &Testcase) override;

private:
  size_t Mutate(uint8_t *Data, const size_t DataLen, const size_t MaxSize);
  void SetCrossOverWith(const Testcase_t &Testcase);
};