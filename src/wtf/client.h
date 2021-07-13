// Axel '0vercl0k' Souchet - October 14 2020
#pragma once
#include "backend.h"
#include "globals.h"
#include "platform.h"
#include "socket.h"
#include "targets.h"
#include "utils.h"
#include "yas/serialize.hpp"
#include "yas/std_types.hpp"
#include <fmt/format.h>
#include <span>
#include <string>
#include <thread>
#include <unordered_set>
#include <variant>

namespace chrono = std::chrono;

[[nodiscard]] TestcaseResult_t
RunTestcaseAndRestore(const Target_t &Target, const CpuState_t &CpuState,
                      const std::span<uint8_t> Buffer,
                      const bool PrintRunStats = false);

class Client_t {
  SocketFd_t Client_ = INVALID_SOCKET;
  const Options_t &Opts_;
  chrono::high_resolution_clock::time_point Start_ =
      chrono::high_resolution_clock::now();
  chrono::high_resolution_clock::time_point LastPrint_ =
      chrono::high_resolution_clock::now();
  uint64_t Received_ = 0;
  std::unique_ptr<uint8_t[]> Scratch_;
  std::span<uint8_t> ScratchBuffer_;

public:
  Client_t(const Options_t &Opts);

  bool SendResult(const SocketFd_t &Fd, const std::string &Testcase,
                  const tsl::robin_set<Gva_t> &Coverage,
                  const TestcaseResult_t &TestcaseResult);

  std::string DeserializeTestcase(const std::span<uint8_t> Buffer);

  int Run(const Target_t &Target, const CpuState_t &CpuState);
};
