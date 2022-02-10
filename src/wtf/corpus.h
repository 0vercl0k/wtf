// Axel '0vercl0k' Souchet - April 4 2020
#pragma once
#include "globals.h"
#include "platform.h"
#include "utils.h"
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fmt/format.h>
#include <random>
#include <vector>

//
// A testcase is basically a buffer and a size.
//

struct Testcase_t {
  std::unique_ptr<uint8_t[]> Buffer_;
  size_t BufferSize_;

  explicit Testcase_t(const uint8_t *Buffer, const size_t BufferSize) {
    Buffer_ = std::make_unique<uint8_t[]>(BufferSize);
    BufferSize_ = BufferSize;
    memcpy(Buffer_.get(), Buffer, BufferSize);
  }

  explicit Testcase_t(std::unique_ptr<uint8_t[]> Buffer,
                      const size_t BufferSize)
      : Buffer_(std::move(Buffer)), BufferSize_(BufferSize) {}

  Testcase_t(Testcase_t &&Testcase) noexcept
      : Buffer_(std::move(Testcase.Buffer_)),
        BufferSize_(std::exchange(Testcase.BufferSize_, 0)) {}

  Testcase_t(const Testcase_t &) = delete;
  Testcase_t &operator=(Testcase_t &) = delete;
  Testcase_t &operator=(Testcase_t &&Testcase) = delete;
};

class Corpus_t {
  std::vector<Testcase_t> Testcases_;
  const std::filesystem::path OutputsPath_;
  uint64_t Bytes_ = 0;
  std::mt19937_64 &Rng_;

public:
  explicit Corpus_t(const std::filesystem::path &OutputsPath,
                    std::mt19937_64 &Rng)
      : OutputsPath_(OutputsPath), Rng_(Rng) {}

  Corpus_t(const Corpus_t &) = delete;
  Corpus_t &operator=(const Corpus_t &) = delete;

  [[nodiscard]] size_t Size() { return Testcases_.size(); }

  bool SaveTestcase(const TestcaseResult_t &TestcaseResult,
                    Testcase_t Testcase) {
    const std::string TestcaseHash =
        Blake3HexDigest(Testcase.Buffer_.get(), Testcase.BufferSize_);

    //
    // Hash the testcase and use it as a name.
    //

    std::string Prefix;
    if (!std::holds_alternative<Ok_t>(TestcaseResult)) {
      Prefix = fmt::format("{}-", TestcaseResultToString(TestcaseResult));
    }

    const std::string TestcaseName = fmt::format("{}{}", Prefix, TestcaseHash);
    const std::filesystem::path OutputFilePath(OutputsPath_ / TestcaseName);
    const bool ShouldSave =
        !OutputsPath_.empty() && !std::filesystem::exists(OutputFilePath);

    if (ShouldSave) {
      fmt::print("Saving output in {}\n", OutputFilePath.string());
      if (!SaveFile(OutputFilePath, Testcase.Buffer_.get(),
                    Testcase.BufferSize_)) {
        fmt::print("Could not create the destination file.\n");
        return false;
      }
    }

    Bytes_ += Testcase.BufferSize_;
    Testcases_.emplace_back(std::move(Testcase));
    return true;
  }

  [[nodiscard]] const Testcase_t *PickTestcase() const {

    //
    // Try to grab a testcase for the user.
    //

    if (Testcases_.size() == 0) {
      return nullptr;
    }

    std::uniform_int_distribution<size_t> Dist(0, Testcases_.size() - 1);
    const size_t LuckyWinner = Dist(Rng_);
    return &Testcases_[LuckyWinner];
  }

  [[nodiscard]] uint64_t Bytes() const { return Bytes_; }

private:
  [[nodiscard]] std::string_view
  TestcaseResultToString(const TestcaseResult_t &Res) {
    return std::visit([](const auto &R) { return R.Name(); }, Res);
  }
};