// Axel '0vercl0k' Souchet - June 14 2020
#include "kvm_backend.h"

#ifdef LINUX
#include "blake3.h"
#include "nt.h"
#include "utils.h"
#include <algorithm>
#include <fcntl.h>
#include <fstream>
#include <inttypes.h>
#include <linux/userfaultfd.h>
#include <memory>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

constexpr bool KvmLoggingOn = false;

template <typename... Args_t>
void KvmDebugPrint(const char *Format, const Args_t &...args) {
  if constexpr (KvmLoggingOn) {
    fmt::print("kvm: ");
    fmt::print(fmt::runtime(Format), args...);
  }
}

const uint64_t PfVector = 14;

#ifndef __NR_userfaultfd
#error Need userfaultfd support
#endif

/**
 * @brief Architectural Performance Monitoring Leaf
 *
 * When CPUID executes with EAX set to 0AH, the processor returns information
 * about support for architectural performance monitoring capabilities.
 * Architectural performance monitoring is supported if the version ID is
 * greater than Pn 0. For each version of architectural performance monitoring
 * capability, software must enumerate this leaf to discover the programming
 * facilities and the architectural performance events available in the
 * processor.
 *
 * @see Vol3C[23(Introduction to Virtual-Machine Extensions)]
 */
#define CPUID_ARCHITECTURAL_PERFORMANCE_MONITORING 0x0000000A
struct CPUID_EAX_0A_t {
  CPUID_EAX_0A_t() {
    Eax.Flags = 0;
    Ebx.Flags = 0;
    Ecx.Flags = 0;
    Edx.Flags = 0;
  }

  CPUID_EAX_0A_t(const uint32_t _Eax, const uint32_t _Ebx, const uint32_t _Ecx,
                 const uint32_t _Edx) {
    Eax.Flags = _Eax;
    Ebx.Flags = _Ebx;
    Ecx.Flags = _Ecx;
    Edx.Flags = _Edx;
  }

  bool operator==(const CPUID_EAX_0A_t &B) const {
    return Eax.Flags == B.Eax.Flags && Ebx.Flags == B.Ebx.Flags &&
           Ecx.Flags == B.Ecx.Flags && Edx.Flags == B.Edx.Flags;
  }

  union {
    struct {
      /**
       * [Bits 7:0] Version ID of architectural performance monitoring.
       */
      uint32_t VersionIdOfArchitecturalPerformanceMonitoring : 8;

      /**
       * [Bits 15:8] Number of general-purpose performance monitoring counter
       * per logical processor.
       */
      uint32_t NumberOfPerformanceMonitoringCounterPerLogicalProcessor : 8;

      /**
       * [Bits 23:16] Bit width of general-purpose, performance monitoring
       * counter.
       */
      uint32_t BitWidthOfPerformanceMonitoringCounter : 8;

      /**
       * [Bits 31:24] Length of EBX bit vector to enumerate architectural
       * performance monitoring events.
       */
      uint32_t EbxBitVectorLength : 8;
    };

    uint32_t Flags;
  } Eax;

  union {
    struct {
      /**
       * [Bit 0] Core cycle event not available if 1.
       */
      uint32_t CoreCycleEventNotAvailable : 1;

      /**
       * [Bit 1] Instruction retired event not available if 1.
       */
      uint32_t InstructionRetiredEventNotAvailable : 1;

      /**
       * [Bit 2] Reference cycles event not available if 1.
       */
      uint32_t ReferenceCyclesEventNotAvailable : 1;

      /**
       * [Bit 3] Last-level cache reference event not available if 1.
       */
      uint32_t LastLevelCacheReferenceEventNotAvailable : 1;

      /**
       * [Bit 4] Last-level cache misses event not available if 1.
       */
      uint32_t LastLevelCacheMissesEventNotAvailable : 1;

      /**
       * [Bit 5] Branch instruction retired event not available if 1.
       */
      uint32_t BranchInstructionRetiredEventNotAvailable : 1;

      /**
       * [Bit 6] Branch mispredict retired event not available if 1.
       */
      uint32_t BranchMispredictRetiredEventNotAvailable : 1;

      uint32_t Reserved1 : 25;
    };

    uint32_t Flags;
  } Ebx;

  union {
    struct {
      /**
       * [Bits 31:0] ECX is reserved.
       */
      uint32_t Reserved : 32;
    };

    uint32_t Flags;
  } Ecx;

  union {
    struct {
      /**
       * [Bits 4:0] Number of fixed-function performance counters (if Version ID
       * > 1).
       */
      uint32_t NumberOfFixedFunctionPerformanceCounters : 5;

      /**
       * [Bits 12:5] Bit width of fixed-function performance counters (if
       * Version ID > 1).
       */
      uint32_t BitWidthOfFixedFunctionPerformanceCounters : 8;

      uint32_t Reserved1 : 2;

      /**
       * [Bit 15] AnyThread deprecation.
       */
      uint32_t AnyThreadDeprecation : 1;

      uint32_t Reserved2 : 16;
    };

    uint32_t Flags;
  } Edx;
};

//
// To check if the hardware is compatible (Azure Standard_D8s_v3 /
// Standard_E4s_v3 are not):
//   $ cat /sys/module/kvm_intel/parameters/unrestricted_guest
//   Y
//   $ sudo apt install cpu-checker
//   $ kvm-ok
//   INFO: /dev/kvm exists
//   KVM acceleration can be used
//
// To slap extra swap:
//   $ dd if=/dev/zero of=/swapfile2 bs=1G count=10
//   $ chmod 600 /swapfile2
//   $ mkswap /swapfile2
//
// To add your user to the kvm group:
//   $ sudo usermod -a -G kvm `whoami`
//
// To record a perf profile (might need to start the profiler after
// the initialization of the fuzzer):
//   ## Allow access to kallsyms
//   # echo 0 > /proc/sys/kernel/kptr_restrict
//   ## Perf counters
//   $ sudo sysctl kernel.perf_event_paranoid=-1
//   $ perf record --call-graph dwarf --pid $(pidof wtf)
//   $ perf report
//

KvmBackend_t::~KvmBackend_t() {
  UffdThreadStop_ = true;
  UffdThread_.join();

  if (Vp_ != -1) {
    close(Vp_);
  }

  if (Vm_ != -1) {
    close(Vm_);
  }

  if (Kvm_ != -1) {
    close(Kvm_);
  }

  if (Uffd_ != -1) {
    close(Uffd_);
  }

  if (Run_ != nullptr) {
    munmap(Run_, VpMmapSize_);
  }
}

bool KvmBackend_t::Initialize(const Options_t &Opts,
                              const CpuState_t &CpuState) {
  if ((CpuState.ApicBase & 0xfffffffffffff000) != APIC_DEFAULT_PHYS_BASE) {
    fmt::print("We assume that the APIC_BASE is at {:#x}, so bailing.\n",
               APIC_DEFAULT_PHYS_BASE);
    return false;
  }

  CoveragePath_ = Opts.CoveragePath;

  //
  // Open the KVM device.
  //

  Kvm_ = open("/dev/kvm", O_RDWR | O_CLOEXEC);
  if (Kvm_ < 0) {
    perror("Could not open the kvm device");
    return false;
  }

  const int SyncRegs =
      KVM_SYNC_X86_REGS | KVM_SYNC_X86_SREGS | KVM_SYNC_X86_EVENTS;
  if (CheckCapability(KVM_CAP_SYNC_REGS) != SyncRegs) {
    return false;
  }

  //
  // Ensure we have a stable API.
  //

  if (ioctl(Kvm_, KVM_GET_API_VERSION, 0) != KVM_API_VERSION) {
    perror("No stable API");
    return false;
  }

  //
  // Create the VM.
  //

  Vm_ = ioctl(Kvm_, KVM_CREATE_VM, 0);
  if (Vm_ < 0) {
    perror("Could not create the VM");
    return false;
  }

  //
  // Create the IRQCHIP. This is needed to get the PMI when PMU is
  // used but on top of that it seems to give a ~2x speed-up.
  //

  if (ioctl(Vm_, KVM_CREATE_IRQCHIP, 0) < 0) {
    perror("KVM_CREATE_IRQCHIP");
    return false;
  }

  //
  // Create VP.
  //

  const uint32_t VpId = 0;
  Vp_ = ioctl(Vm_, KVM_CREATE_VCPU, VpId);
  if (Vp_ < 0) {
    perror("Could not create the VP");
    return false;
  }

  //
  // Get the size of the shared kvm run structure.
  //

  VpMmapSize_ = ioctl(Kvm_, KVM_GET_VCPU_MMAP_SIZE, 0);
  if (VpMmapSize_ < 0) {
    perror("Could not get the size of the shared memory region.");
    return false;
  }

  //
  // Man says:
  //   there is an implicit parameter block that can be obtained by mmap()'ing
  //   the vcpu fd at offset 0, with the size given by KVM_GET_VCPU_MMAP_SIZE.
  //

  Run_ = (struct kvm_run *)mmap(nullptr, VpMmapSize_, PROT_READ | PROT_WRITE,
                                MAP_SHARED, Vp_, 0);
  if (Run_ == nullptr) {
    perror("mmap VCPU_MMAP_SIZE");
    return false;
  }

  //
  // Ensure we have the capabilities we need:
  //   - KVM_CAP_MANUAL_DIRTY_LOG_PROTECT2,
  //   - KVM_CAP_IMMEDIATE_EXIT.
  //

  const int DirtyLogCaps = CheckCapability(KVM_CAP_MANUAL_DIRTY_LOG_PROTECT2);
  if (DirtyLogCaps < 0) {
    perror("No KVM_CAP_MANUAL_DIRTY_LOG_PROTECT2 support");
    return false;
  }

  if (CheckCapability(KVM_CAP_IMMEDIATE_EXIT) != 1) {
    fmt::print("No support for KVM_CAP_IMMEDIATE_EXIT, bailing.\n");
    return false;
  }

  //
  // Turn on KVM_CAP_MANUAL_DIRTY_LOG_PROTECT2.
  //

#define KVM_DIRTY_LOG_MANUAL_PROTECT_ENABLE (1 << 0)
  if ((DirtyLogCaps & KVM_DIRTY_LOG_MANUAL_PROTECT_ENABLE) == 0) {
    fmt::print("KVM_DIRTY_LOG_MANUAL_PROTECT_ENABLE not available, bailing.\n");
    return false;
  }

  if (!EnableCapability(KVM_CAP_MANUAL_DIRTY_LOG_PROTECT2,
                        KVM_DIRTY_LOG_MANUAL_PROTECT_ENABLE)) {
    perror("KVM_CAP_MANUAL_DIRTY_LOG_PROTECT2");
    return false;
  }

  //
  // Initialize the sync registers.
  //

  if (!GetRegs(Run_->s.regs.regs)) {
    return false;
  }

  if (!GetSregs(Run_->s.regs.sregs)) {
    return false;
  }

  if (!GetCpuEvents(Run_->s.regs.events)) {
    return false;
  }

  //
  // Load the CPUID leaves.
  //

  if (!LoadCpuid()) {
    perror("LoadCpuid");
    return false;
  }

  //
  // Initialize the local APIC.
  //

  if (ioctl(Vp_, KVM_GET_LAPIC, &Lapic_) < 0) {
    perror("KVM_GET_LAPIC");
    return false;
  }

  //
  // This means that the PMI should be delivered as an interruption
  // on the vector 0xfe (hal!HalPerfInterrupt).
  //

  union ApicLVTRegister_t {
    struct {
      uint32_t Vector : 8;
      uint32_t DeliveryMode : 3;
      uint32_t Reserved : 1;
      uint32_t DeliveryStatus : 1;
      uint32_t Reserved2 : 3;
      uint32_t Mask : 1;
      uint32_t Reserved3 : 15;
    };
    uint32_t Flags;
    ApicLVTRegister_t() : Flags(0) {}
  };
  ApicLVTRegister_t ApicLVTPerfMonCountersRegister;

  ApicLVTPerfMonCountersRegister.DeliveryMode = APIC_MODE_FIXED;
  ApicLVTPerfMonCountersRegister.Vector = 0xFE;
  *(uint32_t *)(Lapic_.regs + APIC_LVTPC) =
      ApicLVTPerfMonCountersRegister.Flags;

  //
  // XXX: Supposedely turn on the local APIC? I'm not entirely sure why it is
  // needed.
  //

  union ApicSpuriousInterruptVectorRegister_t {
    struct {
      uint32_t Vector : 8;
      uint32_t ApicEnabled : 1;
    };
    uint32_t Flags;
    ApicSpuriousInterruptVectorRegister_t() : Flags(0) {}
  } ApicSpuriousInterruptVectorRegister;

  ApicSpuriousInterruptVectorRegister.ApicEnabled = 1;
  *(uint32_t *)(Lapic_.regs + APIC_SPIV) =
      ApicSpuriousInterruptVectorRegister.Flags;

  //
  // Configure the VM.
  //

  if (!LoadState(CpuState)) {
    return false;
  }

  //
  // Register our RAM with a memory slot.
  //

  if (!PopulateMemory(Opts)) {
    return false;
  }

  //
  // We need to flush the registers into the VP the first time.
  //

  if (!SetRegs(Run_->s.regs.regs)) {
    return false;
  }

  if (!SetSregs(Run_->s.regs.sregs)) {
    return false;
  }

  Run_->kvm_dirty_regs = 0;

  if (!PmuAvailable_) {

    //
    // Set up the alarm handler if we cannot use the PMU to implement a precise
    // timeout.
    //

    struct sigaction Sig;
    Sig.sa_sigaction = StaticSignalAlarm;
    Sig.sa_flags = SA_SIGINFO;

    if (sigaction(SIGALRM, &Sig, nullptr) != 0) {
      perror("sigaction SIGALRM");
      return false;
    }

    //
    // Prepares a sigmask to block SIGALARM from every threads (it'll get
    // inherited). At initialization phase, no other threads have been spawned
    // so we can be sure this mask will get inherited during the creation of the
    // threads.
    //
    // We'll unblock the signal only for the VCPU thread the first time Run() is
    // called.
    //

    sigset_t Sigset;
    if (pthread_sigmask(SIG_BLOCK, nullptr, &Sigset) < 0) {
      perror("pthread_sigmask SIG_BLOCK");
      return false;
    }

    if (sigaddset(&Sigset, SIGALRM) < 0) {
      perror("sigaddset SIGALRM");
      return false;
    }

    if (pthread_sigmask(SIG_BLOCK, &Sigset, nullptr) < 0) {
      perror("pthread_sigmask SIG_BLOCK2");
      return false;
    }
  }

  //
  // Set up demand paging with using userfaultfd.
  //

  if (!SetupDemandPaging()) {
    perror("SetupDemandPaging");
    return false;
  }

  //
  // Set the coverage breakpoints. Note that we
  // need to do that after having set-up demand paging.
  //

  if (!SetCoverageBps()) {
    fmt::print("Failed to SetCoverageBps\n");
    return false;
  }

  return true;
}

//
// A good start to debug KVM_SET_REGS issues is to start by: __kvm_set_msr /
// vmx_set_msr For PMU MSRS: hand-off happens in kvm_set_msr_common;
// intel_is_valid_msr / intel_pmu_set_msr / intel_pmu_get_msr (intel_pmu_ops /
// pmu_intel.c)
//

bool KvmBackend_t::LoadMsrs(const CpuState_t &CpuState) {
  if (MsrsBacking_ == nullptr) {
    //
    // This is the first time, so we need to initialize the data structure.
    //

    //
    // First step is to get the list of supported MSRs.
    //

    struct kvm_msr_list MsrList_ = {.nmsrs = 0};

    if (ioctl(Kvm_, KVM_GET_MSR_INDEX_LIST, &MsrList_) >= 0 || errno != E2BIG) {
      perror("KVM_GET_MSR_INDEX_LIST 1");
      return false;
    }

    const uint64_t MsrListSize = sizeof(struct kvm_msr_list) +
                                 (MsrList_.nmsrs * sizeof(MsrList_.indices[0]));
    auto MsrListBacking = std::make_unique<uint8_t[]>(MsrListSize);
    struct kvm_msr_list *MsrList = (struct kvm_msr_list *)MsrListBacking.get();
    MsrList->nmsrs = MsrList_.nmsrs;

    if (ioctl(Kvm_, KVM_GET_MSR_INDEX_LIST, MsrList) < 0) {
      perror("KVM_GET_MSR_INDEX_LIST 2");
      return false;
    }

    //
    // Once we have the list, we want to grab the MSRs themselves.
    //

    const uint64_t AllMsrsSize =
        sizeof(struct kvm_msrs) +
        (MsrList->nmsrs * sizeof(struct kvm_msr_entry));
    auto AllMsrsBacking = std::make_unique<uint8_t[]>(AllMsrsSize);
    struct kvm_msrs *AllMsrs = (struct kvm_msrs *)AllMsrsBacking.get();

    for (uint64_t MsrIdx = 0; MsrIdx < AllMsrs->nmsrs; MsrIdx++) {
      AllMsrs->entries[MsrIdx].index = MsrList->indices[MsrIdx];
    }

    if (!GetMsrs(AllMsrs)) {
      perror("GetMsrs");
      return false;
    }

    //
    // Build the array of MSRs.
    //

    using MsrEntry_t = std::pair<uint32_t, uint64_t>;
    std::vector<MsrEntry_t> Entries;

    for (uint64_t MsrIdx = 0; MsrIdx < AllMsrs->nmsrs; MsrIdx++) {
      const auto &Msr = AllMsrs->entries[MsrIdx];
      Entries.emplace_back(Msr.index, Msr.data);
    }

    //
    // Append those guys in.
    //

    const std::vector<MsrEntry_t> BaseEntries = {
        {MSR_IA32_APICBASE, CpuState.ApicBase},
        {MSR_IA32_TSC, CpuState.Tsc},
        {MSR_IA32_SYSENTER_CS, CpuState.SysenterCs},
        {MSR_IA32_SYSENTER_ESP, CpuState.SysenterEsp},
        {MSR_IA32_SYSENTER_EIP, CpuState.SysenterEip},
        {MSR_IA32_CR_PAT, CpuState.Pat},
        {MSR_IA32_EFER, CpuState.Efer.Flags},
        {MSR_IA32_STAR, CpuState.Star},
        {MSR_IA32_LSTAR, CpuState.Lstar},
        {MSR_IA32_CSTAR, CpuState.Cstar},
        {MSR_IA32_SFMASK, CpuState.Sfmask},
        {MSR_IA32_KERNEL_GS_BASE, CpuState.KernelGsBase},
        {MSR_IA32_TSC_AUX, CpuState.TscAux},
    };

    Entries.insert(Entries.end(), BaseEntries.cbegin(), BaseEntries.cend());

    if (PmuAvailable_) {

      //
      // We want to configure FIXED_CTR0 - this counter counts INST_RETIRED.ANY.
      //   This event counts the number of instructions that retire execution.
      //   For instructions that consist of multiple uops, this event counts the
      //   retirement of the last uop of the instruction. The counter continues
      //   counting during hardware interrupts, traps, and in-side interrupt
      //   handlers.)
      //
      // Note that the order of operations matter here; we first need to disable
      // the counters, then initialize them and finally turn them back on. Code
      // is in intel_pmu_set_msr. Overflows of counter is handled in
      // kvm_perf_overflow_intr.
      //

      //
      // First step is to reset the counters.
      //   IA32_PERF_GLOBAL_STATUS MSR provides single-bit status for software
      //   to query the overflow condition of each performance counter.
      //

      Entries.emplace_back(MSR_IA32_PERF_GLOBAL_STATUS, 0);
      Entries.emplace_back(MSR_IA32_PERF_GLOBAL_CTRL, 0);

      //
      // Fix the value of CTR0 to artificially create an overflow situation.
      // Code for this is in intel_pmu_set_msr.
      // See kvm_perf_overflow_intr / vcpu_enter_guest / kvm_pmu_deliver_pmi
      // for the interrupt delivery.
      //

      const uint64_t CounterMax = 1ULL << 48;
      if (Limit_ > CounterMax) {
        fmt::print(
            "The limit {:#x} is bigger than the capacity of CTR0, bailing.\n",
            Limit_);
        return false;
      }

      const uint64_t InitialValue = (Limit_ != 0) ? CounterMax - Limit_ : 0;
      Entries.emplace_back(MSR_IA32_FIXED_CTR0, InitialValue);

      //
      // To turn it on we follow the intel manuals:
      //   Three of the architectural performance events are counted using three
      //   fixed-function MSRs (IA32_FIXED_CTR0 through IA32_FIXED_CTR2). Each
      //   of the fixed-function PMC can count only one architectural
      //   performance event. Configuring the fixed-function PMCs is done by
      //   writing to bit fields in the MSR (IA32_FIXED_CTR_CTRL).
      //
      // Each fixed counters gets 4 bits; 0b1011 counts event when cpl >= 0, the
      // top bit turns on a PMI interrupt on overflow of the counter.
      //

      Entries.emplace_back(MSR_IA32_FIXED_CTR_CTRL, 0b1011);

      //
      // From the manuals:
      //   IA32_PERF_GLOBAL_CTRL MSR provides single-bit controls to enable
      //   counting of each performance counter. Each enable bit in
      //   IA32_PERF_GLOBAL_CTRL is AND'ed with the enable bits for all
      //   privilege levels in the respective IA32_PERFEVTSELx or
      //   IA32_PERF_FIXED_CTR_CTRL MSRs to start/stop the counting of
      //   respective counters. Counting is enabled if the AND'ed results is
      //   true; counting is disabled when the result is false.
      //
      // To satisfy the above we turn on the bit for CTR0.
      //

      IA32_PERF_GLOBAL_CTRL_REGISTER_t GlobalControl;
      GlobalControl.EnFixedCtrn = 0b1;
      Entries.emplace_back(MSR_IA32_PERF_GLOBAL_CTRL, GlobalControl.Flags);
    }

    //
    // Allocate and initialize the structure once and for all.
    //

    const uint64_t EntriesSize = Entries.size() * sizeof(struct kvm_msr_entry);
    const uint64_t MsrsSize = sizeof(struct kvm_msrs) + EntriesSize;
    MsrsBacking_ = std::make_unique<uint8_t[]>(MsrsSize);
    Msrs_ = (struct kvm_msrs *)MsrsBacking_.get();
    Msrs_->nmsrs = Entries.size();
    for (uint64_t MsrIdx = 0; MsrIdx < Entries.size(); MsrIdx++) {
      Msrs_->entries[MsrIdx].index = Entries[MsrIdx].first;
      Msrs_->entries[MsrIdx].data = Entries[MsrIdx].second;
    }
  }

  //
  // Set the MSRs registers.
  //

  if (!SetMsrs(Msrs_)) {
    return false;
  }

  return true;
}

bool KvmBackend_t::SetMsrs(const struct kvm_msrs *Msrs) {
  const int Ret = ioctl(Vp_, KVM_SET_MSRS, Msrs);
  if (Ret < 0) {
    perror("KVM_SET_MSRS");
    return false;
  }

  if (Ret != Msrs->nmsrs) {
    fmt::print("KVM_SET_MSRS set {} registers off the {} provided\n", Ret,
               Msrs->nmsrs);
    return false;
  }

  return true;
}

bool KvmBackend_t::SetMsr(const uint32_t Msr, const uint64_t Value) {
  const uint64_t MsrsSize =
      sizeof(struct kvm_msrs) + sizeof(struct kvm_msr_entry);

  std::array<uint8_t, MsrsSize> _Msrs;
  struct kvm_msrs *Msrs = (struct kvm_msrs *)_Msrs.data();

  Msrs->nmsrs = 1;
  Msrs->entries[0].index = Msr;
  Msrs->entries[0].data = Value;
  return SetMsrs(Msrs);
}

bool KvmBackend_t::GetMsrs(struct kvm_msrs *Msrs) {
  const int Ret = ioctl(Vp_, KVM_GET_MSRS, Msrs);
  if (Ret < 0) {
    perror("KVM_GET_MSRS");
    return false;
  }

  if (Ret != Msrs->nmsrs) {
    fmt::print("KVM_GET_MSRS set {} registers off the {} provided\n", Ret,
               Msrs->nmsrs);
    return false;
  }

  return true;
}

uint64_t KvmBackend_t::GetMsr(const uint32_t Msr) {
  const uint64_t MsrsSize =
      sizeof(struct kvm_msrs) + sizeof(struct kvm_msr_entry);

  std::array<uint8_t, MsrsSize> _Msrs;
  struct kvm_msrs *Msrs = (struct kvm_msrs *)_Msrs.data();

  Msrs->nmsrs = 1;
  Msrs->entries[0].index = Msr;
  Msrs->entries[0].data = 0;

  if (!GetMsrs(Msrs)) {
    __debugbreak();
  }

  return Msrs->entries[0].data;
}

bool KvmBackend_t::GetRegs(struct kvm_regs &Regs) {
  if (ioctl(Vp_, KVM_GET_REGS, &Regs) < 0) {
    perror("KVM_GET_REGS failed");
    return false;
  }

  return true;
}

bool KvmBackend_t::SetRegs(const struct kvm_regs &Regs) {
  if (ioctl(Vp_, KVM_SET_REGS, &Regs) < 0) {
    perror("KVM_SET_REGS failed");
    return false;
  }

  return true;
}

bool KvmBackend_t::SetDregs(struct kvm_guest_debug &Dregs) {
  if (ioctl(Vp_, KVM_SET_GUEST_DEBUG, &Dregs) < 0) {
    perror("KVM_SET_GUEST_DEBUG failed");
    return false;
  }

  return true;
}

bool KvmBackend_t::LoadRegs(const CpuState_t &CpuState) {
  //
  // Set the GPRs.
  //

  Run_->s.regs.regs = {.rax = CpuState.Rax,
                       .rbx = CpuState.Rbx,
                       .rcx = CpuState.Rcx,
                       .rdx = CpuState.Rdx,
                       .rsi = CpuState.Rsi,
                       .rdi = CpuState.Rdi,
                       .rsp = CpuState.Rsp,
                       .rbp = CpuState.Rbp,
                       .r8 = CpuState.R8,
                       .r9 = CpuState.R9,
                       .r10 = CpuState.R10,
                       .r11 = CpuState.R11,
                       .r12 = CpuState.R12,
                       .r13 = CpuState.R13,
                       .r14 = CpuState.R14,
                       .r15 = CpuState.R15,
                       .rip = CpuState.Rip,
                       .rflags = CpuState.Rflags};

  Run_->kvm_dirty_regs |= KVM_SYNC_X86_REGS;
  return true;
}

bool KvmBackend_t::LoadSregs(const CpuState_t &CpuState) {
#define SEG(Name, WtfName)                                                     \
  {                                                                            \
    Run_->s.regs.sregs.Name.base = CpuState.WtfName.Base;                      \
    Run_->s.regs.sregs.Name.limit = CpuState.WtfName.Limit;                    \
    Run_->s.regs.sregs.Name.selector = CpuState.WtfName.Selector;              \
    Run_->s.regs.sregs.Name.type = CpuState.WtfName.SegmentType;               \
    Run_->s.regs.sregs.Name.s = CpuState.WtfName.NonSystemSegment;             \
    Run_->s.regs.sregs.Name.dpl = CpuState.WtfName.DescriptorPrivilegeLevel;   \
    Run_->s.regs.sregs.Name.present = CpuState.WtfName.Present;                \
    Run_->s.regs.sregs.Name.avl = CpuState.WtfName.Available;                  \
    Run_->s.regs.sregs.Name.l = CpuState.WtfName.Long;                         \
    Run_->s.regs.sregs.Name.db = CpuState.WtfName.Default;                     \
    Run_->s.regs.sregs.Name.g = CpuState.WtfName.Granularity;                  \
  }

  //
  // 3.4.2.1 Segment Registers in 64-Bit Mode
  // In 64-bit mode: CS, DS, ES, SS are treated as if each segment base is 0,
  // regardless of the value of the associated segment descriptor base. This
  // creates a flat address space for code, data, and stack. FS and GS are
  // exceptions. Both segment registers may be used as additional base registers
  // in linear address calculations (in the addressing of local data and certain
  // operating system data structures). Even though segmentation is generally
  // disabled, segment register loads may cause the processor to perform segment
  // access assists. During these activities, enabled processors will still
  // perform most of the legacy checks on loaded values (even if the checks are
  // not applicable in 64-bit mode). Such checks are needed because a segment
  // register loaded in 64-bit mode may be used by an application running in
  // compatibility mode. Limit checks for CS, DS, ES, SS, FS, and GS are
  // disabled in 64-bit mode
  //

  Run_->s.regs.sregs.cr0 = CpuState.Cr0.Flags;
  Run_->s.regs.sregs.cr2 = CpuState.Cr2;
  Run_->s.regs.sregs.cr3 = CpuState.Cr3;
  Run_->s.regs.sregs.cr4 = CpuState.Cr4.Flags;
  Run_->s.regs.sregs.cr8 = CpuState.Cr8;
  Run_->s.regs.sregs.efer = CpuState.Efer.Flags;
  Run_->s.regs.sregs.apic_base = CpuState.ApicBase;

  SEG(cs, Cs);
  SEG(ss, Ss);
  SEG(es, Es);
  SEG(ds, Ds);
  SEG(fs, Fs);
  SEG(gs, Gs);
  SEG(tr, Tr);
  SEG(ldt, Ldtr);

#undef SEG
#define GLOBALSEG(Name, WtfName)                                               \
  {                                                                            \
    Run_->s.regs.sregs.Name.base = CpuState.WtfName.Base;                      \
    Run_->s.regs.sregs.Name.limit = CpuState.WtfName.Limit;                    \
  }

  GLOBALSEG(gdt, Gdtr);
  GLOBALSEG(idt, Idtr);

#undef GLOBALSEG

  Run_->kvm_dirty_regs |= KVM_SYNC_X86_SREGS;
  return true;
}

bool KvmBackend_t::GetSregs(struct kvm_sregs &Sregs) {
  if (ioctl(Vp_, KVM_GET_SREGS, &Sregs) < 0) {
    perror("KVM_GET_SREGS");
    return false;
  }

  return true;
}

bool KvmBackend_t::SetSregs(const struct kvm_sregs &Sregs) {
  if (ioctl(Vp_, KVM_SET_SREGS, &Sregs) < 0) {
    perror("KVM_SET_SREGS");
    return false;
  }

  return true;
}

bool KvmBackend_t::LoadDebugRegs(const CpuState_t &CpuState) {
  //
  // Set the Debug registers.
  //

  struct kvm_guest_debug Dregs;
  memset(&Dregs, 0, sizeof(Dregs));

#define KVM_GUESTDBG_ENABLE 0x00000001
#define KVM_GUESTDBG_SINGLESTEP 0x00000002
#define KVM_GUESTDBG_USE_SW_BP 0x00010000

  Dregs.control = KVM_GUESTDBG_USE_SW_BP | KVM_GUESTDBG_ENABLE;
  Dregs.arch.debugreg[0] = CpuState.Dr0;
  Dregs.arch.debugreg[1] = CpuState.Dr1;
  Dregs.arch.debugreg[2] = CpuState.Dr2;
  Dregs.arch.debugreg[3] = CpuState.Dr3;
  Dregs.arch.debugreg[6] = CpuState.Dr6;
  Dregs.arch.debugreg[7] = CpuState.Dr7;

  if (!SetDregs(Dregs)) {
    return false;
  }

  return true;
}

bool KvmBackend_t::LoadFpu(const CpuState_t &CpuState) {
  //
  // Set the FPU registers.
  //

  struct kvm_fpu Fregs;
  if (ioctl(Vp_, KVM_GET_FPU, &Fregs) < 0) {
    perror("KVM_GET_FPU failed");
    return false;
  }

  for (uint64_t Idx = 0; Idx < 8; Idx++) {
    memcpy(&Fregs.fpr[Idx], &CpuState.Fpst[Idx], 16);
  }

  Fregs.fcw = CpuState.Fpcw;
  Fregs.fsw = CpuState.Fpsw;
  // Fregs.ftwx = ??
  Fregs.last_opcode = CpuState.Fpop;
  // Fregs.last_ip = ??
  // Fregs.last_dp = ??
  Fregs.mxcsr = CpuState.Mxcsr;
  for (uint64_t Idx = 0; Idx < 16; Idx++) {
    memcpy(Fregs.xmm[Idx], &CpuState.Zmm[Idx].Q[0], 16);
  }

  if (ioctl(Vp_, KVM_SET_FPU, &Fregs) < 0) {
    perror("KVM_SET_FPU failed");
    return false;
  }

  return true;
}

bool KvmBackend_t::LoadXcrs(const CpuState_t &CpuState) {
  //
  // Set the XCRs registers.
  //

  struct kvm_xcrs Xregs;
  memset(&Xregs, 0, sizeof(Xregs));

  Xregs.nr_xcrs = 1;
  Xregs.flags = 0;
  Xregs.xcrs[0].xcr = 0;
  Xregs.xcrs[0].value = CpuState.Xcr0;

  if (ioctl(Vp_, KVM_SET_XCRS, &Xregs) < 0) {
    perror("KVM_SET_XCRS failed");
    return false;
  }

  return true;
}

bool KvmBackend_t::LoadCpuid() {
  //
  // Load up CPUID leaves.
  //

#define KVM_MAX_CPUID_ENTRIES 80
  const uint64_t CpuidSize =
      sizeof(struct kvm_cpuid2) +
      (KVM_MAX_CPUID_ENTRIES * sizeof(struct kvm_cpuid_entry2));
  std::array<uint8_t, CpuidSize> _Cpuid = {};
  struct kvm_cpuid2 *Cpuid = (struct kvm_cpuid2 *)_Cpuid.data();

  Cpuid->nent = KVM_MAX_CPUID_ENTRIES;
  if (ioctl(Kvm_, KVM_GET_SUPPORTED_CPUID, Cpuid) < 0) {
    perror("KVM_GET_SUPPORTED_CPUID failed");
    return false;
  }

  //
  // Walk the leaves to see if we have PMU support.
  //

  for (uint64_t Idx = 0; Idx < Cpuid->nent; Idx++) {
    struct kvm_cpuid_entry2 &Entry = Cpuid->entries[Idx];
    switch (Entry.function) {
    case CPUID_ARCHITECTURAL_PERFORMANCE_MONITORING: {
      CPUID_EAX_0A_t Perf(Entry.eax, Entry.ebx, Entry.ecx, Entry.edx);
      if (Perf.Eax.VersionIdOfArchitecturalPerformanceMonitoring >= 2) {
        const uint32_t NumberOfFixedFunctionPerformanceCounters =
            Perf.Edx.NumberOfFixedFunctionPerformanceCounters;
        const uint32_t BitWidthOfFixedFunctionPerformanceCounters =
            Perf.Edx.BitWidthOfFixedFunctionPerformanceCounters;
        fmt::print(
            "PMU Version 2 is available ({} fixed counters of {} bits)\n",
            NumberOfFixedFunctionPerformanceCounters,
            BitWidthOfFixedFunctionPerformanceCounters);

        if (NumberOfFixedFunctionPerformanceCounters != 3 ||
            BitWidthOfFixedFunctionPerformanceCounters != 48) {
          fmt::print("Weird PMU, bailing.\n");
          return false;
        }

        PmuAvailable_ = true;
      }
      break;
    }
    }
  }

  if (ioctl(Vp_, KVM_SET_CPUID2, Cpuid) < 0) {
    perror("KVM_SET_CPUID2 failed");
    return false;
  }

  return true;
}

bool KvmBackend_t::GetCpuEvents(struct kvm_vcpu_events &Events) {
  if (ioctl(Vp_, KVM_GET_VCPU_EVENTS, &Events) < 0) {
    perror("KVM_GET_VCPU_EVENTS");
    return false;
  }

  return true;
}

bool KvmBackend_t::GetDirtyLog(const KvmMemoryRegion_t &MemoryRegion) {
  const struct kvm_dirty_log DirtyLog = {.slot = MemoryRegion.Kvm.slot,
                                         .dirty_bitmap =
                                             MemoryRegion.DirtyBitmap.get()};

  //
  // Get the dirty bitmap.
  //

  if (ioctl(Vm_, KVM_GET_DIRTY_LOG, &DirtyLog) < 0) {
    perror("KVM_GET_DIRTY_LOG");
    return false;
  }

  return true;
}

bool KvmBackend_t::ClearDirtyLog(const KvmMemoryRegion_t &MemoryRegion) {
  struct kvm_clear_dirty_log ClearDirtyLog = {
      .slot = MemoryRegion.Kvm.slot,
      .num_pages = uint32_t(MemoryRegion.Pages),
      .first_page = 0,
      .dirty_bitmap = MemoryRegion.DirtyBitmap.get(),
  };

  //
  // Clear the dirty bitmap.
  //

  if (ioctl(Vm_, KVM_CLEAR_DIRTY_LOG, &ClearDirtyLog) < 0) {
    perror("KVM_CLEAR_DIRTY_LOG");
    return false;
  }

  return true;
}

int KvmBackend_t::CheckCapability(const long Capability) {
  const int Ret = ioctl(Kvm_, KVM_CHECK_EXTENSION, Capability);
  if (Ret == -1) {
    perror("KVM_CHECK_EXTENSION");
    return -1;
  }

  return Ret;
}

bool KvmBackend_t::EnableCapability(const uint32_t Capability,
                                    const uint32_t Arg) {
  const struct kvm_enable_cap Cap = {.cap = Capability, .args = {Arg}};

  if (ioctl(Vm_, KVM_ENABLE_CAP, &Cap) < 0) {
    perror("KVM_ENABLE_CAP");
    return false;
  }

  return true;
}

bool KvmBackend_t::LoadState(const CpuState_t &CpuState) {
  memset(Run_, 0, sizeof(*Run_));
  Seed_ = CpuState.Seed;

  if (!LoadRegs(CpuState)) {
    perror("LoadRegs");
    return false;
  }

  if (!LoadSregs(CpuState)) {
    perror("LoadSregs");
    return false;
  }

  if (!LoadFpu(CpuState)) {
    perror("LoadFpu");
    return false;
  }

  if (!LoadMsrs(CpuState)) {
    perror("LoadMsrs");
    return false;
  }

  if (!LoadXcrs(CpuState)) {
    perror("LoadXcrs");
    return false;
  }

  if (!LoadDebugRegs(CpuState)) {
    perror("LoadDebugRegs");
    return false;
  }

  memset(&Run_->s.regs.events.exception, 0,
         sizeof(Run_->s.regs.events.exception));

  if (ioctl(Vp_, KVM_SET_LAPIC, &Lapic_) < 0) {
    perror("KVM_SET_LAPIC");
    return false;
  }

  return true;
}

bool KvmBackend_t::PopulateMemory(const Options_t &Opts) {

  //
  // Populate the guest ram using the crash-dump.
  //

  if (!Ram_.Populate(Opts.DumpPath)) {
    perror("Could not allocate RAM");
    return false;
  }

  //
  // We first register a slot for [0 - APIC_DEFAULT_PHYS_BASE[
  // The IOAPIC page is allocated in alloc_apic_access_page with a private
  // slot (APIC_ACCESS_PAGE_PRIVATE_MEMSLOT).
  //

  const struct kvm_userspace_memory_region First = {
      .slot = 0,
      .flags = KVM_MEM_LOG_DIRTY_PAGES,
      .guest_phys_addr = 0,
      .memory_size = APIC_DEFAULT_PHYS_BASE,
      .userspace_addr = uint64_t(Ram_.Hva()),
  };

  MemoryRegions_[0].Initialize(First);
  if (!RegisterMemory(MemoryRegions_[0])) {
    perror("Cannot register first part of RAM");
    return false;
  }

  //
  // Skip over the APIC page.
  //

  const uint64_t Gpa = First.memory_size + Page::Size;
  if (Ram_.Size() <= Gpa) {
    perror("The RAM size is smaller than expected");
    return false;
  }

  const struct kvm_userspace_memory_region Second = {
      .slot = 1,
      .flags = KVM_MEM_LOG_DIRTY_PAGES,
      .guest_phys_addr = Gpa,
      .memory_size = Ram_.Size() - Gpa,
      .userspace_addr = First.userspace_addr + Gpa,
  };

  MemoryRegions_[1].Initialize(Second);
  if (!RegisterMemory(MemoryRegions_[1])) {
    perror("Cannot register second part of RAM");
    return false;
  }

  //
  // The kernel regularly scans those areas of user memory that have been marked
  // as mergeable, looking for pages with identical content. These are replaced
  // by a single write-protected page (which is automatically copied if a
  // process later wants to update the content of the page). KSM merges only
  // private anonymous pages.
  //

  madvise(Ram_.Hva(), Ram_.Size(), MADV_MERGEABLE);
  return true;
}

bool KvmBackend_t::OnExitCoverageBp(const Gva_t Rip) {
  const Gpa_t Gpa = CovBreakpoints_.at(Rip);
  Ram_.RemoveBreakpoint(Gpa);

  if (TraceFile_) {
    fmt::print(TraceFile_, "{:#x}\n", Rip);
  }

  CovBreakpoints_.erase(Rip);
  Coverage_.emplace(Rip);
  return true;
}

bool KvmBackend_t::OnExitDebug(struct kvm_debug_exit_arch &Debug) {
  const Gva_t Rip = Gva_t(Debug.pc);

  if (Debug.exception == 3) {

    //
    // First let's handle the case where the breakpoint is a coverage
    // breakpoint.
    //

    const bool CoverageBp = CovBreakpoints_.contains(Rip);
    const bool IsBreakpoint = Breakpoints_.contains(Rip);

    if (!CoverageBp && !IsBreakpoint) {
      SaveCrash(Rip, EXCEPTION_BREAKPOINT);
      return true;
    }

    //
    // Is it a coverage breakpoint?
    //

    if (CoverageBp) {
      if (!OnExitCoverageBp(Rip)) {
        return false;
      }
    }

    //
    // If this was JUST a coverage breakpoint, then we're fine.
    //

    if (!IsBreakpoint) {
      return true;
    }

    //
    // Well this was also a normal breakpoint..
    //

    KvmBreakpoint_t &Breakpoint = Breakpoints_.at(Rip);
    Breakpoint.Handler(this);

    //
    // If we hit a coverage breakpoint right before, it means that the 0xcc has
    // been removed
    // and restored by the original byte. In that case, we need to re-arm it,
    // otherwise we loose our breakpoint.
    //

    if (CoverageBp) {
      Ram_.AddBreakpoint(Breakpoint.Gpa);
    }

    //
    // If the breakpoint handler moved rip, injected a pending pf or asked to
    // stop the testcase, then no need to single step over the instruction.
    //

    const auto &Exception = Run_->s.regs.events.exception;
    const bool InjectedPf = Exception.injected == 1 && Exception.nr == PfVector;
    if (Run_->s.regs.regs.rip != Rip.U64() || InjectedPf || Stop_) {
      return true;
    }

    //
    // Sounds like we need to disarm the breakpoint, turn on TF to step over the
    // instruction. We'll take a fault and re-arm the breakpoint when we get the
    // fault.
    //

    KvmDebugPrint("Disarming bp and turning on RFLAGS.TF\n");
    LastBreakpointGpa_ = Breakpoint.Gpa;

    Ram_.RemoveBreakpoint(Breakpoint.Gpa);

    struct kvm_guest_debug Dregs;
    Dregs.control =
        KVM_GUESTDBG_USE_SW_BP | KVM_GUESTDBG_ENABLE | KVM_GUESTDBG_SINGLESTEP;
    if (!SetDregs(Dregs)) {
      return false;
    }

    return true;
  }

  if (Debug.exception == 1) {

    //
    // If we got a TF, then let's re-enable all our of breakpoints. Remember if
    // we get there, it is because we hit a breakpoint, turned on TF in order to
    // step over the instruction, and now we get the chance to turn back on the
    // breakpoint.
    //

    Ram_.AddBreakpoint(LastBreakpointGpa_);
    LastBreakpointGpa_ = Gpa_t(0xffffffffffffffff);

    //
    // Strip TF off Rflags.
    //

    struct kvm_guest_debug Dregs;
    Dregs.control = KVM_GUESTDBG_USE_SW_BP | KVM_GUESTDBG_ENABLE;
    if (!SetDregs(Dregs)) {
      return false;
    }

    KvmDebugPrint("Turning off RFLAGS.TF\n");
    return true;
  }

  return true;
}

std::optional<TestcaseResult_t> KvmBackend_t::Run(const uint8_t *Buffer,
                                                  const uint64_t BufferSize) {
  const char *ExitToStr[] = {
      "KVM_EXIT_UNKNOWN",
      "KVM_EXIT_EXCEPTION",
      "KVM_EXIT_IO",
      "KVM_EXIT_HYPERCALL",
      "KVM_EXIT_DEBUG",
      "KVM_EXIT_HLT",
      "KVM_EXIT_MMIO",
      "KVM_EXIT_IRQ_WINDOW_OPEN",
      "KVM_EXIT_SHUTDOWN",
      "KVM_EXIT_FAIL_ENTRY",
      "KVM_EXIT_INTR",
      "KVM_EXIT_SET_TPR",
      "KVM_EXIT_TPR_ACCESS",
      "KVM_EXIT_S390_SIEIC",
      "KVM_EXIT_S390_RESET",
      "KVM_EXIT_DCR", /* deprecated */
      "KVM_EXIT_NMI",
      "KVM_EXIT_INTERNAL_ERROR",
      "KVM_EXIT_OSI",
      "KVM_EXIT_PAPR_HCALL",
      "KVM_EXIT_S390_UCONTROL",
      "KVM_EXIT_WATCHDOG",
      "KVM_EXIT_S390_TSCH",
      "KVM_EXIT_EPR",
      "KVM_EXIT_SYSTEM_EVENT",
      "KVM_EXIT_S390_STSI",
      "KVM_EXIT_IOAPIC_EOI",
      "KVM_EXIT_HYPERV",
  };

  static bool FirstTime = true;
  if (FirstTime && !PmuAvailable_) {
    //
    // Unblock SIGALRM just for the VCPU thread. If we don't do that, the signal
    // can get queued on any available thread that doesn't block SIGALARM. When
    // this happens the VCPU thread doesn't detect that it should be stopping
    // and it carries on forever (because KVM checks if there's a pending signal
    // to be handled on the vcpu thread as a way to know when to stop; this is
    // on top of using |Run->immediate_exit|). The idea is that at this point
    // all the threads needed that we spawn have been created; and they have
    // inherited the signal mask that blocks SIGALARM so this thread will be the
    // only one receiving the signal.
    //

    sigset_t Sigset;
    if (pthread_sigmask(SIG_UNBLOCK, nullptr, &Sigset) < 0) {
      perror("pthread_sigmask SIG_UNBLOCK1");
      return {};
    }

    if (sigaddset(&Sigset, SIGALRM) < 0) {
      perror("sigaddset");
      return {};
    }

    if (pthread_sigmask(SIG_UNBLOCK, &Sigset, nullptr) < 0) {
      perror("pthread_sigmask SIG_UNBLOCK2");
      return {};
    }

    FirstTime = false;
  }

  if (!PmuAvailable_ && Limit_ > 0) {
    const struct itimerval Interval = {.it_interval =
                                           {
                                               .tv_sec = 0,
                                           },
                                       .it_value = {
                                           .tv_sec = uint32_t(Limit_),
                                       }};

    if (setitimer(ITIMER_REAL, &Interval, nullptr) < 0) {
      perror("setitimer");
      return {};
    }
  }

  TestcaseBuffer_ = Buffer;
  TestcaseBufferSize_ = BufferSize;
  Stop_ = false;
  TestcaseRes_ = Ok_t();
  Coverage_.clear();
  Run_->immediate_exit = 0;

  while (!Stop_) {

    //
    // Ask KVM for regs / sregs / guest events regs.
    //

    Run_->kvm_valid_regs =
        KVM_SYNC_X86_REGS | KVM_SYNC_X86_SREGS | KVM_SYNC_X86_EVENTS;
    const int Ret = ioctl(Vp_, KVM_RUN, nullptr);

    //
    // Let's check for errors / timeout.
    //

    if (Ret < 0) {
      if (errno != EINTR) {
        perror("KVM_RUN");
        return {};
      }

      //
      // We handle KVM_EXIT_INTR in the switch case below.
      //

      Run_->exit_reason = KVM_EXIT_INTR;
    }

    RunStats_.Vmexits++;

    //
    // Reset the dirty regs.
    //

    Run_->kvm_dirty_regs = 0;
    switch (Run_->exit_reason) {
    case KVM_EXIT_INTR: {
      KvmDebugPrint("exit_reason = KVM_EXIT_INTR\n");
      TestcaseRes_ = Timedout_t();
      Stop_ = true;
      break;
    }

    case KVM_EXIT_SHUTDOWN: {
      fmt::print("exit_reason = KVM_EXIT_SHUTDOWN\n");
      fmt::print("{:#x}\n", Run_->hw.hardware_exit_reason);
      Stop_ = true;
      break;
    }

    case KVM_EXIT_SET_TPR: {
      // const auto &TprAccess = Run_->tpr_access;
      // fmt::print("exit_reason = KVM_EXIT_SET_TPR\n");
      // fmt::print("  rip={:#x}\n", TprAccess.rip);
      // fmt::print("  is_write={:#x}\n", TprAccess.is_write);
      break;
    }

    case KVM_EXIT_DEBUG: {
      KvmDebugPrint("exit_reason = KVM_EXIT_DEBUG @ {:#x}\n",
                    Run_->debug.arch.pc);
      if (!OnExitDebug(Run_->debug.arch)) {
        Stop_ = true;
      }
      break;
    }

    case KVM_EXIT_FAIL_ENTRY: {
      fmt::print("exit_reason = KVM_EXIT_FAIL_ENTRY\n");
      fmt::print("  fail_entry.hardware_entry_failure_reason = {:#x}\n",
                 Run_->fail_entry.hardware_entry_failure_reason);
      Stop_ = true;
      break;
    }

    default: {
      const char *ExitStr =
          Run_->exit_reason >= (sizeof(ExitToStr) / sizeof(ExitToStr[0]))
              ? "unknown"
              : ExitToStr[Run_->exit_reason];
      fmt::print("exit_reason = {} ({:#x})\n", ExitStr, Run_->exit_reason);
      Stop_ = true;
      break;
    }
    }
  }

  //
  // Close the trace file if we had one.
  //

  if (TraceFile_) {
    fclose(TraceFile_);
    TraceFile_ = nullptr;

    //
    // Reset the code coverage breakpoint in we were tracing. We want to make
    // sure every test case gets a full trace vs getting only what hasn't been
    // executed before.
    //

    if (!RevokeLastNewCoverage()) {
      fmt::print("RevokeLastNewCoverage failed\n");
      return {};
    }
  }

  return TestcaseRes_;
}

bool KvmBackend_t::Restore(const CpuState_t &CpuState) {
  if (PmuAvailable_) {
    RunStats_.InstructionsExecuted = GetMsr(MSR_IA32_FIXED_CTR0);
  }

  if (!LoadState(CpuState)) {
    return false;
  }

  for (const auto &MemoryRegion : MemoryRegions_) {

    //
    // Get the dirty log.
    //

    if (!GetDirtyLog(MemoryRegion)) {
      return false;
    }

    //
    // Walk the bitmap.
    //

    const uint64_t NumberBits = 64;
    for (uint64_t QwordIdx = 0; QwordIdx < MemoryRegion.DirtyBitmapSizeQwords;
         QwordIdx++) {
      const uint64_t DirtyQword = MemoryRegion.DirtyBitmap[QwordIdx];
      if (DirtyQword == 0) {
        continue;
      }

      for (uint64_t BitIdx = 0; BitIdx < NumberBits; BitIdx++) {
        const uint8_t DirtyBit = (DirtyQword >> BitIdx) & 1;
        if (DirtyBit == 0) {
          continue;
        }

        const uint64_t DirtyPageIdx = (QwordIdx * NumberBits) + BitIdx;
        const uint64_t DirtyGpaRelative = DirtyPageIdx * Page::Size;
        const Gpa_t DirtyGpa =
            Gpa_t(MemoryRegion.Kvm.guest_phys_addr + DirtyGpaRelative);
        DirtyGpas_.emplace(DirtyGpa);
      }
    }
  }

  // fmt::print("Restoring {} dirty pages..\n", DirtyGpas_.size());
  RunStats_.Dirty = DirtyGpas_.size();

  //
  // Restores the dirty GPAs.
  //

  for (const auto &DirtyGpa : DirtyGpas_) {
    Ram_.Restore(DirtyGpa);
  }

  //
  // Clear dirty.
  //

  for (const auto &MemoryRegion : MemoryRegions_) {
    if (!ClearDirtyLog(MemoryRegion)) {
      return false;
    }
  }

  DirtyGpas_.clear();
  return true;
}

void KvmBackend_t::Stop(const TestcaseResult_t &Res) {
  TestcaseRes_ = Res;
  Stop_ = true;
}

void KvmBackend_t::SetLimit(const uint64_t Limit) { Limit_ = Limit; }

uint64_t KvmBackend_t::GetReg(const Registers_t Reg) {
  switch (Reg) {
  case Registers_t::Rax: {
    return Run_->s.regs.regs.rax;
  }

  case Registers_t::Rbx: {
    return Run_->s.regs.regs.rbx;
  }

  case Registers_t::Rcx: {
    return Run_->s.regs.regs.rcx;
  }

  case Registers_t::Rdx: {
    return Run_->s.regs.regs.rdx;
  }

  case Registers_t::Rsi: {
    return Run_->s.regs.regs.rsi;
  }

  case Registers_t::Rdi: {
    return Run_->s.regs.regs.rdi;
  }

  case Registers_t::Rip: {
    return Run_->s.regs.regs.rip;
  }

  case Registers_t::Rsp: {
    return Run_->s.regs.regs.rsp;
  }

  case Registers_t::Rbp: {
    return Run_->s.regs.regs.rbp;
  }

  case Registers_t::R8: {
    return Run_->s.regs.regs.r8;
  }

  case Registers_t::R9: {
    return Run_->s.regs.regs.r9;
  }

  case Registers_t::R10: {
    return Run_->s.regs.regs.r10;
  }

  case Registers_t::R11: {
    return Run_->s.regs.regs.r11;
  }

  case Registers_t::R12: {
    return Run_->s.regs.regs.r12;
  }

  case Registers_t::R13: {
    return Run_->s.regs.regs.r13;
  }

  case Registers_t::R14: {
    return Run_->s.regs.regs.r14;
  }

  case Registers_t::R15: {
    return Run_->s.regs.regs.r15;
  }

  case Registers_t::Rflags: {
    return Run_->s.regs.regs.rflags;
  }

  case Registers_t::Cr2: {
    return Run_->s.regs.sregs.cr2;
  }

  case Registers_t::Cr3: {
    return Run_->s.regs.sregs.cr3;
  }
  }

  //
  // We don't use a default case above to have the compiler warn us
  // when we're missing a case.
  //

  __debugbreak();
  return 0;
}

uint64_t KvmBackend_t::SetReg(const Registers_t Reg, const uint64_t Value) {
  switch (Reg) {
  case Registers_t::Rax: {
    Run_->s.regs.regs.rax = Value;
    break;
  }

  case Registers_t::Rbx: {
    Run_->s.regs.regs.rbx = Value;
    break;
  }

  case Registers_t::Rcx: {
    Run_->s.regs.regs.rcx = Value;
    break;
  }

  case Registers_t::Rdx: {
    Run_->s.regs.regs.rdx = Value;
    break;
  }

  case Registers_t::Rsi: {
    Run_->s.regs.regs.rsi = Value;
    break;
  }

  case Registers_t::Rdi: {
    Run_->s.regs.regs.rdi = Value;
    break;
  }

  case Registers_t::Rip: {
    Run_->s.regs.regs.rip = Value;
    break;
  }

  case Registers_t::Rsp: {
    Run_->s.regs.regs.rsp = Value;
    break;
  }

  case Registers_t::Rbp: {
    Run_->s.regs.regs.rbp = Value;
    break;
  }

  case Registers_t::R8: {
    Run_->s.regs.regs.r8 = Value;
    break;
  }

  case Registers_t::R9: {
    Run_->s.regs.regs.r9 = Value;
    break;
  }

  case Registers_t::R10: {
    Run_->s.regs.regs.r10 = Value;
    break;
  }

  case Registers_t::R11: {
    Run_->s.regs.regs.r11 = Value;
    break;
  }

  case Registers_t::R12: {
    Run_->s.regs.regs.r12 = Value;
    break;
  }

  case Registers_t::R13: {
    Run_->s.regs.regs.r13 = Value;
    break;
  }

  case Registers_t::R14: {
    Run_->s.regs.regs.r14 = Value;
    break;
  }

  case Registers_t::R15: {
    Run_->s.regs.regs.r15 = Value;
    break;
  }

  case Registers_t::Rflags: {
    Run_->s.regs.regs.rflags = Value;
    break;
  }

  case Registers_t::Cr2: {
    Run_->s.regs.sregs.cr2 = Value;
    break;
  }

  case Registers_t::Cr3: {
    Run_->s.regs.sregs.cr3 = Value;
    break;
  }
  }

  //
  // Tell KVM to flush the regs into the VCPU next time it runs.
  //

  Run_->kvm_dirty_regs |= KVM_SYNC_X86_REGS;
  return Value;
}

uint64_t KvmBackend_t::Rdrand() {
  const uint64_t HashSize = sizeof(uint64_t) * 2;
  uint8_t Hash[HashSize];
  blake3_hasher Hasher;
  blake3_hasher_init(&Hasher);
  blake3_hasher_update(&Hasher, &Seed_, sizeof(Seed_));
  blake3_hasher_finalize(&Hasher, Hash, HashSize);

  const uint64_t *P = (uint64_t *)Hash;
  Seed_ = P[0];
  return P[1];
}

void KvmBackend_t::PrintRunStats() { RunStats_.Print(); }

bool KvmBackend_t::SetTraceFile(const fs::path &TestcaseTracePath,
                                const TraceType_t TraceType) {
  if (TraceType == TraceType_t::Rip) {
    fmt::print("Rip traces are not supported by kvm.\n");
    return false;
  }

  //
  // Open the trace file.
  //

  TraceFile_ = fopen(TestcaseTracePath.string().c_str(), "w");
  if (TraceFile_ == nullptr) {
    return false;
  }

  return true;
}

bool KvmBackend_t::SetBreakpoint(const Gva_t Gva,
                                 const BreakpointHandler_t Handler) {
  Gpa_t Gpa;
  if (!VirtTranslate(Gva, Gpa, MemoryValidate_t::ValidateRead)) {
    return false;
  }

  //
  // Once we have it, we push the breakpoint/handler in our structures, and let
  // the dmp cache knows about the breakpoint as well.
  //

  if (Breakpoints_.contains(Gva)) {
    fmt::print("/!\\ There already is a breakpoint at {:#x}\n", Gva);
    return false;
  }

  KvmBreakpoint_t Breakpoint(Gpa, Handler);
  Breakpoints_.emplace(Gva, Breakpoint);
  const uint8_t *Hva = Ram_.AddBreakpoint(Gpa);

  fmt::print("Resolved breakpoint {:#x} at GPA {:#x} aka HVA {}\n", Gva, Gpa,
             fmt::ptr(Hva));
  return true;
}

bool KvmBackend_t::DirtyGpa(const Gpa_t Gpa) {
  return DirtyGpas_.emplace(Gpa.Align()).second;
}

// XXX: paging64_gva_to_gpa -> paging64_walk_addr_generic ->
// kvm_vcpu_gfn_to_hva_prot
// https://elixir.bootlin.com/linux/latest/source/arch/x86/kvm/mmu/paging_tmpl.h
// bool KvmBackend_t::SlowVirtTranslate(const uint64_t Gva, uint64_t &Gpa,
//                                      const MemoryValidate_t Validate) {
//   struct kvm_translation Tr;
//   memset(&Tr, 0, sizeof(Tr));
//   Tr.linear_address = Gva;

//   if (ioctl(Vp_, KVM_TRANSLATE, &Tr) < 0) {
//     perror("KVM_TRANSLATE failed");
//     return false;
//   }

//   bool Validated = true;
//   if (Validate & MemoryValidate_t::ValidateWrite) {
//     Validated = Validated && Tr.writeable == 1;
//   }

//   const bool Valid = Tr.valid;
//   Gpa = Tr.physical_address;
//   return Valid && Validated;
// }

bool KvmBackend_t::VirtTranslate(const Gva_t Gva, Gpa_t &Gpa,
                                 const MemoryValidate_t Validate) const {
  //
  // Stole most of the logic from @yrp604's code so thx bro.
  //

  const VIRTUAL_ADDRESS GuestAddress = Gva.U64();
  const MMPTE_HARDWARE Pml4 = Run_->s.regs.sregs.cr3;
  const uint64_t Pml4Base = Pml4.PageFrameNumber * Page::Size;
  const Gpa_t Pml4eGpa = Gpa_t(Pml4Base + GuestAddress.Pml4Index * 8);
  const MMPTE_HARDWARE Pml4e = PhysRead8(Pml4eGpa);
  if (!Pml4e.Present) {
    return false;
  }

  const uint64_t PdptBase = Pml4e.PageFrameNumber * Page::Size;
  const Gpa_t PdpteGpa = Gpa_t(PdptBase + GuestAddress.PdPtIndex * 8);
  const MMPTE_HARDWARE Pdpte = PhysRead8(PdpteGpa);
  if (!Pdpte.Present) {
    return false;
  }

  //
  // huge pages:
  // 7 (PS) - Page size; must be 1 (otherwise, this entry references a page
  // directory; see Table 4-1
  //

  const uint64_t PdBase = Pdpte.PageFrameNumber * Page::Size;
  if (Pdpte.LargePage) {
    Gpa = Gpa_t(PdBase + (Gva.U64() & 0x3fff'ffff));
    return true;
  }

  const Gpa_t PdeGpa = Gpa_t(PdBase + GuestAddress.PdIndex * 8);
  const MMPTE_HARDWARE Pde = PhysRead8(PdeGpa);
  if (!Pde.Present) {
    return false;
  }

  //
  // large pages:
  // 7 (PS) - Page size; must be 1 (otherwise, this entry references a page
  // table; see Table 4-18
  //

  const uint64_t PtBase = Pde.PageFrameNumber * Page::Size;
  if (Pde.LargePage) {
    Gpa = Gpa_t(PtBase + (Gva.U64() & 0x1f'ffff));
    return true;
  }

  const Gpa_t PteGpa = Gpa_t(PtBase + GuestAddress.PtIndex * 8);
  const MMPTE_HARDWARE Pte = PhysRead8(PteGpa);
  if (!Pte.Present) {
    return false;
  }

  const uint64_t PageBase = Pte.PageFrameNumber * 0x1000;
  Gpa = Gpa_t(PageBase + GuestAddress.Offset);
  return true;
}

uint8_t *KvmBackend_t::PhysTranslate(const Gpa_t Gpa) const {
  return Ram_.Hva() + Gpa.U64();
}

Gva_t KvmBackend_t::GetFirstVirtualPageToFault(const Gva_t Gva,
                                               const uint64_t Size) {
  const Gva_t EndGva = Gva + Gva_t(Size);
  for (Gva_t AlignedGva = Gva.Align(); AlignedGva < EndGva;
       AlignedGva += Gva_t(Page::Size)) {
    Gpa_t AlignedGpa;
    if (!VirtTranslate(AlignedGva, AlignedGpa,
                       MemoryValidate_t::ValidateRead)) {
      return AlignedGva;
    }
  }

  return Gva_t(0xffffffffffffffff);
}

bool KvmBackend_t::PageFaultsMemoryIfNeeded(const Gva_t Gva,
                                            const uint64_t Size) {
  const Gva_t PageToFault = GetFirstVirtualPageToFault(Gva, Size);

  //
  // If we haven't found any GVA to fault-in then we have no job to do so we
  // return.
  //

  if (PageToFault == Gva_t(0xffffffffffffffff)) {
    return false;
  }

  KvmDebugPrint("Inserting page fault for GVA {:#x}\n", PageToFault);
  Run_->s.regs.sregs.cr2 = PageToFault.U64();

  Run_->s.regs.events.exception = {.injected = 1,
                                   .nr = PfVector,
                                   .has_error_code = 1,
                                   .error_code = ErrorWrite | ErrorUser};

  Run_->kvm_dirty_regs |= KVM_SYNC_X86_SREGS | KVM_SYNC_X86_EVENTS;
  return true;
}

const tsl::robin_set<Gva_t> &KvmBackend_t::LastNewCoverage() const {
  return Coverage_;
}

bool KvmBackend_t::RevokeLastNewCoverage() {

  //
  // To revoke coverage we need to reset code coverage breakpoint at those
  // addresses.
  //

  for (const auto &Gva : Coverage_) {

    //
    // To set a breakpoint, we need to know the GPA associated with it, so
    // let's get it.
    //

    Gpa_t Gpa;
    const bool Translate =
        VirtTranslate(Gva, Gpa, MemoryValidate_t::ValidateReadExecute);

    if (!Translate) {
      fmt::print("Failed to translate GVA {:#x}\n", Gva);
      return false;
    }

    //
    // Set a breakpoint in RAM.
    //

    if (Ram_.AddBreakpoint(Gpa) == nullptr) {
      return false;
    }

    //
    // Re-emplace the coverage.
    //

    CovBreakpoints_.emplace(Gva, Gpa);
  }

  Coverage_.clear();
  return true;
}

bool KvmBackend_t::RegisterMemory(const KvmMemoryRegion_t &MemoryRegion) {
  if (ioctl(Vm_, KVM_SET_USER_MEMORY_REGION, &MemoryRegion.Kvm) < 0) {
    perror("Cannot RegisterMemory");
    return false;
  }

  return true;
}

bool KvmBackend_t::PhysRead(const Gpa_t Gpa, uint8_t *Buffer,
                            const uint64_t BufferSize) const {
  const uint8_t *Src = PhysTranslate(Gpa);
  memcpy(Buffer, Src, BufferSize);
  return true;
}

uint64_t KvmBackend_t::PhysRead8(const Gpa_t Gpa) const {
  uint64_t Qword;
  if (!PhysRead(Gpa, (uint8_t *)&Qword, sizeof(Qword))) {
    __debugbreak();
  }
  return Qword;
}

bool KvmBackend_t::SetupDemandPaging() {
  //
  // Documentation:
  // https://www.kernel.org/doc/html/latest/admin-guide/mm/userfaultfd.html
  //

  Uffd_ = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK);
  if (Uffd_ < 0) {
    perror("__NR_userfaultfd");
    return false;
  }

  //
  // When first opened the userfaultfd must be enabled invoking the UFFDIO_API
  // ioctl specifying a uffdio_api.api value set to UFFD_API (or a later API
  // version) which will specify the read/POLLIN protocol userland intends to
  // speak on the UFFD and the uffdio_api.features userland requires. The
  // UFFDIO_API ioctl if successful (i.e. if the requested uffdio_api.api is
  // spoken also by the running kernel and the requested features are going to
  // be enabled) will return into uffdio_api.features and uffdio_api.ioctls two
  // 64bit bitmasks of respectively all the available features of the read(2)
  // protocol and the generic ioctl available.
  //

  const struct uffdio_api UffdioApi = {
      .api = UFFD_API,
  };

  if (ioctl(Uffd_, UFFDIO_API, &UffdioApi) < 0) {
    perror("ioctl uffdio_api failed\n");
    return false;
  }

  //
  // The uffdio_api.features bitmask returned by the UFFDIO_API ioctl defines
  // what memory types are supported by the userfaultfd and what events, except
  // page fault notifications, may be generated. Once the userfaultfd has been
  // enabled the UFFDIO_REGISTER ioctl should be invoked (if present in the
  // returned uffdio_api.ioctls bitmask) to register a memory range in the
  // userfaultfd by setting the uffdio_register structure accordingly.
  //

  struct uffdio_register UffdioRegister = {
      .range = {.start = uint64_t(Ram_.Hva()), .len = Ram_.Size()},

      //
      // The uffdio_register.mode bitmask will specify to the kernel which kind
      // of faults to track for the range (UFFDIO_REGISTER_MODE_MISSING would
      // track missing pages).
      //

      .mode = UFFDIO_REGISTER_MODE_MISSING};

  if (ioctl(Uffd_, UFFDIO_REGISTER, &UffdioRegister) < 0) {
    perror("UFFDIO_REGISTER");
    return false;
  }

  //
  // The UFFDIO_REGISTER ioctl will return the uffdio_register.ioctls bitmask of
  // ioctls that are suitable to resolve userfaults on the range registered. Not
  // all ioctls will necessarily be supported for all memory types depending on
  // the underlying virtual memory backend (anonymous memory vs tmpfs vs real
  // filebacked mappings).
  //

  if ((UffdioRegister.ioctls & UFFD_API_RANGE_IOCTLS) !=
      UffdioRegister.ioctls) {
    fmt::print("Unexpected UFFDIO_REGISTER ioctls, bailing\n");
    return false;
  }

  //
  // We are ready to kick off the thread that does demand paging.
  //

  UffdThread_ = std::thread(StaticUffdThreadMain, this);
  return true;
}

void KvmBackend_t::UffdThreadMain() {
  while (!UffdThreadStop_) {

    //
    // Set up the pool fd with the uffd fd.
    //

    struct pollfd PoolFd = {.fd = Uffd_, .events = POLLIN};

    int Res = poll(&PoolFd, 1, 6000);
    if (Res < 0) {

      //
      // Sometimes poll returns -EINTR when we are trying to kick off the CPU
      // out of KVM_RUN.
      //

      if (errno == EINTR) {
        fmt::print("Poll returned EINTR\n");
        continue;
      }

      perror("poll");
      exit(EXIT_FAILURE);
    }

    //
    // This is the timeout, so we loop around to have a chance to check for
    // UffdThreadStop_.
    //

    if (Res == 0) {
      continue;
    }

    //
    // You get the address of the access that triggered the missing page event
    // out of a struct uffd_msg that you read in the thread from the uffd. You
    // can supply as many pages as you want with UFFDIO_COPY or UFFDIO_ZEROPAGE.
    // Keep in mind that unless you used DONTWAKE then the first of any of those
    // IOCTLs wakes up the faulting thread.
    //

    struct uffd_msg UffdMsg;
    Res = read(Uffd_, &UffdMsg, sizeof(UffdMsg));
    if (Res < 0) {
      perror("read");
      exit(EXIT_FAILURE);
    }

    //
    // Let's ensure we are dealing with what we think we are dealing with.
    //

    if (Res != sizeof(UffdMsg) || UffdMsg.event != UFFD_EVENT_PAGEFAULT) {
      fmt::print("The uffdmsg or the type of event we received is unexpected, "
                 "bailing.");
      exit(EXIT_FAILURE);
    }

    //
    // Grab the HVA off the message.
    //

    const uint64_t Hva = UffdMsg.arg.pagefault.address;

    //
    // Compute the GPA from the HVA.
    //

    const Gpa_t Gpa = Gpa_t(Hva - uint64_t(Ram_.Hva()));

    //
    // Page it in.
    //

    RunStats_.UffdPages++;
    const uint8_t *Src = Ram_.GetHvaFromDump(Gpa);
    if (Src != nullptr) {
      const struct uffdio_copy UffdioCopy = {
          .dst = Hva,
          .src = uint64_t(Src),
          .len = Page::Size,
      };

      //
      // The primary ioctl to resolve userfaults is UFFDIO_COPY. That atomically
      // copies a page into the userfault registered range and wakes up the
      // blocked userfaults (unless uffdio_copy.mode & UFFDIO_COPY_MODE_DONTWAKE
      // is set). Other ioctl works similarly to UFFDIO_COPY. They’re atomic as
      // in guaranteeing that nothing can see an half copied page since it’ll
      // keep userfaulting until the copy has finished.
      //

      Res = ioctl(Uffd_, UFFDIO_COPY, &UffdioCopy);
      if (Res < 0) {
        perror("UFFDIO_COPY");
        exit(EXIT_FAILURE);
      }
    } else {
      const struct uffdio_zeropage UffdioZeroPage = {
          .range = {.start = Hva, .len = Page::Size}};

      Res = ioctl(Uffd_, UFFDIO_ZEROPAGE, &UffdioZeroPage);
      if (Res < 0) {
        perror("UFFDIO_ZEROPAGE");
        exit(EXIT_FAILURE);
      }
    }
  }
}

bool KvmBackend_t::SetCoverageBps() {
  //
  // If the user didn't provided a path with code coverage files or if the
  // folder doesn't exist, then we have nothing to do.
  //

  if (CoveragePath_.empty() || !fs::exists(CoveragePath_)) {
    return true;
  }

  //
  // If the user provided a directory let's parse them.
  //

  auto CovBreakpointsMaybe = ParseCovFiles(*this, CoveragePath_);
  if (!CovBreakpointsMaybe) {
    return false;
  }

  //
  // Move the breakpoints in the class.
  //

  CovBreakpoints_ = std::move(*CovBreakpointsMaybe);

  //
  // Set the breakpoints.
  //

  for (const auto &[Gva, Gpa] : CovBreakpoints_) {

    //
    // Set a breakpoint in RAM.
    //

    if (Ram_.AddBreakpoint(Gpa) == nullptr) {
      return false;
    }
  }

  fmt::print("Applied {} code coverage breakpoints\n", CovBreakpoints_.size());
  return true;
}

void KvmBackend_t::SignalAlarm() {
  __atomic_store_n(&Run_->immediate_exit, 1, __ATOMIC_RELAXED);
}

void KvmBackend_t::StaticUffdThreadMain(KvmBackend_t *Ptr) {
  Ptr->UffdThreadMain();
}

void KvmBackend_t::StaticSignalAlarm(int, siginfo_t *, void *) {
  KvmBackend_t *KvmBackend = reinterpret_cast<KvmBackend_t *>(g_Backend);
  KvmBackend->SignalAlarm();
}

#endif
