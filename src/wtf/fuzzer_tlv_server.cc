// y0ny0ns0n / Axel'0vercl0k' Souchet - February 3 2022
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

constexpr bool LoggingOn = true;

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
  const auto &J = json::json::parse(Buffer, Buffer + BufferSize);
  const auto &Packets = J["Packets"];
  for (const auto &Packet : Packets) {
    auto DeserializedPacket = Packet.get<Packet_t>();
    GlobalState.Testcases.emplace_back(std::move(DeserializedPacket));
  }

  return GlobalState.Testcases.size() > 0;
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

//
// Register the target.
//

Target_t tlv_server("tlv_server", Init, InsertTestcase, Restore);

} // namespace TlvServer
