// Axel '0vercl0k' Souchet - May 24 2020
#pragma once
#include "platform.h"

#ifdef WINDOWS
#include "backend.h"
#include "tsl/robin_map.h"
#include "tsl/robin_set.h"
#include "utils.h"
#include <WinHvPlatform.h>
#include <cstdint>
#include <fmt/format.h>
#include <string>

#define WHvX64ExceptionTypeFailFast (0x29)

//
// This is the run stats for the WHV backend.
//

struct WhvRunStats_t {
  uint64_t PageFaults = 0;
  uint64_t Dirty = 0;
  uint64_t Vmexits = 0;

public:
  void Print() {
    fmt::print("--------------------------------------------------\n");
    fmt::print("Run stats:\n");
    const uint64_t DirtyMemoryBytes = Dirty * Page::Size;
    const uint64_t DirtyMemoryMb = Dirty / Page::Size;
    fmt::print("Dirty pages: {} bytes, {} pages, {} MB\n", DirtyMemoryBytes,
               Dirty, DirtyMemoryMb);
    fmt::print("Page-faults: {}\n", PageFaults);
    fmt::print("    VMExits: {}\n", Vmexits);
  }

  void Reset() {
    PageFaults = 0;
    Dirty = 0;
    Vmexits = 0;
  }
};

//
// A breakpoint is basically a Gpa and a handler.
//

struct WhvBreakpoint_t {
  Gpa_t Gpa;
  BreakpointHandler_t Handler;

  explicit WhvBreakpoint_t(const Gpa_t Gpa_, const BreakpointHandler_t Handler_)
      : Gpa(Gpa_), Handler(Handler_) {}
};

//
// This is the WHV backend. It runs test cases inside an Hyper-V backed VM.
//

class WhvBackend_t : public Backend_t {

  //
  // This is the small utility class we use to set a time limit; when the timer
  // expires, we cancel the execution of the VP.
  // Stolen from libfuzzer.
  //

  class TimerQ_t {
    HANDLE TimerQueue_ = nullptr;
    HANDLE LastTimer_ = nullptr;

    static void CALLBACK AlarmHandler(PVOID, BOOLEAN) {
      reinterpret_cast<WhvBackend_t *>(g_Backend)->CancelRunVirtualProcessor();
    }

  public:
    ~TimerQ_t() {
      if (TimerQueue_) {
        DeleteTimerQueueEx(TimerQueue_, nullptr);
      }
    }

    TimerQ_t() = default;
    TimerQ_t(const TimerQ_t &) = delete;
    TimerQ_t &operator=(const TimerQ_t &) = delete;

    void SetTimer(const uint32_t Seconds) {
      if (Seconds == 0) {
        return;
      }

      if (!TimerQueue_) {
        TimerQueue_ = CreateTimerQueue();
        if (!TimerQueue_) {
          fmt::print("CreateTimerQueue failed.\n");
          exit(1);
        }
      }

      if (!CreateTimerQueueTimer(&LastTimer_, TimerQueue_, AlarmHandler,
                                 nullptr, Seconds * 1000, Seconds * 1000, 0)) {
        fmt::print("CreateTimerQueueTimer failed.\n");
        exit(1);
      }
    }

    void TerminateLastTimer() {
      DeleteTimerQueueTimer(TimerQueue_, LastTimer_, nullptr);
    }
  };

  //
  // This is the VM's handle.
  //

  WHV_PARTITION_HANDLE Partition_ = nullptr;

  //
  // This is the index of the virtual processor. We only support a single
  // virtual processor.
  //

  uint32_t Vp_ = 0;

  //
  // Breakpoints. This maps a GVA to a breakpoint.
  //

  tsl::robin_map<Gva_t, WhvBreakpoint_t> Breakpoints_;

  //
  // This tracks every dirty GPA.
  //

  tsl::robin_set<Gpa_t> DirtyGpas_;

  //
  // This keeps track of every code coverage breakpoints. This maps a GVA to a
  // GPA.
  //

  tsl::robin_map<Gva_t, Gpa_t> CovBreakpoints_;

  //
  // This is a list of every basic block GVA the last test-case hit.
  //

  tsl::robin_set<Gva_t> Coverage_;

  //
  // This is the GPA of the last breakpoint we disabled.
  //

  Gpa_t LastBreakpointGpa_ = Gpa_t(0xffffffffffffffff);

  //
  // This is the RAM.
  //

  Ram_t Ram_;

  //
  // This is a seed we use to implement our deterministic RDRAND. The seed needs
  // to get restored for every test cases.
  //

  uint64_t Seed_ = 0;

  //
  // Do we need to stop the VP?
  //

  bool Stop_ = false;

  //
  // The result of the current test case.
  //

  TestcaseResult_t TestcaseRes_ = Ok_t();

  //
  // This is the currently executed test case.
  //

  const uint8_t *TestcaseBuffer_ = nullptr;

  //
  // This is the size of the test case.
  //

  uint64_t TestcaseBufferSize_ = 0;

  //
  // This is the path where we find mesos files.
  //

  fs::path CoveragePath_;

  //
  // This is the run stats for the current test case.
  //

  WhvRunStats_t RunStats_;

  //
  // This is the time limit we use to protect ourselves against infinite loops /
  // long test cases.
  //

  uint32_t Limit_ = 0;

  //
  // This is the trace type.
  //

  TraceType_t TraceType_ = TraceType_t::NoTrace;

  //
  // This is the trace file if we are tracing the current test case.
  //

  FILE *TraceFile_ = nullptr;

  //
  // This is the timer we use to terminate long testcases.
  //

  TimerQ_t Timer_;

public:
  WhvBackend_t() = default;
  ~WhvBackend_t();
  WhvBackend_t(const WhvBackend_t &) = delete;
  WhvBackend_t &operator=(const WhvBackend_t &) = delete;

  //
  // Initialize the backend.
  //

  bool Initialize(const Options_t &Opts, const CpuState_t &CpuState) override;

  //
  // Execution.
  //

  std::optional<TestcaseResult_t> Run(const uint8_t *Buffer,
                                      const uint64_t BufferSize) override;

  bool Restore(const CpuState_t &CpuState) override;

  void Stop(const TestcaseResult_t &Res) override;

  void SetLimit(const uint64_t Limit) override;

  //
  // Registers.
  //

  uint64_t GetReg(const Registers_t Reg) override;
  uint64_t SetReg(const Registers_t Reg, const uint64_t Value) override;

  //
  // Non-determinism.
  //

  uint64_t Rdrand() override;

  //
  // Stats.
  //

  void PrintRunStats() override;

  //
  // Tracing.
  //

  bool SetTraceFile(const fs::path &TestcaseTracePath,
                    const TraceType_t TraceType) override;

  //
  // Breakpoints.
  //

  bool SetBreakpoint(const Gva_t Gva,
                     const BreakpointHandler_t Handler) override;

  //
  // Virtual memory access.
  //

  bool DirtyGpa(const Gpa_t Gpa) override;

  bool VirtTranslate(const Gva_t Gva, Gpa_t &Gpa,
                     const MemoryValidate_t Validate) const override;

  uint8_t *PhysTranslate(const Gpa_t Gpa) const override;

  bool PageFaultsMemoryIfNeeded(const Gva_t Gva, const uint64_t Size) override;

  //
  // Get the last new coverage generated by the last executed testcase.
  //

  const tsl::robin_set<Gva_t> &LastNewCoverage() const override;

  bool RevokeLastNewCoverage() override;

  bool InsertCoverageEntry(const Gva_t Gva) override;

private:
  HRESULT
  OnDebugTrap(const WHV_RUN_VP_EXIT_CONTEXT &Exception);
  HRESULT
  OnBreakpointTrap(const WHV_RUN_VP_EXIT_CONTEXT &Exception);
  HRESULT OnExitReasonException(const WHV_RUN_VP_EXIT_CONTEXT &Exception);
  HRESULT OnExitCoverageBp(const WHV_RUN_VP_EXIT_CONTEXT &Exception);
  HRESULT OnExitReasonMemoryAccess(const WHV_RUN_VP_EXIT_CONTEXT &Exception);

  HRESULT SetPartitionProperty(const WHV_PARTITION_PROPERTY_CODE PropertyCode,
                               const uint64_t PropertyValue);
  HRESULT LoadState(const CpuState_t &CpuState);
  bool SetCoverageBps();
  HRESULT MapGpaRange(const uint8_t *Hva, const Gpa_t Gpa,
                      const uint64_t RangeSize,
                      const WHV_MAP_GPA_RANGE_FLAGS Flags);
  HRESULT PopulateMemory(const Options_t &Opts);

  HRESULT GetRegisters(const WHV_REGISTER_NAME *Names,
                       WHV_REGISTER_VALUE *Values, const uint32_t Numb) const;
  HRESULT GetRegister(const WHV_REGISTER_NAME Name,
                      WHV_REGISTER_VALUE *Value) const;
  uint64_t GetReg64(const WHV_REGISTER_NAME Name) const;
  HRESULT SetRegisters(const WHV_REGISTER_NAME *Names,
                       const WHV_REGISTER_VALUE *Values, const uint32_t Numb);
  HRESULT SetRegister(const WHV_REGISTER_NAME Name,
                      const WHV_REGISTER_VALUE *Value);
  HRESULT SetReg64(const WHV_REGISTER_NAME Name, const uint64_t Value);

  HRESULT
  SlowTranslateGva(const Gva_t Gva, const WHV_TRANSLATE_GVA_FLAGS Flags,
                   WHV_TRANSLATE_GVA_RESULT &TranslationResult, Gpa_t &Gpa);
  HRESULT
  TranslateGva(const Gva_t Gva, const WHV_TRANSLATE_GVA_FLAGS Flags,
               WHV_TRANSLATE_GVA_RESULT &TranslationResult, Gpa_t &Gpa) const;

  bool PhysRead(const Gpa_t Gpa, uint8_t *Buffer,
                const uint64_t BufferSize) const;
  uint64_t PhysRead8(const Gpa_t Gpa) const;

  HRESULT RunProcessor(WHV_RUN_VP_EXIT_CONTEXT &ExitContext);

  Gva_t GetFirstVirtualPageToFault(const Gva_t Gva, const uint64_t Size);

  const uint8_t *GetTestcaseBuffer();
  const uint64_t GetTestcaseSize();

public:
  void CancelRunVirtualProcessor();
};

#endif