// Axel '0vercl0k' Souchet - October 13 2020
#pragma once
#include "corpus.h"
#include "dirwatch.h"
#include "fmt/core.h"
#include "globals.h"
#include "mutator.h"
#include "socket.h"
#include "targets.h"
#include "tsl/robin_set.h"
#include "utils.h"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>
#include <span>
#include <unordered_set>

namespace chrono = std::chrono;

//
// Holds all the stats for the server.
//

class ServerStats_t {

  //
  // If we're asked to print stats and it hasn't been at least 10 seconds since
  // the last refresh we won't display them.
  //

  constexpr static uint64_t RefreshRate = 10;

  //
  // The number of files in the corpus.
  //

  uint64_t CorpusSize_ = 0;

  //
  // The aggregated size of the corpus in bytes.
  //

  uint64_t CorpusBytes_ = 0;

  //
  // The amount of coverage hit.
  //

  uint64_t Coverage_ = 0;

  //
  // The last coverage displayed on the output (used to compute the difference
  // since the previous time the status changed).
  //

  uint64_t LastCoverage_ = 0;

  //
  // The number of testcase the nodes have executed.
  //

  uint64_t TestcasesNumber_ = 0;

  //
  // The number of currently connected clients.
  //

  uint64_t Clients_ = 0;

  //
  // The number of testcases that crashed.
  //

  uint64_t Crashes_ = 0;

  //
  // The number of testcases that triggered a cr3 change.
  //

  uint64_t Cr3s_ = 0;

  //
  // The number of testcases that triggered a timeout.
  //

  uint64_t Timeouts_ = 0;

  //
  // This is a point in time when the server stats are created.
  //

  chrono::system_clock::time_point Start_ = chrono::system_clock::now();

  //
  // This is a point in time when the first client connects to the server.
  //

  chrono::system_clock::time_point FirstClientStart_;

  //
  // The first time a client connects.
  //

  bool FirstClient_ = true;

  //
  // This is the last time we printed the stats.
  //

  chrono::system_clock::time_point LastPrint_ = chrono::system_clock::now();

  //
  // This is the last time we received a new coverage.
  //

  chrono::system_clock::time_point LastCov_ = chrono::system_clock::now();

public:
  //
  // A new client just connected.
  //

  void NewClient() {
    Clients_++;

    //
    // The first time a client connects, grab a time point. This is useful to
    // calculate the speed. If we don't do that we could have a server that has
    // been sitting without any clients and as a result it would skew the e/s.
    //

    if (FirstClient_) {
      FirstClientStart_ = chrono::system_clock::now();
      FirstClient_ = false;
    }
  }

  //
  // A client dropped.
  //

  void DisconnectClient() { Clients_--; }

  //
  // Print the stats.
  //

  void Print(const bool ForcePrint = false, std::FILE *Log_ = nullptr) {

    //
    // Let's check if we should print the stats.
    //

    const uint64_t TimeSinceLastPrint = SecondsSince(LastPrint_).count();
    const bool Refresh =
        (TimeSinceLastPrint >= ServerStats_t::RefreshRate) || ForcePrint;

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

    const auto &[ExecsPerSecond, ExecsPerSecondUnit] =
        NumberToHuman(double(TestcasesNumber_) /
                      double(SecondsSince(FirstClientStart_).count()));

    //
    // Compute the size of the corpus.
    //

    const auto &[CorpusBytes, CorpusSizeUnit] = BytesToHuman(CorpusBytes_);

    //
    // Compute the coverage difference.
    //

    const uint64_t CovDiff = Coverage_ - LastCoverage_;

    std::string StatsStr = fmt::format(
        "#{} cov: {} (+{}) corp: {} ({:.1f}{}) exec/s: {:.1f}{} ({} "
        "nodes) lastcov: "
        "{:.1f}{} crash: {} "
        "timeout: {} cr3: {} uptime: {:.1f}{}\n",
        TestcasesNumber_, Coverage_, CovDiff, CorpusSize_, CorpusBytes,
        CorpusSizeUnit, ExecsPerSecond, ExecsPerSecondUnit, Clients_, LastCov,
        LastCovUnit, Crashes_, Timeouts_, Cr3s_, Uptime, UptimeUnit);

    fmt::print("{}", StatsStr);
    if (Log_) {
      fmt::print(Log_, "{}", StatsStr);
      std::fflush(Log_);
    }

    //
    // Remember last time we printed the stats as well as the coverage we
    // printed.
    //

    LastPrint_ = chrono::system_clock::now();
    LastCoverage_ = Coverage_;
  }

  //
  // Account for a new testcase the server has received.
  //

  void Testcase(const TestcaseResult_t &TestcaseResult, const uint64_t Coverage,
                const uint64_t Corpus, const uint64_t CorpusBytes) {
    TestcasesNumber_++;
    CorpusSize_ = Corpus;
    CorpusBytes_ = CorpusBytes;

    //
    // If the aggregated coverage is larger, then remember when this happened.
    //

    if (Coverage > Coverage_) {
      Coverage_ = Coverage;
      LastCov_ = chrono::system_clock::now();
    }

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

class Server_t {
  //
  // For now we have two states:
  //   - Read means that we are awaiting to receive from this socket,
  //   - Write means that we are awaiting to write to this socket.
  //

  enum class SocketState_t { Read, Write };

  //
  // This keeps track of the socket and their states.
  //

  std::unordered_map<SocketFd_t, SocketState_t> Clients_;

  //
  // Readset for select.
  //

  fd_set ReadSet_;

  //
  // WriteSet for select.
  //

  fd_set WriteSet_;

  //
  // RNG.
  //

  std::mt19937_64 Rng_;

  //
  // The corpus.
  //

  Corpus_t Corpus_;

  //
  // Scratch buffer that we use to receive data.
  //

  std::unique_ptr<uint8_t[]> ScratchBufferGrip_;

  //
  // This is a span over the scratch buffer above.
  //

  std::span<uint8_t> ScratchBuffer_;

  //
  // Mutator.
  //

  std::unique_ptr<Mutator_t> Mutator_;

  //
  // Master options.
  //

  const MasterOptions_t &Opts_;

  //
  // Server socket that listens.
  //

  SocketFd_t Server_ = INVALID_SOCKET;

  //
  // Server stats.
  //

  ServerStats_t Stats_;

  //
  // Log file.
  //

  std::FILE *Log_ = nullptr;

  //
  // This stores paths to files that needs their content to be sent to clients
  // as is, without mutations. This is useful to send the corpus files for
  // example. It is later sorted from the biggest size to the smallest.
  //

  std::vector<fs::path> Paths_;

  //
  // Aggregated coverage.
  //

  tsl::robin_set<Gva_t> Coverage_;

  //
  // Number of mutations we have performed.
  //

  uint64_t Mutations_ = 0;

public:
  explicit Server_t(const MasterOptions_t &Opts)
      : Opts_(Opts), Rng_(Opts.Seed), Corpus_(Opts.OutputsPath, Rng_) {

    std::filesystem::path LogPath("master.log");
    Log_ = std::fopen(LogPath.string().c_str(), "w");

    if (!Log_) {
      fmt::print("Failed to open log file: {}\n", LogPath.string());
      std::abort();
    }

    FD_ZERO(&ReadSet_);
    FD_ZERO(&WriteSet_);
  }

  //
  // Rule of three.
  //

  ~Server_t() {
    if (Log_) {
      std::fclose(Log_);
    }

    for (const auto &[Fd, _] : Clients_) {
      CloseSocket(Fd);
    }
  }

  Server_t(const Server_t &) = delete;
  Server_t &operator=(const Server_t &) = delete;

  //
  // Run the server.
  //

  int Run(const Target_t &Target) {

    //
    // Set up RNG.
    //

    fmt::print("Seeded with {}\n", Opts_.Seed);

    //
    // Initialize our internal state.
    //

    ScratchBufferGrip_ = std::make_unique<uint8_t[]>(_1MB);
    ScratchBuffer_ = {ScratchBufferGrip_.get(), _1MB};

    if (Opts_.TestcaseBufferMaxSize > ScratchBuffer_.size_bytes()) {
      fmt::print("The biggest testcase would not fit in the scratch buffer\n");
      return EXIT_FAILURE;
    }

    //
    // We'll have at most FD_SETSIZE clients, so let's preallocate whatever we
    // can.
    //

    std::vector<SocketFd_t> ReadFds, WriteFds;
    ReadFds.reserve(FD_SETSIZE);
    WriteFds.reserve(FD_SETSIZE);
    Clients_.reserve(FD_SETSIZE);

    //
    // Instantiate the mutator.
    //

    Mutator_ = Target.CreateMutator(Rng_, Opts_.TestcaseBufferMaxSize);

    //
    // Prepare initial seeds.
    //

    fmt::print("Iterating through the corpus..\n");
    const fs::directory_iterator DirIt(Opts_.InputsPath);
    for (const auto &DirEntry : DirIt) {
      Paths_.emplace_back(DirEntry);
    }

    //
    // Note that we use rbegin / rend because we want to order the vector from
    // biggest to smallest. We do that because that it's easy to pop data off
    // the vector and it means we don't have to move items either.
    //

    fmt::print("Sorting through the {} entries..\n", Paths_.size());
    std::sort(Paths_.rbegin(), Paths_.rend(), CompareTwoFileBySize);

    //
    // Let's kick off the server.
    //

    fmt::print("Running server on {}..\n", Opts_.Address);
    auto ServerOpt = Listen(Opts_.Address);
    if (!ServerOpt) {
      fmt::print("Listen() failed\n");
      return EXIT_FAILURE;
    }

    //
    // The server socket is also a client.
    //

    Server_ = *ServerOpt;
    Clients_.emplace(Server_, SocketState_t::Read);

    int Ret = EXIT_SUCCESS;
    while (Ret == EXIT_SUCCESS) {

      //
      // Zero out the sets.
      //

      FD_ZERO(&ReadSet_);
      FD_ZERO(&WriteSet_);
      ReadFds.clear();
      WriteFds.clear();

      //
      // For now, the Server socket is the biggest fd AFAWK.
      //

      SocketFd_t MaxFd = Server_;

      //
      // Let's iterate through our fds and put them in the right vector based on
      // their states. While doing that, also let's keep looking for the biggest
      // fd.
      // The reason for having ReadFds / WriteFds vectors are:
      //   1/ I don't think there's a cross-platform way of iterating through
      //   the fdset_t internal data structure to find the fds,
      //   2/ Instead, if iterate over Clients_, accepting / disconnecting
      //   clients modifies Clients_ *while* getting iterated on which is
      //   annoying. Instead, we keep those two small vectors arround.
      //

      for (const auto &[Fd, State] : Clients_) {
        MaxFd = std::max(MaxFd, Fd);
        if (State == SocketState_t::Read) {
          FD_SET(Fd, &ReadSet_);
          ReadFds.emplace_back(Fd);
        } else {
          FD_SET(Fd, &WriteSet_);
          WriteFds.emplace_back(Fd);
        }
      }

      //
      // Wait for activity.
      //

      if (select(MaxFd + 1, &ReadSet_, &WriteSet_, nullptr, nullptr) == -1) {
        fmt::print("select failed with {}\n", SocketError());
        break;
      }

      //
      // Display some stats.
      //

      Stats_.Print(false, Log_);

      //
      // Scan the read set.
      //

      for (const auto &Fd : ReadFds) {
        //
        // If the Fd is not in the read set, let's continue.
        //

        if (!FD_ISSET(Fd, &ReadSet_)) {
          continue;
        }

        //
        // The server socket is a special case as it means that a client is
        // awaiting us to accept the connexion.
        //

        const bool IsServer = Fd == Server_;
        if (IsServer) {
          if (!HandleNewConnection()) {
            fmt::print("NewConnection failed\n");
            Ret = EXIT_FAILURE;
            break;
          }

          //
          // Once we handled the new connection we're done.
          //

          continue;
        }

        //
        // Otherwise, it means a client sent us a new result so handle that.
        //

        if (!HandleNewResult(Fd, Target)) {

          //
          // If we failed handling of the result, let's just disconnect the
          // client.
          //

          if (!Disconnect(Fd)) {

            //
            // If we failed doing that... well, let's shut everything down.
            //

            fmt::print("Disconnect failed\n");
            Ret = EXIT_FAILURE;
            break;
          }
        }
      }

      //
      // Let's check if we have no more paths to handle and that we aren't going
      // above the number of mutations we are supposed to do.
      //

      if (Mutations_ >= Opts_.Runs && Paths_.size() == 0) {
        fmt::print("Completed {} mutations, time to stop the server..\n",
                   Mutations_);
        break;
      }

      //
      // Scan the write set.
      //

      for (const auto &Fd : WriteFds) {

        //
        // If the fd is not in the write set, let's continue.
        //

        if (!FD_ISSET(Fd, &WriteSet_)) {
          continue;
        }

        //
        // If the fd is in the write set, it means that it is ready to receive a
        // new testcase, so let's do that.
        // If we failed sending a new testcase over, then let's try to
        // disconnect the client.
        //

        if (!HandleNewRequest(Fd, Target) && !Disconnect(Fd)) {

          //
          // If we failed to disconnect the client... let's just call it quits.
          //

          fmt::print("Disconnect failed\n");
          Ret = EXIT_FAILURE;
          break;
        }
      }
    }

    //
    // We exited, so let's force displaying the stats one last time.
    //

    Stats_.Print(true);
    return Ret;
  }

private:
  //
  // Disconnects Fd.
  //

  bool Disconnect(const SocketFd_t Fd) {

    //
    // We close the socket, as well as removing Fd off our internal data
    // structures.
    //

    CloseSocket(Fd);
    Clients_.erase(Fd);

    //
    // We also notify the stats and force print it to let the user know that a
    // node dropped.
    //

    Stats_.DisconnectClient();
    Stats_.Print(true);
    return true;
  }

  //
  // Generates a testcase.
  //

  std::string GetTestcase(const Target_t &Target) {

    //
    // If we have paths, it means we haven't finished to run through the corpus
    // yet, so this takes priority over mutation stage.
    //

    if (Paths_.size() > 0) {

      //
      // Let's try to read a file.
      //

      std::unique_ptr<uint8_t[]> Buffer;
      size_t BufferSize = 0;

      //
      // We'll loop over the paths until we are able to read a file.
      //

      bool FoundFile = false;
      while (!FoundFile && Paths_.size() > 0) {

        //
        // Let's grab the smallest available file.
        //

        const auto &Path = Paths_.back();

        //
        // Let's try to read it.
        //

        Buffer = ReadFile(Path, BufferSize);

        //
        // If reading the file failed for whatever reasons, or if the file is
        // too big, we're looping and trying again.
        //

        const bool Valid =
            BufferSize > 0 && BufferSize <= Opts_.TestcaseBufferMaxSize;

        if (!Valid) {
          fmt::print("Skipping because {} size is zero or bigger than the max "
                     "({} vs {})\n",
                     Path.string(), BufferSize, Opts_.TestcaseBufferMaxSize);
        }

        //
        // We are done with this path now!
        //

        Paths_.pop_back();

        if (!Valid) {
          continue;
        }

        //
        // Phew, we read a file \o/
        //

        FoundFile = true;
      }

      //
      // If we have read a file successfully in the previous stage, this is the
      // testcase we'll return to the caller.
      //

      if (FoundFile) {
        std::string TestcaseContent;
        TestcaseContent.resize(BufferSize);
        memcpy(TestcaseContent.data(), Buffer.get(), BufferSize);
        return TestcaseContent;
      }
    }

    //
    // Ask the mutator to generate a testcase.
    //

    Mutations_++;
    return Mutator_->GetNewTestcase(Corpus_);
  }

  //
  // Sends a testcase to a client.
  //

  bool SendTestcase(const SocketFd_t &Fd, const std::string &Testcase) {

    //
    // Serialize the message.
    //

    yas::mem_ostream Os;
    yas::binary_oarchive<decltype(Os), YasFlags> Oa(Os);
    Oa &Testcase;
    const auto &Buf = Os.get_intrusive_buffer();
    if (!Send(Fd, (uint8_t *)Buf.data, Buf.size)) {
      fmt::print("Send failed\n");
      return false;
    }

    return true;
  }

  //
  // The client wants a new testcase.
  //

  bool HandleNewRequest(const SocketFd_t Fd, const Target_t &Target) {

    //
    // Prepare a message to send to a client.
    //

    const std::string Testcase = GetTestcase(Target);

    //
    // Send the testcase.
    //

    if (!SendTestcase(Fd, Testcase)) {
      fmt::print("SendTestcase failed\n");
      return false;
    }

    //
    // We'll be waiting for an answer from this client.
    //

    Clients_[Fd] = SocketState_t::Read;
    return true;
  }

  //
  // Deserializes a result from a client.
  //

  bool DeserializeResult(const std::span<uint8_t> Buffer,
                         std::string &ReceivedTestcase,
                         tsl::robin_set<Gva_t> &Coverage,
                         TestcaseResult_t &Result) {
    yas::mem_istream Is(Buffer.data(), Buffer.size_bytes());
    yas::binary_iarchive<decltype(Is), YasFlags> Ia(Is);
    Ia &ReceivedTestcase &Coverage &Result;
    return true;
  }

  //
  // The client sent a result.
  //

  bool HandleNewResult(const SocketFd_t Fd, const Target_t &Target) {

    //
    // Receive client data into the scratch buffer.
    //

    const auto ReceivedSize = Receive(Fd, ScratchBuffer_);
    if (!ReceivedSize) {
      fmt::print("Receive failed\n");
      return false;
    }

    //
    // Let's deserialize the response from the client.
    //

    const std::span<uint8_t> ReceivedBuffer(ScratchBuffer_.data(),
                                            *ReceivedSize);
    std::string ReceivedTestcase;
    tsl::robin_set<Gva_t> Coverage;
    TestcaseResult_t Result;
    if (!DeserializeResult(ReceivedBuffer, ReceivedTestcase, Coverage,
                           Result)) {
      fmt::print("DeserializeResult failed\n");
      return false;
    }

    //
    // If the client says has new coverage, then let's have a look.
    //

    if (Coverage.size() > 0) {

      //
      // Emplace the new coverage in our data.
      //

      const size_t SizeBefore = Coverage_.size();
      Coverage_.insert(Coverage.cbegin(), Coverage.cend());

      //
      // If the coverage size has changed, it means that this testcase
      // provided new coverage indeed.
      //

      const bool NewCoverage = Coverage_.size() > SizeBefore;
      if (NewCoverage) {

        //
        // Allocate a test that will get moved into the corpus and maybe
        // saved on disk.
        //

        Testcase_t Testcase((uint8_t *)ReceivedTestcase.data(),
                            ReceivedTestcase.size());

        //
        // Before moving the buffer into the corpus, set up cross over with
        // it.
        //

        Mutator_->OnNewCoverage(Testcase);

        //
        // Ready to move the buffer into the corpus now.
        //

        Corpus_.SaveTestcase(Result, std::move(Testcase));
      }
    }

    //
    // If the client reported a crash, let's check if it has a name, if so
    // we'll save it in the crashes folder.
    //

    if (const auto &Crash = std::get_if<Crash_t>(&Result)) {
      if (Crash->CrashName.size() > 0) {
        const auto &OutputPath = Opts_.CrashesPath / Crash->CrashName;
        const auto &Success =
            SaveFile(OutputPath, (uint8_t *)ReceivedTestcase.data(),
                     ReceivedTestcase.size());
        if (!Success) {
          fmt::print("Could not create the destination file.\n");
          return false;
        }

        const bool WroteFile = *Success;
        if (WroteFile) {
          fmt::print("Saving crash in {}\n", OutputPath.string());
        }
      }
    }

    //
    // We'll be waiting to be able to send this client a new job.
    //

    Clients_[Fd] = SocketState_t::Write;
    Stats_.Testcase(Result, Coverage_.size(), Corpus_.Size(), Corpus_.Bytes());
    return true;
  }

  bool HandleNewConnection() {
    const SocketFd_t Client = accept(Server_, nullptr, nullptr);
    if (Client == INVALID_SOCKET) {
      fmt::print("accept failed\n");
      return false;
    }

    //
    // Keep track of the client.
    //

    Clients_.emplace(Client, SocketState_t::Write);
    Stats_.NewClient();
    Stats_.Print(true);
    return true;
  }
};
