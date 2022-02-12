// y0ny0ns0n / Axel '0vercl0k' Souchet - February 3 2022
#include "backend.h"
#include "crash_detection_umode.h"
#include "mutator.h"
#include "nlohmann/json.hpp"
#include "targets.h"
#include "utils.h"
#include <deque>
#include <fmt/format.h>
#include <optional>

namespace TlvServer {

constexpr bool LoggingOn = false;

template <typename... Args_t>
void DebugPrint(const char *Format, const Args_t &...args) {
  if constexpr (LoggingOn) {
    fmt::print("tlv_server: ");
    fmt::print(Format, args...);
  }
}

struct Packet_t {
  uint32_t Command;
  uint16_t Id;
  uint16_t BodySize;
  std::vector<uint8_t> Body;
  NLOHMANN_DEFINE_TYPE_INTRUSIVE(Packet_t, Command, Id, BodySize, Body);
};

struct Packets_t {
  std::vector<Packet_t> Packets;
  NLOHMANN_DEFINE_TYPE_INTRUSIVE(Packets_t, Packets);
};

Packets_t Deserialize(const uint8_t *Buffer, const size_t BufferSize) {
  const auto &Root = json::json::parse(Buffer, Buffer + BufferSize);
  return Root.get<Packets_t>();
}

struct {
  std::deque<Packet_t> Testcases;
  CpuState_t Context;

  void RestoreGprs(Backend_t *B) {
    const auto &C = Context;
    B->Rsp(C.Rsp);
    B->Rip(C.Rip);
    B->Rax(C.Rax);
    B->Rbx(C.Rbx);
    B->Rcx(C.Rcx);
    B->Rdx(C.Rdx);
    B->Rsi(C.Rsi);
    B->Rdi(C.Rdi);
    B->R8(C.R8);
    B->R9(C.R9);
    B->R10(C.R10);
    B->R11(C.R11);
    B->R12(C.R12);
    B->R13(C.R13);
    B->R14(C.R14);
    B->R15(C.R15);
  }
} GlobalState;

bool InsertTestcase(const uint8_t *Buffer, const size_t BufferSize) {
  const auto &DeserializedPackets = Deserialize(Buffer, BufferSize);
  for (auto DeserializedPacket : DeserializedPackets.Packets) {
    GlobalState.Testcases.emplace_back(std::move(DeserializedPacket));
  }

  return true;
}

bool Init(const Options_t &Opts, const CpuState_t &State) {
  GlobalState.Context = State;

  const Gva_t Rsp = Gva_t(g_Backend->Rsp());
  const Gva_t ReturnAddress = Gva_t(g_Backend->VirtRead8(Rsp));
  if (!g_Backend->SetBreakpoint(
          "tlv_server!ProcessPacket", [](Backend_t *Backend) {
            if (GlobalState.Testcases.size() == 0) {

              //
              // We are done with the testcase so return to the engine.
              //

              return g_Backend->Stop(Ok_t());
            }

            //
            // Let's insert the testcase in memory now.
            //

            const auto &Testcase = GlobalState.Testcases.front();

            //
            // Calculate the size of the testcase and update the CPU context.
            //

            const size_t PacketSize = sizeof(uint32_t) + sizeof(uint16_t) +
                                      sizeof(uint16_t) + Testcase.Body.size();

            if (PacketSize >= 0x1000) {
              GlobalState.Testcases.pop_front();
              Backend->Stop(Ok_t());
              fmt::print("This testcase is too big to fit, bailing\n");
              return;
            }

            Backend->Rdx(PacketSize);

            //
            // Calculate the address of the packet buffer and push it as close
            // as possible to the end of the page so that out-of-bounds hit the
            // guard page behind.
            //

            const auto &PacketOriginalAddress = Backend->Rcx();
            auto PacketAddress = PacketOriginalAddress + (0x1'000 - PacketSize);

            Backend->Rcx(PacketAddress);

            //
            // Insert the testcase in memory now.
            //

            if (!Backend->VirtWriteStructDirty(Gva_t(PacketAddress),
                                               &Testcase.Command)) {
              fmt::print("Failed to write the command\n");
              std::abort();
            }

            PacketAddress += sizeof(Testcase.Command);

            if (!Backend->VirtWriteStructDirty(Gva_t(PacketAddress),
                                               &Testcase.Id)) {
              fmt::print("Failed to write the id\n");
              std::abort();
            }

            PacketAddress += sizeof(Testcase.Id);

            if (!Backend->VirtWriteStructDirty(Gva_t(PacketAddress),
                                               &Testcase.BodySize)) {
              fmt::print("Failed to write the body size\n");
              std::abort();
            }

            PacketAddress += sizeof(Testcase.BodySize);

            if (!Backend->VirtWriteDirty(Gva_t(PacketAddress),
                                         Testcase.Body.data(),
                                         Testcase.Body.size())) {
              fmt::print("Failed to write the body\n");
              std::abort();
            }

            //
            // We're done with this testcase!
            //

            GlobalState.Testcases.pop_front();
          })) {
    DebugPrint("Failed to SetBreakpoint ProcessPacket\n");
    return false;
  }

  if (!g_Backend->SetBreakpoint(ReturnAddress, [](Backend_t *Backend) {
        //
        // Restore the register as we prepare ourselves to deliver another
        // testcase.
        //

        GlobalState.RestoreGprs(g_Backend);
        DebugPrint("Ready to get back on entry point!\n");
      })) {
    fmt::print("Failed to SetBreakpoint on the return address.\n");
    return false;
  }

  if (!g_Backend->SetBreakpoint("tlv_server!printf", [](Backend_t *Backend) {
        const Gva_t FormatPtr = Backend->GetArgGva(0);
        const std::string &Format = Backend->VirtReadString(FormatPtr);
        DebugPrint("printf: {}", Format);
        Backend->SimulateReturnFromFunction(0);
      })) {
    fmt::print("Failed to SetBreakpoint on printf\n");
    return false;
  }

  if (!SetupUsermodeCrashDetectionHooks()) {
    fmt::print("Failed to SetupUsermodeCrashDetectionHooks\n");
    return false;
  }

  return true;
}

bool Restore() { return true; }

class CustomMutator_t : public Mutator_t {
  std::unique_ptr<uint8_t[]> ScratchBuffer__;
  span_u8 ScratchBuffer_;
  size_t TestcaseMaxSize_ = 0;

public:
  static std::unique_ptr<Mutator_t> Create(std::mt19937_64 &Rng,
                                           const size_t TestcaseMaxSize) {
    return std::make_unique<CustomMutator_t>(Rng, TestcaseMaxSize);
  }

  explicit CustomMutator_t(std::mt19937_64 &Rng, const size_t TestcaseMaxSize)
      : Rng_(Rng), TestcaseMaxSize_(TestcaseMaxSize) {
    ScratchBuffer__ = std::make_unique<uint8_t[]>(_1MB);
    ScratchBuffer_ = {ScratchBuffer__.get(), _1MB};
  }

  std::string GetNewTestcase(const Corpus_t &Corpus) override {
    const bool Generational = GetUint32(1, 5) == 5;
    if (Generational) {
      return Generate();
    }

    const Testcase_t *Testcase = Corpus.PickTestcase();
    if (!Testcase) {
      fmt::print("The corpus is empty, exiting\n");
      std::abort();
    }

    //
    // Copy the input in a buffer we're going to mutate.
    //

    memcpy(ScratchBuffer_.data(), Testcase->Buffer_.get(),
           Testcase->BufferSize_);
    return Mutate(ScratchBuffer_.data(), Testcase->BufferSize_,
                  ScratchBuffer_.size_bytes());
  }

private:
  std::string Generate() {
    Packets_t Root;
    const auto N = GetUint32(1, 10);
    for (size_t Idx = 0; Idx < N; Idx++) {
      Packet_t Packet;
      Packet.Id = Idx;
      Packet.Command = GetUint32(0, 10);
      Packet.Body.resize(GetUint32(0, 100));
      Packet.BodySize = Packet.Body.size();
      if (GetUint32(1, 3) == 1) {
        Packet.BodySize ^= 1 << GetUint32(0, 15);
      }

      Root.Packets.emplace_back(Packet);
    }

    json::json Serialized;
    to_json(Serialized, Root);
    return Serialized.dump();
  }

  std::string Mutate(uint8_t *Data, const size_t DataLen,
                     const size_t MaxSize) {
    enum Transformation_t : uint32_t {
      Start,
      InsertPacket = Start,
      CopyField,
      DeletePacket,
      End = DeletePacket
    };
    auto Root = Deserialize(Data, DataLen);
    auto &Packets = Root.Packets;
    DebugPrint("Mutate: {} packets\n", Packets.size());
    const auto Transformation = Transformation_t(
        GetUint32(Transformation_t::Start, Transformation_t::End));
    switch (Transformation) {
    case Transformation_t::InsertPacket: {
      MutationInsertPacket(Packets);
      break;
    }

    case Transformation_t::CopyField: {
      MutationCopyField(Packets);
      break;
    }

    case Transformation_t::DeletePacket: {
      MutationDeletePacket(Packets);
      break;
    }
    }

    json::json Serialized;
    to_json(Serialized, Root);
    return Serialized.dump();
  }

  uint32_t GetUint32(const uint32_t A, const uint32_t B) {
    return std::uniform_int_distribution<uint32_t>(A, B)(Rng_);
  }

  void MutationCopyField(std::vector<Packet_t> &Packets) {

    //
    // Copy a field from another packet.
    //

    const uint32_t SrcIdx = GetUint32(0, Packets.size() - 1);
    const uint32_t DstIdx = GetUint32(0, Packets.size() - 1);
    const uint32_t FieldIdx = GetUint32(0, 3);
    const auto &Src = Packets[SrcIdx];
    auto &Dst = Packets[DstIdx];
    switch (FieldIdx) {
    case 0: {
      Dst.Id = Src.Id;
      break;
    }

    case 1: {
      Dst.Command = Src.Command;
      break;
    }

    case 2: {
      Dst.BodySize = Src.BodySize;
      break;
    }

    case 3: {
      Dst.Body = Src.Body;
      break;
    }
    }
  }

  void MutationInsertPacket(std::vector<Packet_t> &Packets) {

    //
    // Insert a packet somewhere.
    //

    if (Packets.size() > 10) {
      return;
    }

    const uint32_t FromIdx = GetUint32(0, Packets.size() - 1);
    const uint32_t ToIdx = GetUint32(0, Packets.size());
    Packets.insert(Packets.begin() + ToIdx, Packets[FromIdx]);
  }

  void MutationDeletePacket(std::vector<Packet_t> &Packets) {

    //
    // Delete a packet.
    //

    const uint32_t SrcIdx = GetUint32(0, Packets.size() - 1);
    Packets.erase(Packets.begin() + SrcIdx);
  }

private:
  std::mt19937_64 &Rng_;
};

//
// Register the target.
//

Target_t TlvServer("tlv_server", Init, InsertTestcase, Restore,
                   CustomMutator_t::Create);

} // namespace TlvServer
