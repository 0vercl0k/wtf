// y0ny0ns0n - February 1 2022
#include "backend.h"
#include "crash_detection_umode.h"
#include "mutator.h"
#include "targets.h"
#include "utils.h"
#include <fmt/format.h>

namespace fs = std::filesystem;

using namespace std;

#define FIELD_SIZEOF(t, f) (sizeof(((t *)0)->f))

/*
test_tlv_server source code:

https://gist.github.com/y0ny0ns0n/389654ff9eb2b1541367de2b365c8210

*/

namespace test_tlv_server {

std::unique_ptr<LibfuzzerMutator_t> Mutator_ = NULL;
vector<pair<uint8_t *, uint32_t> *> Testcases_;
size_t Testcase_CurIdx;
size_t Testcase_LastIdx;
uint64_t MaxTestcaseCount = 100;
uint64_t SingleTestcaseMaxSize = 0x3FF;

uint64_t g_Rsp, g_Rip, g_Rax, g_Rbx, g_Rcx, g_Rdx, g_Rsi, g_Rdi, g_R8, g_R9,
    g_R10, g_R11, g_R12, g_R13, g_R14, g_R15;

#pragma pack(push, 1)
typedef struct COMMON_PACKET_HEADER {
  uint32_t CommandId;
  uint32_t BodySize;
} COMMON_PACKET_HEADER;
#pragma pack(pop)

void Save64bitRegs() {
  g_Rsp = g_Backend->Rsp();
  g_Rip = g_Backend->Rip();
  g_Rax = g_Backend->Rax();
  g_Rbx = g_Backend->Rbx();
  g_Rcx = g_Backend->Rcx();
  g_Rdx = g_Backend->Rdx();
  g_Rsi = g_Backend->Rsi();
  g_Rdi = g_Backend->Rdi();
  g_R8 = g_Backend->R8();
  g_R9 = g_Backend->R9();
  g_R10 = g_Backend->R10();
  g_R11 = g_Backend->R11();
  g_R12 = g_Backend->R12();
  g_R13 = g_Backend->R13();
  g_R14 = g_Backend->R14();
  g_R15 = g_Backend->R15();
}

void Restore64bitRegs() {
  g_Backend->Rsp(g_Rsp);
  g_Backend->Rip(g_Rip);
  g_Backend->Rax(g_Rax);
  g_Backend->Rbx(g_Rbx);
  g_Backend->Rcx(g_Rcx);
  g_Backend->Rdx(g_Rdx);
  g_Backend->Rsi(g_Rsi);
  g_Backend->Rdi(g_Rdi);
  g_Backend->R8(g_R8);
  g_Backend->R9(g_R9);
  g_Backend->R10(g_R10);
  g_Backend->R11(g_R11);
  g_Backend->R12(g_R12);
  g_Backend->R13(g_R13);
  g_Backend->R14(g_R14);
  g_Backend->R15(g_R15);
}

constexpr bool LoggingOn = false;

template <typename... Args_t>
void DebugPrint(const char *Format, const Args_t &...args) {
  if constexpr (LoggingOn) {
    fmt::print("test_tlv_server: ");
    fmt::print(Format, args...);
  }
}

bool InsertTestcase(const uint8_t *Buffer, const size_t BufferSize) {
  DebugPrint("InsertTestcase: called\n");

  // initializing testcase vector
  for (int i = 0; i < Testcases_.size(); i++) {
    free(Testcases_.at(i)->first); // free testcase_ptr
    free(Testcases_.at(i));        // free testcase
  }

  Testcases_.clear();
  Testcase_CurIdx = 0;

  size_t idx = 0;
  uint32_t testcase_size = 0;
  uint8_t *testcase_ptr = NULL;
  pair<uint8_t *, uint32_t> *testcase = NULL;
  Testcase_LastIdx = 0;

  while (idx < BufferSize) {
    testcase_size = *((uint32_t *)&Buffer[idx]);
    idx += 4;

    // +1 for pad
    testcase_ptr = (uint8_t *)calloc(1, testcase_size + 1);

    memcpy(testcase_ptr, &Buffer[idx], testcase_size);
    idx += testcase_size;

    testcase = new pair<uint8_t *, uint32_t>();
    testcase->first = testcase_ptr;
    testcase->second = testcase_size;

    Testcases_.push_back(testcase);
    Testcase_LastIdx += 1;
  }

  return true;
}

bool Init(const Options_t &Opts, const CpuState_t &) {
  //
  // return addr = rip + 5
  //
  // 0033:00007ff7`9d821894 e867f7ffff       call test_tlv_server!process_packet
  // (00007ff7`9d821000) <- take a snapshot on here 0033:00007ff7`9d821899
  // 488b4c2438       mov     rcx, qword ptr [rsp+38h]

  const Gva_t Rip = Gva_t(g_Backend->Rip());
  const Gva_t AfterCall = Rip + Gva_t(5);

  DebugPrint("Init: Rip = {:#x}, AfterCall = {:#x}\n", Rip.U64(),
             AfterCall.U64());

  Save64bitRegs();

  if (!g_Backend->SetBreakpoint(AfterCall, [](Backend_t *Backend) {
        Restore64bitRegs();
        DebugPrint("Aftercall reached, now go back!\n");
      })) {
    DebugPrint("Failed to SetBreakpoint AfterCall\n");
    return false;
  }

  //
  // NOP the calls to DbgPrintEx.
  //

  if (!g_Backend->SetBreakpoint("nt!DbgPrintEx", [](Backend_t *Backend) {
        const Gva_t FormatPtr = Backend->GetArgGva(2);
        const std::string &Format = Backend->VirtReadString(FormatPtr);
        DebugPrint("DbgPrintEx: {}", Format);
        Backend->SimulateReturnFromFunction(0);
      })) {
    DebugPrint("Failed to SetBreakpoint DbgPrintEx\n");
    return false;
  }

  //
  // Make ExGenRandom deterministic. <- have to edit based on ntoskrnl version
  //
  // kd> u nt!ExGenRandom+0xfb l1
  // nt!ExGenRandom+0xfb:
  // fffff800`6125073b 0fc7f2          rdrand  edx
  const Gva_t ExGenRandom = Gva_t(g_Dbg.GetSymbol("nt!ExGenRandom") + 0xfb);
  if (!g_Backend->SetBreakpoint(ExGenRandom, [](Backend_t *Backend) {
        DebugPrint("Hit ExGenRandom!\n");
        Backend->Rdx(Backend->Rdrand());
      })) {
    return false;
  }

  if (!g_Backend->SetBreakpoint(
          "test_tlv_server!process_packet", [](Backend_t *Backend) {
            if (Testcase_CurIdx == Testcase_LastIdx) {
              DebugPrint("No more testcases. goto next round\n");
              Backend->Stop(Ok_t());
            } else {
              DebugPrint("process packet called\n");

              uint8_t *testcase_ptr = Testcases_.at(Testcase_CurIdx)->first;
              uint32_t testcase_size = Testcases_.at(Testcase_CurIdx)->second;

              Gva_t PacketBuf = Backend->GetArgGva(0);
              DebugPrint("Testcase_CurIdx = {}, testcase_size = {}\n",
                         Testcase_CurIdx, testcase_size);

              if (testcase_size >
                  FIELD_SIZEOF(COMMON_PACKET_HEADER, CommandId)) {
                COMMON_PACKET_HEADER *hdr =
                    (COMMON_PACKET_HEADER *)testcase_ptr;

                // 1 = Alloc, 2 = Edit, 3 = delete
                hdr->CommandId = (hdr->CommandId % 3) + 1;
                hdr->BodySize = testcase_size - sizeof(COMMON_PACKET_HEADER);
              }

              Backend->VirtWriteDirty(PacketBuf, testcase_ptr, testcase_size);
              Backend->Rdx(testcase_size);

              Testcase_CurIdx += 1;
            }
          })) {
    DebugPrint("Failed to SetBreakpoint process_packet\n");
    return false;
  }

  if (!g_Backend->SetBreakpoint(
          "test_tlv_server!printf", [](Backend_t *Backend) {
            const Gva_t FormatPtr = Backend->GetArgGva(0);
            const std::string &Format = Backend->VirtReadString(FormatPtr);
            DebugPrint("printf: {}", Format);
            Backend->SimulateReturnFromFunction(0);
          })) {
    DebugPrint("Failed to SetBreakpoint printf\n");
    return false;
  }

  if (!g_Backend->SetBreakpoint(
          "ntdll!RtlEnterCriticalSection",
          [](Backend_t *Backend) { Backend->SimulateReturnFromFunction(0); })) {
    DebugPrint("Failed to SetBreakpoint RtlEnterCriticalSection\n");
    return false;
  }

  SetupUsermodeCrashDetectionHooks();

  return true;
}

bool Restore() { return true; }

size_t CustomMutate(uint8_t *Data, const size_t DataLen, const size_t MaxSize,
                    std::mt19937_64 Rng_) {

  vector<pair<uint8_t *, uint32_t>> mutated_testcases;

  size_t idx = 0;
  uint32_t testcase_size = 0;
  uint8_t *testcase_ptr = NULL;

  if (Mutator_ == NULL) {
    DebugPrint("CustomMutate: allocating new Mutator\n");
    Mutator_ = std::make_unique<LibfuzzerMutator_t>(Rng_);
  }

  while (idx < DataLen) {
    /**************************************************************
      multi input testcase layout

          +-----------------------------------+
          |                                   |
          |  size of 1th testcase( 4 bytes )  |
          |                                   |
          +-----------------------------------+
          |                                   |
          |      1th testcase( x bytes )      |
          |                                   |
          +-----------------------------------+
                            .
                            .
                            .
          +-----------------------------------+
          |                                   |
          |  size of Nth testcase( 4 bytes )  |
          |                                   |
          +-----------------------------------+
          |                                   |
          |      Nth testcase( y bytes )      |
          |                                   |
          +-----------------------------------+
    **************************************************************/

    testcase_size = *((uint32_t *)&Data[idx]);
    idx += 4;

    testcase_ptr = (uint8_t *)calloc(1, SingleTestcaseMaxSize);
    memcpy(testcase_ptr, &Data[idx], testcase_size);
    idx += testcase_size;

    testcase_size =
        Mutator_->Mutate(testcase_ptr, testcase_size, SingleTestcaseMaxSize);
    mutated_testcases.push_back(make_pair(testcase_ptr, testcase_size));
  }

  // initializing output buffer
  memset(Data, 0, DataLen);

  idx = 0;
  for (int i = 0; i < mutated_testcases.size(); i++) {
    testcase_ptr = mutated_testcases.at(i).first;
    testcase_size = mutated_testcases.at(i).second;

    *((uint32_t *)&Data[idx]) = testcase_size;
    idx += 4;

    memcpy(&Data[idx], testcase_ptr, testcase_size);
    idx += testcase_size;

    free(testcase_ptr);
  }

  mutated_testcases.clear();
  return idx;
}

void PostMutate(Testcase_t *testcase) {
  DebugPrint("PostMutate called\n");

  if (Mutator_ != NULL) {
    Mutator_->SetCrossOverWith(*testcase);
  }
}

//
// Register the target.
//

Target_t test_tlv_server("test_tlv_server", Init, InsertTestcase, Restore,
                         CustomMutate, PostMutate);

} // namespace test_tlv_server
