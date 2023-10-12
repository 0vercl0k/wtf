// Axel '0vercl0k' Souchet - June 14 2020
#pragma once
#include "backend.h"
#include "platform.h"

#ifdef LINUX
#include "tsl/robin_map.h"
#include "tsl/robin_set.h"
#include <array>
#include <linux/kvm.h>
#include <memory>
#include <signal.h>
#include <thread>

//
// Stolen from bitmap.h / bitops.h.
//

#define DIV_ROUND_UP(n, d) (((n) + (d)-1) / (d))
#define BITS_PER_BYTE 8
#define BITS_PER_TYPE(type) (sizeof(type) * BITS_PER_BYTE)
#define BITS_TO_LONGS(nr) DIV_ROUND_UP(nr, BITS_PER_TYPE(long))

//
// This is the run stats for the KVM backend.
//

struct KvmRunStats_t {
  uint64_t UffdPages = 0;
  uint64_t Dirty = 0;
  uint64_t Vmexits = 0;

  //
  // Only if PMU is available
  //

  uint64_t InstructionsExecuted = 0;

  void Print() {
    fmt::print("--------------------------------------------------\n");
    fmt::print("Run stats:\n");
    const uint64_t DirtyMemoryBytes = Dirty * Page::Size;
    const uint64_t DirtyMemoryMb = Dirty / Page::Size;
    fmt::print("          Dirty pages: {} bytes, {} pages, {} MB\n",
               DirtyMemoryBytes, Dirty, DirtyMemoryMb);
    const uint64_t UffdPagesBytes = UffdPages * Page::Size;
    const uint64_t UffdPagesMb = UffdPages / Page::Size;
    fmt::print("            UffdPages: {} bytes, {} pages, {} MB\n",
               UffdPagesBytes, UffdPages, UffdPagesMb);
    fmt::print("              VMExits: {}\n", Vmexits);
    if (InstructionsExecuted > 0) {
      fmt::print("Instructions executed: {}\n", InstructionsExecuted);
    }
  }

  void Reset() {
    UffdPages = 0;
    Dirty = 0;
    Vmexits = 0;
    InstructionsExecuted = 0;
  }
};

//
// A breakpoint is basically a Gpa and a handler.
//

struct KvmBreakpoint_t {
  Gpa_t Gpa;
  BreakpointHandler_t Handler;

  KvmBreakpoint_t(const Gpa_t Gpa_, const BreakpointHandler_t Handler_)
      : Gpa(Gpa_), Handler(Handler_) {}
};

//
// A KVM memory region can be seen as a stick of RAM memory. Imagine having a
// slot of 2GB of contiguous RAM in your VM.
//

struct KvmMemoryRegion_t {
  struct kvm_userspace_memory_region Kvm;
  std::unique_ptr<uint64_t[]> DirtyBitmap;
  uint64_t DirtyBitmapSizeBits;
  uint64_t DirtyBitmapSizeQwords;
  uint64_t Pages;

  KvmMemoryRegion_t()
      : DirtyBitmap(nullptr), DirtyBitmapSizeBits(0), DirtyBitmapSizeQwords(0),
        Pages(0) {
    memset(&Kvm, 0, sizeof(Kvm));
  }

  void Initialize(const struct kvm_userspace_memory_region &Region) {
    Kvm = Region;

    //
    // Creates the dirty bitmap. BITS_TO_LONGS ensures that the result is
    // rounded up to the closest multiple of 64.
    //

    Pages = Kvm.memory_size / Page::Size;
    DirtyBitmapSizeQwords = BITS_TO_LONGS(Pages);
    DirtyBitmapSizeBits = DirtyBitmapSizeQwords * 64;
    DirtyBitmap = std::make_unique<uint64_t[]>(DirtyBitmapSizeQwords);
  }
};

class KvmBackend_t : public Backend_t {
private:
  //
  // This is the RAM.
  //

  Ram_t Ram_;

  //
  // Breakpoints. This maps a GVA to a breakpoint.
  //

  tsl::robin_map<Gva_t, KvmBreakpoint_t> Breakpoints_;

  //
  // This keeps track of every code coverage breakpoints. This maps a GVA to a
  // GPA.
  //

  tsl::robin_map<Gva_t, Gpa_t> CovBreakpoints_;

  //
  // This is a list of every basic block GVA we have hit so far; basically the
  // aggregated coverage.
  //

  tsl::robin_set<Gva_t> Coverage_;

  //
  // This tracks every dirty GPA.
  //

  tsl::robin_set<Gpa_t> DirtyGpas_;

  //
  // This is a seed we use to implement our deterministic RDRAND. The seed needs
  // to get restored for every test cases.
  //

  uint64_t Seed_ = 0;

  //
  // This is the file descriptor to the KVM device.
  //

  int Kvm_ = -1;

  //
  // This is the file descriptor to our VM.
  //

  int Vm_ = -1;

  //
  // This is the file descriptor to our VP.
  //

  int Vp_ = -1;

  //
  // This is a mmap memory region that is shared between KVM and usermode where
  // state is stored, like some registers. This means we can access some of the
  // state without calling into the kernel.
  //
  struct kvm_run *Run_ = nullptr;

  //
  // This is the size of the Run region above.
  //

  uint64_t VpMmapSize_ = 0;

  //
  // This is user-mode fault file descriptor we use to implement demand paging.
  //

  int Uffd_ = -1;

  //
  // This is a boolean that we turn on when we want to stop the UFFD thread.
  //

  bool UffdThreadStop_ = false;

  //
  // This is the UFFD thread. This thread handles faults when the guest code
  // accesses memory that haven't been mapped in yet. The thread populates the
  // memory region based on the crash-dump.
  //

  std::thread UffdThread_;

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
  // This is a boolean we use to be able to stop running a testcase. It is
  // useful for breakpoints for example to be able to stop the main execution
  // loop.
  //

  bool Stop_ = false;

  //
  // This stores stats for every test-case run.
  //

  KvmRunStats_t RunStats_;

  //
  // This is the path where we find code coverage files.
  //

  fs::path CoveragePath_;

  //
  // This is the time limit we use to protect ourselves against infinite loops /
  // long test cases.
  // If we have PMU available this is a number of instructions, otherwise this
  // is an amount of seconds.
  //

  uint64_t Limit_ = 0;

  //
  // This is the trace file if we are tracing the current test case.
  //

  FILE *TraceFile_ = nullptr;

  //
  // This tracks if the VM supports PMU.
  //

  bool PmuAvailable_ = false;

  //
  // This is an array where we stuff MSRs to restore.
  //

  std::unique_ptr<uint8_t[]> MsrsBacking_;

  //
  // This pointer actually points into the previous buffer.
  //

  struct kvm_msrs *Msrs_ = nullptr;

  //
  // Local APIC configuration.
  //

  struct kvm_lapic_state Lapic_;

  //
  // The physical memory space of our vm. Basically we use two memory regions to
  // map:
  //   - [0 - APIC_DEFAULT_PHYS_BASE[
  //   - [APIC_DEFAULT_PHYS_BASE + 0x1000 - ... ]
  // We have to use two different regions because KVM registers a memory slot
  // that covers the IOAPIC page and memory regions cannot overlay each other.
  // So, yeah.
  //

  std::array<KvmMemoryRegion_t, 2> MemoryRegions_;

  Gpa_t LastBreakpointGpa_ = Gpa_t(0xffffffffffffffff);

public:
  //
  // Ctor / Dtor.
  //

  ~KvmBackend_t();
  KvmBackend_t() = default;
  KvmBackend_t(const KvmBackend_t &) = delete;
  KvmBackend_t &operator=(const KvmBackend_t &) = delete;

  //
  // Initialize the backend. A CPU state is provided in order for the backend to
  // be able to set-up memory accesses / translations so that the callers are
  // able to set breakpoints or read memory before starting fuzzing.
  //

  bool Initialize(const Options_t &Opts, const CpuState_t &CpuState) override;

  //
  // Run a test case.
  //

  std::optional<TestcaseResult_t> Run(const uint8_t *Buffer,
                                      const uint64_t BufferSize) override;

  //
  // Restore state.
  //

  bool Restore(const CpuState_t &CpuState) override;

  //
  // Stop the current test case from executing (can be used by breakpoints.
  //

  void Stop(const TestcaseResult_t &Res) override;

  //
  // Set a limit to avoid infinite loops test cases. The limit depends on the
  // backend for now, for example the Bochscpu backend interprets the limit as
  // an instruction limit but the WinHV backend interprets it as a time limit.
  //

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
  // Some backend collect stats for a test case run; this displays it.
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

  //
  // Dirty a GPA.
  //

  bool DirtyGpa(const Gpa_t Gpa) override;

  //
  // GVA->GPA translation.
  //

  bool VirtTranslate(const Gva_t Gva, Gpa_t &Gpa,
                     const MemoryValidate_t Validate) const override;

  //
  // GPA->HVA translation.
  //

  uint8_t *PhysTranslate(const Gpa_t Gpa) const override;

  //
  // Page faults a GVA range. This basically injects a #PF in the guest.
  //

  bool PageFaultsMemoryIfNeeded(const Gva_t Gva, const uint64_t Size) override;

  //
  // Get the last new coverage generated by the last executed testcase.
  //

  const tsl::robin_set<Gva_t> &LastNewCoverage() const override;

  bool RevokeLastNewCoverage() override;

  bool InsertCoverageEntry(const Gva_t Gva) override;

private:
  //
  // Load the CPU state into the VP.
  //

  bool LoadState(const CpuState_t &CpuState);

  //
  // Load MSRs into the VP.
  //

  bool LoadMsrs(const CpuState_t &CpuState);

  //
  // Set an MSR.
  //

  bool SetMsr(const uint32_t Msr, const uint64_t Value);

  //
  // Set a list of MSRs.
  //

  bool SetMsrs(const struct kvm_msrs *Msrs);

  //
  // Get a bunch of MSRs.
  //

  bool GetMsrs(struct kvm_msrs *Msrs);

  //
  // Get an MSR.
  //

  uint64_t GetMsr(const uint32_t Msr);

  //
  // Load the kvm_regs into the VP.
  //

  bool LoadRegs(const CpuState_t &CpuState);

  //
  // Get kvm_regs.
  //

  bool GetRegs(struct kvm_regs &Regs);

  //
  // Set kvm_regs.
  //

  bool SetRegs(const struct kvm_regs &Regs);

  //
  // Load the kvm_sregs into the VP.
  //

  bool LoadSregs(const CpuState_t &CpuState);

  //
  // Get kvm_sregs.
  //

  bool GetSregs(struct kvm_sregs &Sregs);

  //
  // Set the kvm_sregs.
  //

  bool SetSregs(const struct kvm_sregs &Sregs);

  //
  // Load the kvm_guest_debug into the VP.
  //

  bool LoadDebugRegs(const CpuState_t &CpuState);

  //
  // Set the kvm_guest_debug.
  //

  bool SetDregs(struct kvm_guest_debug &Dregs);

  //
  // Load the FPU state into the VP.
  //

  bool LoadFpu(const CpuState_t &CpuState);

  //
  // Load the XCRs into the VP.
  //

  bool LoadXcrs(const CpuState_t &CpuState);

  //
  // Load the CPUID leaves config into the VP.
  //

  bool LoadCpuid();

  //
  // Get the kvm_vcpu_events.
  //

  bool GetCpuEvents(struct kvm_vcpu_events &Events);

  //
  // Get the dirty log for a region.
  //

  bool GetDirtyLog(const KvmMemoryRegion_t &MemoryRegion);

  //
  // Clear the dirty log of a region.
  //

  bool ClearDirtyLog(const KvmMemoryRegion_t &MemoryRegion);

  //
  // Check if KVM has support for a capability.
  //

  int CheckCapability(const long Capability);

  //
  // Enable a VM capability.
  //

  bool EnableCapability(const uint32_t Capability, const uint32_t Arg);

  //
  // Populate the memory. This allocates the RAM and registers the KVM memory
  // regions.
  //

  bool PopulateMemory(const Options_t &Opts);

  //
  // This register a memory region into the VM.
  //

  bool RegisterMemory(const KvmMemoryRegion_t &MemoryRegion);

  //
  // This returns the first virtual page to fault from a memory range. This is
  // useful if we are trying to read a memory region and that we need to fault
  // the memory in ourselves (assuming it isn't). One example is malloc'ing a
  // buffer; the buffer is faulted in on access.
  //

  Gva_t GetFirstVirtualPageToFault(const Gva_t Gva, const uint64_t Size);

  //
  // Handle a VMEXIT due to a coverage breakpoint.
  //

  bool OnExitCoverageBp(const Gva_t Rip);

  //
  // Handle a KVM_EXIT_DEBUG exit.
  //

  bool OnExitDebug(struct kvm_debug_exit_arch &Debug);

  //
  // This does a virtual translation using KVM_TRANSLATE.
  //

  // bool SlowVirtTranslate(const uint64_t Gva, uint64_t &Gpa,
  //                        const MemoryValidate_t Validate);

  //
  // This reads 8 bytes off physical memory.
  //

  uint64_t PhysRead8(const Gpa_t Gpa) const;

  //
  // This reads a physical mmeory region.
  //

  bool PhysRead(const Gpa_t Gpa, uint8_t *Buffer,
                const uint64_t BufferSize) const;

  //
  // Set up demand paging. It registers UFFD over the RAM region and creates the
  // UFFD thread.
  //

  bool SetupDemandPaging();

  //
  // Insert the code coverage breakpoints in memory.
  //

  bool SetCoverageBps();

  //
  // This is the main function for the UFFD thread.
  //

  void UffdThreadMain();

  //
  // This is the static entry point for the UFFD thread.
  //

  static void StaticUffdThreadMain(KvmBackend_t *Ptr);

  //
  // This is the static entry point for handling alarm signals. This is how we
  // implement timeouts.
  //

  static void StaticSignalAlarm(int, siginfo_t *, void *);

public:
  //
  // This set the Run->immediate_exit boolean to tell the kernel to exit the
  // main loop of execution; this is how we handle timeouts.
  //

  void SignalAlarm();
};

#endif
