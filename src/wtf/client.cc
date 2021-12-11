// Axel '0vercl0k' Souchet - November 19 2020
#include "client.h"

namespace chrono = std::chrono;

class ClientStats_t {
  static const uint64_t RefreshRate = 10;
  uint64_t Coverage_ = 0;
  uint64_t TestcasesNumber_ = 0;

  uint64_t Crashes_ = 0;
  uint64_t Cr3s_ = 0;
  uint64_t Timeouts_ = 0;

  chrono::system_clock::time_point Start_ = chrono::system_clock::now();
  chrono::system_clock::time_point LastPrint_ = chrono::system_clock::now();
  chrono::system_clock::time_point LastCov_ = chrono::system_clock::now();

public:
  void Print(const bool ForcePrint = false) {
    //
    // Let's check if we should print the stats.
    //

    const uint64_t TimeSinceLastPrint = SecondsSince(LastPrint_).count();
    const bool Refresh =
        (TimeSinceLastPrint >= ClientStats_t::RefreshRate) || ForcePrint;

    if (!Refresh) {
      return;
    }

    //
    // Compute the amount of time since the last time we got new coverage.
    //

    const auto &[LastCov, LastCovUnit] = SecondsToHuman(SecondsSince(LastCov_));

    //
    // Compute the amount of time since the server started.
    //

    const auto &[Uptime, UptimeUnit] = SecondsToHuman(SecondsSince(Start_));

    //
    // Compute the amount of testcases executed per second.
    //

    const auto &[ExecsPerSecond, ExecsPerSecondUnit] = NumberToHuman(
        double(TestcasesNumber_) / double(SecondsSince(Start_).count()));

    fmt::print("#{} cov: {} exec/s: {:.1f}{} lastcov: {:.1f}{} crash: {} "
               "timeout: {} cr3: {} uptime: {:.1f}{}\n",
               TestcasesNumber_, Coverage_, ExecsPerSecond, ExecsPerSecondUnit,
               LastCov, LastCovUnit, Crashes_, Timeouts_, Cr3s_, Uptime,
               UptimeUnit);

    LastPrint_ = chrono::system_clock::now();
  }

  void RestoreStarts() {}
  void RestoreEnds() {}

  void TestcaseStarts() {}
  void TestcaseEnds(const TestcaseResult_t &TestcaseResult,
                    const uint64_t NewCoverage) {

    if (NewCoverage > 0) {
      LastCov_ = chrono::system_clock::now();
      Coverage_ += NewCoverage;
    }

    TestcasesNumber_++;

    if (std::holds_alternative<Ok_t>(TestcaseResult)) {
    } else if (std::holds_alternative<Cr3Change_t>(TestcaseResult)) {
      Cr3s_++;
    } else if (std::holds_alternative<Crash_t>(TestcaseResult)) {
      Crashes_++;
    } else if (std::holds_alternative<Timedout_t>(TestcaseResult)) {
      Timeouts_++;
    }
  }
};

ClientStats_t g_Stats;

TestcaseResult_t RunTestcaseAndRestore(const Target_t &Target,
                                       const CpuState_t &CpuState,
                                       const std::span<uint8_t> Buffer,
                                       const bool PrintRunStats) {
  //
  // Let the stats know that we are about to start to execute a testcase.
  //

  g_Stats.TestcaseStarts();

  //
  // Invoke the user callback so that it can insert the testcase.
  //

  if (!Target.InsertTestcase(Buffer.data(), Buffer.size_bytes())) {
    fmt::print("Failed to insert testcase\n");
    std::abort();
  }

  //
  // Run the testcase.
  //

  const auto &Res = g_Backend->Run(Buffer.data(), Buffer.size_bytes());
  if (!Res) {
    fmt::print("Failed to run the testcase\n");
    std::abort();
  }

  //
  // Let know the stats that we finished a testcase. Make sure to not count
  // coverage if the testcase timed-out as it'll get revoked.
  //

  const bool Timedout = std::holds_alternative<Timedout_t>(*Res);
  const uint64_t NewCoverage =
      Timedout ? 0 : g_Backend->LastNewCoverage().size();
  g_Stats.TestcaseEnds(*Res, NewCoverage);

  //
  // Let the stats that we are about to start a restore.
  //

  g_Stats.RestoreStarts();

  //
  // Invoke the user callback to give it a chance to restore things.
  //

  if (!Target.Restore()) {
    fmt::print("Failed to restore\n");
    std::abort();
  }

  //
  // Restore the execution environment.
  //

  if (!g_Backend->Restore(CpuState)) {
    fmt::print("Failed to restore the backend\n");
    std::abort();
  }

  //
  // Let the stats that we finished restoring.
  //

  g_Stats.RestoreEnds();

  //
  // Print the run stats after restoring because some backend only know about
  // dirty pages at restore time.
  //

  if (PrintRunStats) {
    g_Backend->PrintRunStats();
  }

  //
  // Print the global stats.
  //

  g_Stats.Print(PrintRunStats);
  return *Res;
}

Client_t::Client_t(const Options_t &Opts) : Opts_(Opts) {
  Scratch_ = std::make_unique<uint8_t[]>(_1MB);
  ScratchBuffer_ = {Scratch_.get(), _1MB};
}

bool Client_t::SendResult(const SocketFd_t &Fd, const std::string &Testcase,
                          const tsl::robin_set<Gva_t> &Coverage,
                          const TestcaseResult_t &TestcaseResult) {
  yas::mem_ostream Os;
  yas::binary_oarchive<decltype(Os), YasFlags> Oa(Os);
  Oa &Testcase &Coverage &TestcaseResult;
  const auto &Buf = Os.get_intrusive_buffer();
  if (!Send(Fd, (uint8_t *)Buf.data, Buf.size)) {
    fmt::print("Send failed\n");
    return false;
  }

  return true;
}

std::string Client_t::DeserializeTestcase(const std::span<uint8_t> Buffer) {
  yas::mem_istream Is(Buffer.data(), Buffer.size_bytes());
  yas::binary_iarchive<decltype(Is), YasFlags> Ia(Is);
  std::string Testcase;
  Ia &Testcase;
  return Testcase;
}

int Client_t::Run(const Target_t &Target, const CpuState_t &CpuState) {
  if (!Target.Init(Opts_, CpuState)) {
    fmt::print("Failed to initialize the target\n");
    return EXIT_FAILURE;
  }

  fmt::print("Dialing to {}..\n", Opts_.Fuzz.Address);
  auto ClientOpt = Dial(Opts_.Fuzz.Address);
  if (!ClientOpt) {
    fmt::print("Dial failed\n");
    return EXIT_FAILURE;
  }

  Client_ = *ClientOpt;

  while (1) {
    const auto ReceivedSize =
        Receive(Client_, ScratchBuffer_.data(), ScratchBuffer_.size_bytes());
    if (!ReceivedSize) {
      fmt::print("Receive failed\n");
      break;
    }

    //
    // Deserialize the testcase.
    //

    const std::string Testcase =
        DeserializeTestcase({ScratchBuffer_.data(), *ReceivedSize});

    //
    // Run the testcase.
    //

    const TestcaseResult_t TestcaseResult = RunTestcaseAndRestore(
        Target, CpuState, {(uint8_t *)Testcase.data(), Testcase.size()});

    //
    // If we triggered a time out testcase, we ask the backend to invalidate
    // the new coverage it has triggered. On top of that, we lie to the
    // server by telling it that we haven't hit new coverage.
    //

    if (std::holds_alternative<Timedout_t>(TestcaseResult)) {
      g_Backend->RevokeLastNewCoverage();
    }

    //
    // Send the result back to the server.
    //

    if (!SendResult(Client_, Testcase, g_Backend->LastNewCoverage(),
                    TestcaseResult)) {
      fmt::print("SendResult failed\n");
      break;
    }

    Received_++;
  }

  g_Stats.Print(true);
  CloseSocket(Client_);
  return EXIT_SUCCESS;
}