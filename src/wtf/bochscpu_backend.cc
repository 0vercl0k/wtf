// Axel '0vercl0k' Souchet - February 28 2020
#include "bochscpu_backend.h"
#include "blake3.h"
#include "bochscpu.hpp"
#include "compcov.h"
#include "fmt/core.h"
#include "globals.h"
#include "platform.h"
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <fstream>

constexpr bool BochsLoggingOn = false;
constexpr bool BochsHooksLoggingOn = false;

template <typename... Args_t>
void BochsDebugPrint(const char *Format, const Args_t &...args) {
  if constexpr (BochsLoggingOn) {
    fmt::print("bochs: ");
    fmt::print(fmt::runtime(Format), args...);
  }
}

template <typename... Args_t>
void BochsHooksDebugPrint(const char *Format, const Args_t &...args) {
  if constexpr (BochsHooksLoggingOn) {
    fmt::print("bochshooks: ");
    fmt::print(fmt::runtime(Format), args...);
  }
}

//
// This is a function that gets called when there is missing physical memory.
// It is very useful because it allows us to adopt a lazy paging mechanism.
// Everything will get loaded lazily in memory even the page tables hierarchy.
//

void StaticGpaMissingHandler(const uint64_t Gpa) {

  //
  // Align the GPA.
  //

  const Gpa_t AlignedGpa = Gpa_t(Gpa).Align();
  BochsHooksDebugPrint("GpaMissingHandler: Mapping GPA {:#x} ({:#x}) ..\n",
                       AlignedGpa, Gpa);

  //
  // Retrieve the page from the dump file.
  //

  const void *DmpPage =
      reinterpret_cast<BochscpuBackend_t *>(g_Backend)->GetPhysicalPage(
          AlignedGpa);
  if (DmpPage == nullptr) {
    BochsHooksDebugPrint(
        "GpaMissingHandler: GPA {:#x} is not mapped in the dump.\n",
        AlignedGpa);
    //__debugbreak();
  }

  //
  // Allocate a new page of memory. We allocate a new page because the dump
  // memory is not writeable. Also, because we will be using the original page
  // content to be able to restore the context.
  //
  // Something *really* important is that the allocation *needs* to be page
  // aligned as bochs assume they are. Bochs does computation like `base |
  // offset` (which is equivalent to base + offset assuming base is aligned) but
  // it doesn't hold if base is not aligned: (0000022ed2ae7010 | 00000738) !=
  // (0000022ed2ae7010 + 00000738) but (0000022ed2ae7000 | 00000738) ==
  // (0000022ed2ae7000 + 00000738).
  //

#if defined WINDOWS

  //
  // VirtualAlloc is able to give us back page-aligned allocation, but every
  // time we allocate 1 page, the allocator actually reserve a 64KB region of VA
  // and we'll use the first page of that. This basically fragment the
  // address-space, so what we do is we actually reserve a 64KB region, and
  // we'll commit pages as we need them.
  //

  static size_t Left = 0;
  static uint8_t *Current = nullptr;
  if (Left == 0) {

    //
    // It's time to reserve a 64KB region.
    //

    const uint64_t _64KB = 1024 * 64;
    Left = _64KB;
    Current =
        (uint8_t *)VirtualAlloc(nullptr, Left, MEM_RESERVE, PAGE_READWRITE);
  }

  //
  // Commit a page off the reserved region.
  //

  uint8_t *Page =
      (uint8_t *)VirtualAlloc(Current, Page::Size, MEM_COMMIT, PAGE_READWRITE);

  Left -= Page::Size;
  Current += Page::Size;
  if (Page == nullptr) {
#elif defined LINUX
  uint8_t *Page = (uint8_t *)mmap(nullptr, Page::Size, PROT_READ | PROT_WRITE,
                                  MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (Page == (void *)-1) {
#endif
    fmt::print("Failed to allocate memory in GpaMissingHandler.\n");
    __debugbreak();
  }

  if (DmpPage) {

    //
    // Copy the dump page into the new page.
    //

    memcpy(Page, DmpPage, Page::Size);

  } else {

    //
    // Fake it 'till you make it.
    //

    memset(Page, 0, Page::Size);
  }

  //
  // Tell bochscpu that we inserted a page backing the requested GPA.
  //

  bochscpu_mem_page_insert(AlignedGpa.U64(), Page);
}

void StaticPhyAccessHook(void *Context, uint32_t Id, uint64_t PhysicalAddress,
                         uintptr_t Len, uint32_t MemType, uint32_t MemAccess) {

  //
  // Invoking the member function now.
  //

  reinterpret_cast<BochscpuBackend_t *>(Context)->PhyAccessHook(
      Id, PhysicalAddress, Len, MemType, MemAccess);
}

void StaticAfterExecutionHook(void *Context, uint32_t Id, void *Ins) {

  //
  // Invoking the member function now.
  //

  return reinterpret_cast<BochscpuBackend_t *>(Context)->AfterExecutionHook(
      Id, Ins);
}

void StaticBeforeExecutionHook(void *Context, uint32_t Id, void *Ins) {

  //
  // Invoking the member function now.
  //

  return reinterpret_cast<BochscpuBackend_t *>(Context)->BeforeExecutionHook(
      Id, Ins);
}

void StaticLinAccessHook(void *Context, uint32_t Id, uint64_t VirtualAddress,
                         uint64_t PhysicalAddress, uintptr_t Len,
                         uint32_t MemType, uint32_t MemAccess) {

  //
  // Invoking the member function now.
  //

  reinterpret_cast<BochscpuBackend_t *>(Context)->LinAccessHook(
      Id, VirtualAddress, PhysicalAddress, Len, MemType, MemAccess);
}

void StaticInterruptHook(void *Context, uint32_t Id, uint32_t Vector) {

  //
  // Invoking the member function now.
  //

  reinterpret_cast<BochscpuBackend_t *>(Context)->InterruptHook(Id, Vector);
}

void StaticExceptionHook(void *Context, uint32_t Id, uint32_t Vector,
                         uint32_t ErrorCode) {

  //
  // Invoking the member function now.
  //

  reinterpret_cast<BochscpuBackend_t *>(Context)->ExceptionHook(Id, Vector,
                                                                ErrorCode);
}

void StaticTlbControlHook(void *Context, uint32_t Id, uint32_t What,
                          uint64_t NewCrValue) {

  //
  // Invoking the member function now.
  //

  reinterpret_cast<BochscpuBackend_t *>(Context)->TlbControlHook(Id, What,
                                                                 NewCrValue);
}

void StaticOpcodeHook(void *Context, uint32_t Id, const void *i,
                      const uint8_t *opcode, uintptr_t len, bool is32,
                      bool is64) {

  //
  // Invoking the member function now.
  //

  reinterpret_cast<BochscpuBackend_t *>(Context)->OpcodeHook(Id, i, opcode, len,
                                                             is32, is64);
}

void StaticHltHook(void *Context, uint32_t Cpu) {

  //
  // Invoking the member function now.
  //

  reinterpret_cast<BochscpuBackend_t *>(Context)->OpcodeHlt(Cpu);
}

void StaticUcNearBranchHook(void *Context, uint32_t Cpu, uint32_t What,
                            uint64_t Rip, uint64_t NextRip) {
  if ((What == BOCHSCPU_INSTR_IS_JMP_INDIRECT) ||
      (What == BOCHSCPU_INSTR_IS_CALL_INDIRECT)) {

    //
    // Invoking the member function now.
    //

    reinterpret_cast<BochscpuBackend_t *>(Context)->RecordEdge(Cpu, Rip,
                                                               NextRip);
  }
}

void StaticCNearBranchHook(void *Context, uint32_t Cpu, uint64_t Rip,
                           uint64_t NextRip) {

  //
  // Invoking the member function now.
  //

  reinterpret_cast<BochscpuBackend_t *>(Context)->RecordEdge(Cpu, Rip, NextRip);
}

BochscpuBackend_t::BochscpuBackend_t() {

  //
  // Zero init a bunch of variables.
  //

  memset(&Hooks_, 0, sizeof(Hooks_));
  memset(&HookChain_, 0, sizeof(HookChain_));
}

bool BochscpuBackend_t::Initialize(const Options_t &Opts,
                                   const CpuState_t &CpuState) {

  //
  // Open the dump file.
  //

  const std::string DumpPathA(Opts.DumpPath.string());
  if (!DmpParser_.Parse(DumpPathA.c_str())) {
    fmt::print("Parsing '{}' failed, bailing.\n", DumpPathA.c_str());
    return false;
  }

  //
  // Create a cpu.
  //

  Cpu_ = bochscpu_cpu_new(0);

  //
  // Prepare the hooks.
  //

  Hooks_.ctx = this;
  Hooks_.after_execution = StaticAfterExecutionHook;
  Hooks_.before_execution = StaticBeforeExecutionHook;
  Hooks_.lin_access = StaticLinAccessHook;
  Hooks_.interrupt = StaticInterruptHook;
  Hooks_.exception = StaticExceptionHook;
  Hooks_.phy_access = StaticPhyAccessHook;
  Hooks_.tlb_cntrl = StaticTlbControlHook;
  Hooks_.hlt = StaticHltHook;
  // Hooks_.opcode = StaticOpcodeHook;

  //
  // If edge coverage is enabled, configure hooks to be able to record
  // edges from branches.
  //

  if (Opts.Edges) {
    Hooks_.cnear_branch_taken = StaticCNearBranchHook;
    Hooks_.cnear_branch_not_taken = StaticCNearBranchHook;
    Hooks_.ucnear_branch = StaticUcNearBranchHook;
  }

  // @TODO: Maybe it's better to enable laf using a second set of hooks?
  LafMode_ = Opts.Laf;
  LafAllowedRanges_ = Opts.LafAllowedRanges;

  // Enable compcov for various compare functions.
  if (Opts.Compcov) {
    if (!CompcovSetupHooks()) {
      fmt::print("/!\\ Failed to setup some compcov hooks\n");
    }
  }

  //
  // Initialize the hook chain with only one set of hooks.
  //

  HookChain_[0] = &Hooks_;
  HookChain_[1] = nullptr;

  //
  // Install handler that gets called when physical memory
  // is missing.
  //

  bochscpu_mem_missing_page(StaticGpaMissingHandler);

  //
  // Load the state into the CPU.
  //

  LoadState(CpuState);
  Seed_ = CpuState.Seed;
  return true;
}

bool BochscpuBackend_t::SetBreakpoint(const Gva_t Gva,
                                      const BreakpointHandler_t Handler) {
  if (Breakpoints_.contains(Gva)) {
    fmt::print("/!\\ There is already a breakpoint at {:#x}\n", Gva);
    return false;
  }

  Breakpoints_.emplace(Gva, Handler);
  return true;
}

void BochscpuBackend_t::SetLimit(const uint64_t InstructionLimit) {
  InstructionLimit_ = InstructionLimit;
}

std::optional<TestcaseResult_t>
BochscpuBackend_t::Run(const uint8_t *Buffer, const uint64_t BufferSize) {

  //
  // Initialize a few things.
  //

  TestcaseBuffer_ = Buffer;
  TestcaseBufferSize_ = BufferSize;
  LastNewCoverage_.clear();

  //
  // Reset some of the stats.
  //

  RunStats_.Reset();

  //
  // Reset Tenet state.
  //

  Tenet_.MemAccesses_.clear();
  Tenet_.PastFirstInstruction_ = false;

  //
  // Force dumping all the registers if this is a Tenet trace.
  //

  if (TraceType_ == TraceType_t::Tenet) {
    DumpTenetDelta(true);
  }

  //
  // Lift off.
  //

  bochscpu_cpu_run(Cpu_, HookChain_);

  //
  // Dump the last delta for Tenet traces.
  //

  if (TraceType_ == TraceType_t::Tenet) {
    DumpTenetDelta();
  }

  //
  // Fill in the stats.
  //

  RunStats_.AggregatedCodeCoverage = AggregatedCodeCoverage_.size();
  RunStats_.DirtyGpas = DirtyGpas_.size();

  RunStats_.NumberLafCmpHits += RunStats_.NumberLafUniqueCmpHits;
  RunStats_.NumberCompcovHits += RunStats_.NumberCompcovUniqueHits;

  //
  // Return to the user how the testcase exited.
  //

  return TestcaseResult_;
}

void BochscpuBackend_t::LafHandle64BitIntCmp(const uint64_t Op1,
                                             const uint64_t Op2) {
  // The same as described here:
  // https://andreafioraldi.github.io/articles/2019/07/20/aflpp-qemu-compcov.html

  const uint64_t HashedLoc = SplitMix64(bochscpu_cpu_rip(Cpu_));
  const auto UpdateCoverage = [this](const uint64_t HashedLoc) {
    if (InsertCoverageEntry(Gva_t{HashedLoc})) {
      RunStats_.NumberLafUniqueCmpHits++;
    }
  };

  if ((Op1 & 0xff00000000000000) == (Op2 & 0xff00000000000000)) {
    UpdateCoverage(HashedLoc + 6);
    if ((Op1 & 0xff000000000000) == (Op2 & 0xff000000000000)) {
      UpdateCoverage(HashedLoc + 5);
      if ((Op1 & 0xff0000000000) == (Op2 & 0xff0000000000)) {
        UpdateCoverage(HashedLoc + 4);
        if ((Op1 & 0xff00000000) == (Op2 & 0xff00000000)) {
          UpdateCoverage(HashedLoc + 3);
          if ((Op1 & 0xff000000) == (Op2 & 0xff000000)) {
            UpdateCoverage(HashedLoc + 2);
            if ((Op1 & 0xff0000) == (Op2 & 0xff0000)) {
              UpdateCoverage(HashedLoc + 1);
              if ((Op1 & 0xff00) == (Op2 & 0xff00)) {
                UpdateCoverage(HashedLoc);
              }
            }
          }
        }
      }
    }
  }
}

void BochscpuBackend_t::LafHandle32BitIntCmp(const uint32_t Op1,
                                             const uint32_t Op2) {
  // The same as described here:
  // https://andreafioraldi.github.io/articles/2019/07/20/aflpp-qemu-compcov.html

  const uint64_t HashedLoc = SplitMix64(bochscpu_cpu_rip(Cpu_));
  const auto UpdateCoverage = [this](const uint64_t HashedLoc) {
    if (InsertCoverageEntry(Gva_t{HashedLoc})) {
      RunStats_.NumberLafUniqueCmpHits++;
    }
  };

  if ((Op1 & 0xff000000) == (Op2 & 0xff000000)) {
    UpdateCoverage(HashedLoc + 2);
    if ((Op1 & 0xff0000) == (Op2 & 0xff0000)) {
      UpdateCoverage(HashedLoc + 1);
      if ((Op1 & 0xff00) == (Op2 & 0xff00)) {
        UpdateCoverage(HashedLoc);
      }
    }
  }
}

void BochscpuBackend_t::LafHandle16BitIntCmp(const uint16_t Op1,
                                             const uint16_t Op2) {
  // The same as described here:
  // https://andreafioraldi.github.io/articles/2019/07/20/aflpp-qemu-compcov.html

  const uint64_t HashedLoc = SplitMix64(bochscpu_cpu_rip(Cpu_));
  const auto UpdateCoverage = [this](const uint64_t HashedLoc) {
    if (InsertCoverageEntry(Gva_t{HashedLoc})) {
      RunStats_.NumberLafUniqueCmpHits++;
    }
  };

  if ((Op1 & 0xff00) == (Op2 & 0xff00)) {
    UpdateCoverage(HashedLoc);
  }
}

bool BochscpuBackend_t::LafTrySplitIntCmpSub(bochscpu_instr_t *Ins) {
  // Potentially, this function might be a little too aggressive in splitting
  // instructions. The problem is that we are splitting not only
  // comparison/substraction instructions with immediate operands, but also
  // instructions with register, memory, and register/memory operands. This
  // potentially might produce some misleading coverage entries.
  const BochsIns_t Op = static_cast<BochsIns_t>(bochscpu_instr_bx_opcode(Ins));

  switch (Op) {
  // Handle 64-bit CMP instructions.
  case BochsIns_t::BX_IA_CMP_RAXId:
  case BochsIns_t::BX_IA_CMP_EqsIb:
  case BochsIns_t::BX_IA_CMP_EqId:
  case BochsIns_t::BX_IA_CMP_GqEq:
  case BochsIns_t::BX_IA_CMP_EqGq:
  // Handle 64-bit SUB instructions.
  case BochsIns_t::BX_IA_SUB_RAXId:
  case BochsIns_t::BX_IA_SUB_EqsIb:
  case BochsIns_t::BX_IA_SUB_EqId:
  case BochsIns_t::BX_IA_SUB_GqEq:
  case BochsIns_t::BX_IA_SUB_EqGq:
    if (std::optional<OpPair64_t> operands = LafExtract64BitOperands(Ins)) {
      LafCompcovLogInstruction(Ins, operands);
      LafHandle64BitIntCmp(operands->Op1, operands->Op2);
      return true;
    }

    LafCompcovLogInstruction<uint64_t>(Ins, {});
    return false;
  // Handle 32-bit CMP instructions.
  case BochsIns_t::BX_IA_CMP_EAXId:
  case BochsIns_t::BX_IA_CMP_EdId:
  case BochsIns_t::BX_IA_CMP_EdsIb:
  case BochsIns_t::BX_IA_CMP_GdEd:
  case BochsIns_t::BX_IA_CMP_EdGd:
  // Handle 32-bit SUB instructions.
  case BochsIns_t::BX_IA_SUB_EAXId:
  case BochsIns_t::BX_IA_SUB_EdsIb:
  case BochsIns_t::BX_IA_SUB_EdId:
  case BochsIns_t::BX_IA_SUB_GdEd:
  case BochsIns_t::BX_IA_SUB_EdGd:
    if (std::optional<OpPair32_t> operands = LafExtract32BitOperands(Ins)) {
      LafCompcovLogInstruction(Ins, operands);
      LafHandle32BitIntCmp(operands->Op1, operands->Op2);
      return true;
    }

    LafCompcovLogInstruction<uint32_t>(Ins, {});
    return false;
  // Handle 16-bit CMP instructions.
  case BochsIns_t::BX_IA_CMP_AXIw:
  case BochsIns_t::BX_IA_CMP_EwIw:
  case BochsIns_t::BX_IA_CMP_EwsIb:
  case BochsIns_t::BX_IA_CMP_GwEw:
  case BochsIns_t::BX_IA_CMP_EwGw:
  // Handle 16-bit SUB instructions.
  case BochsIns_t::BX_IA_SUB_AXIw:
  case BochsIns_t::BX_IA_SUB_EwsIb:
  case BochsIns_t::BX_IA_SUB_EwIw:
  case BochsIns_t::BX_IA_SUB_GwEw:
  case BochsIns_t::BX_IA_SUB_EwGw:
    if (std::optional<OpPair16_t> operands = LafExtract16BitOperands(Ins)) {
      LafCompcovLogInstruction(Ins, operands);
      LafHandle16BitIntCmp(operands->Op1, operands->Op2);
      return true;
    }

    LafCompcovLogInstruction<uint16_t>(Ins, {});
    return false;
  }

  return false;
}

std::optional<BochscpuBackend_t::OpPair64_t>
BochscpuBackend_t::LafExtract64BitOperands(bochscpu_instr_t *Ins) {
  const BochsIns_t Op = static_cast<BochsIns_t>(bochscpu_instr_bx_opcode(Ins));

  std::optional<OpPair64_t> Operands{};

  switch (Op) {
  case BochsIns_t::BX_IA_CMP_RAXId:
  case BochsIns_t::BX_IA_SUB_RAXId:
    Operands = LafExtractOperands_REGI<uint64_t>(Ins);
    break;
  case BochsIns_t::BX_IA_CMP_EqsIb:
  case BochsIns_t::BX_IA_SUB_EqsIb:
    Operands = LafExtractOperands_EsI<uint64_t>(Ins);
    break;
  case BochsIns_t::BX_IA_CMP_EqId:
  case BochsIns_t::BX_IA_SUB_EqId:
    Operands = LafExtractOperands_EI<uint64_t>(Ins);
    break;
  case BochsIns_t::BX_IA_CMP_GqEq:
  case BochsIns_t::BX_IA_SUB_GqEq:
    Operands = LafExtractOperands_GE<uint64_t>(Ins);
    break;
  case BochsIns_t::BX_IA_CMP_EqGq:
  case BochsIns_t::BX_IA_SUB_EqGq:
    Operands = LafExtractOperands_EG<uint64_t>(Ins);
    break;
  default:
    BochsHooksDebugPrint("Unhandled 64-bit CMP/SUB instruction.\n");
  }

  return Operands;
}

std::optional<BochscpuBackend_t::OpPair32_t>
BochscpuBackend_t::LafExtract32BitOperands(bochscpu_instr_t *Ins) {
  const BochsIns_t Op = static_cast<BochsIns_t>(bochscpu_instr_bx_opcode(Ins));

  std::optional<OpPair32_t> Operands{};

  switch (Op) {
  case BochsIns_t::BX_IA_CMP_EAXId:
  case BochsIns_t::BX_IA_SUB_EAXId:
    Operands = LafExtractOperands_REGI<uint32_t>(Ins);
    break;
  case BochsIns_t::BX_IA_CMP_EdsIb:
  case BochsIns_t::BX_IA_SUB_EdsIb:
    Operands = LafExtractOperands_EsI<uint32_t>(Ins);
    break;
  case BochsIns_t::BX_IA_CMP_EdId:
  case BochsIns_t::BX_IA_SUB_EdId:
    Operands = LafExtractOperands_EI<uint32_t>(Ins);
    break;
  case BochsIns_t::BX_IA_CMP_GdEd:
  case BochsIns_t::BX_IA_SUB_GdEd:
    Operands = LafExtractOperands_GE<uint32_t>(Ins);
    break;
  case BochsIns_t::BX_IA_CMP_EdGd:
  case BochsIns_t::BX_IA_SUB_EdGd:
    Operands = LafExtractOperands_EG<uint32_t>(Ins);
    break;
  default:
    BochsHooksDebugPrint("Unhandled 32-bit CMP/SUB instruction.\n");
  }

  return Operands;
}

std::optional<BochscpuBackend_t::OpPair16_t>
BochscpuBackend_t::LafExtract16BitOperands(bochscpu_instr_t *Ins) {
  const BochsIns_t Op = static_cast<BochsIns_t>(bochscpu_instr_bx_opcode(Ins));

  std::optional<OpPair16_t> Operands{};

  switch (Op) {
  case BochsIns_t::BX_IA_CMP_AXIw:
  case BochsIns_t::BX_IA_SUB_AXIw:
    Operands = LafExtractOperands_REGI<uint16_t>(Ins);
    break;
  case BochsIns_t::BX_IA_CMP_EwsIb:
  case BochsIns_t::BX_IA_SUB_EwsIb:
    Operands = LafExtractOperands_EsI<uint16_t>(Ins);
    break;
  case BochsIns_t::BX_IA_CMP_EwIw:
  case BochsIns_t::BX_IA_SUB_EwIw:
    Operands = LafExtractOperands_EI<uint16_t>(Ins);
    break;
  case BochsIns_t::BX_IA_CMP_GwEw:
  case BochsIns_t::BX_IA_SUB_GwEw:
    Operands = LafExtractOperands_GE<uint16_t>(Ins);
    break;
  case BochsIns_t::BX_IA_CMP_EwGw:
  case BochsIns_t::BX_IA_SUB_EwGw:
    Operands = LafExtractOperands_EG<uint16_t>(Ins);
    break;
  default:
    BochsHooksDebugPrint("Unhandled 16-bit CMP/SUB instruction.\n");
  }

  return Operands;
}

void BochscpuBackend_t::LafSplitCompares(bochscpu_instr_t *Ins) {
  // Enable only for user-mode/kernel-mode.
  if (!(LafMode_ == LafCompcovOptions_t::OnlyUser && BochsCpuIsUserMode() ||
        LafMode_ == LafCompcovOptions_t::OnlyKernel && BochsCpuIsKernelMode() ||
        LafMode_ == LafCompcovOptions_t::KernelAndUser)) {
    return;
  }

  const Gva_t Rip = Gva_t{bochscpu_cpu_rip(Cpu_)};
  const auto CheckInRange = [Rip](const auto &Range) {
    return Range.first < Rip && Rip < Range.second;
  };

  // Check if address range is allowed
  if (LafAllowedRanges_.empty() ||
      std::find_if(LafAllowedRanges_.begin(), LafAllowedRanges_.end(),
                   CheckInRange) != LafAllowedRanges_.end()) {
    //
    // Try to split a given instruction, assuming it is a CMP/SUB instruction.
    //
    LafTrySplitIntCmpSub(Ins);
  }

  return;
}

void BochscpuBackend_t::PhyAccessHook(/*void *Context, */ uint32_t,
                                      uint64_t PhysicalAddress, uintptr_t Len,
                                      uint32_t, uint32_t MemAccess) {

  //
  // Physical memory is getting accessed! Exciting.
  //

  BochsHooksDebugPrint("PhyAccessHook: Access {} bytes to GPA {:#x}.\n", Len,
                       PhysicalAddress);

  //
  // Keep track of stats.
  //

  RunStats_.NumberMemoryAccesses += Len;

  //
  // If this is not a write access, we don't care.
  //

  if (MemAccess != BOCHSCPU_HOOK_MEM_WRITE &&
      MemAccess != BOCHSCPU_HOOK_MEM_RW) {
    return;
  }

  //
  // Adding the physical memory range to the set of dirty GPAs.
  //

  DirtyPhysicalMemoryRange(Gpa_t(PhysicalAddress), Len);
}

void BochscpuBackend_t::AfterExecutionHook(/*void *Context, */ uint32_t,
                                           void *) {

  //
  // Keep track of the instructions executed.
  //

  RunStats_.NumberInstructionsExecuted++;

  //
  // Check the instruction limit.
  //

  if (InstructionLimit_ > 0 &&
      RunStats_.NumberInstructionsExecuted > InstructionLimit_) {

    //
    // If we're over the limit, we stop the cpu.
    //

    BochsHooksDebugPrint("Over the instruction limit ({}), stopping cpu.\n",
                         InstructionLimit_);
    TestcaseResult_ = Timedout_t();
    bochscpu_cpu_stop(Cpu_);
  }
}

//
// This is THE HOT PATH in wtf.
//

#ifdef WINDOWS
__declspec(safebuffers)
#endif
    void BochscpuBackend_t::BeforeExecutionHook(
        /*void *Context, */ uint32_t, void *Ins) {
  const uint32_t Op = bochscpu_instr_bx_opcode(Ins);

  if (Op == BOCHSCPU_OPCODE_INSERTED) {

    //
    // We ignore the opcodes that bochs created as they aren't 'real'
    // instructions. Some more details are in #45.
    //

    return;
  }

  //
  // Grab the rip register off the cpu.
  //

  const Gva_t Rip = Gva_t(bochscpu_cpu_rip(Cpu_));

  //
  // Keep track of new code coverage or log into the trace file.
  //

  const auto &Res = AggregatedCodeCoverage_.emplace(Rip);
  if (Res.second) {
    LastNewCoverage_.emplace(Rip);
  }

  //
  // If LAF is enabled, try to split comparison instructions (cmp, sub, ...)
  //

  if (LafMode_ != LafCompcovOptions_t::Disabled) {
    LafSplitCompares((bochscpu_instr_t *)Ins);
  }

  const bool TenetTrace = TraceType_ == TraceType_t::Tenet;
  if (TraceFile_) {
    const bool RipTrace = TraceType_ == TraceType_t::Rip;
    const bool UniqueRipTrace = TraceType_ == TraceType_t::UniqueRip;
    const bool NewRip = Res.second;

    if (RipTrace || (UniqueRipTrace && NewRip)) {

      //
      // On Linux we don't have access to dbgeng so just write the plain
      // address for both Windows & Linux.
      //

      fmt::print(TraceFile_, "{:#x}\n", Rip);
    } else if (TenetTrace) {
      if (Tenet_.PastFirstInstruction_) {

        //
        // If we already executed an instruction, dump register + mem changes
        // if generating Tenet traces.
        //

        DumpTenetDelta();
      }

      //
      // Save a complete copy of the registers so that we can diff them
      // against the next step when taking Tenet traces.
      //

      bochscpu_cpu_state(Cpu_, &Tenet_.CpuStatePrev_);
      Tenet_.PastFirstInstruction_ = true;
    }
  }

  //
  // Handle breakpoints.
  //

  if (Breakpoints_.contains(Rip)) {
    Breakpoints_.at(Rip)(this);
  }
}

void BochscpuBackend_t::LinAccessHook(
    /*void *Context, */ uint32_t, uint64_t VirtualAddress,
    uint64_t PhysicalAddress, uintptr_t Len, uint32_t, uint32_t MemAccess) {

  //
  // Virtual memory is getting accessed! Exciting.
  //

  BochsHooksDebugPrint(
      "LinAccessHook: Access {} bytes to GVA {:#x} (GPA {:#x}).\n", Len,
      VirtualAddress, PhysicalAddress);

  //
  // Keep track of stats.
  //

  RunStats_.NumberMemoryAccesses += Len;

  //
  // Log explicit details about the memory access if taking a full-trace.
  //

  if (TraceFile_ && TraceType_ == TraceType_t::Tenet) {
    Tenet_.MemAccesses_.emplace_back(VirtualAddress, Len, MemAccess);
  }

  //
  // If this is not a write access, we don't care to go further.
  //

  if (MemAccess != BOCHSCPU_HOOK_MEM_WRITE &&
      MemAccess != BOCHSCPU_HOOK_MEM_RW) {
    return;
  }

  //
  // Adding the physical address the set of dirty GPAs.
  // We don't use DirtyVirtualMemoryRange here as we need to
  // do a GVA->GPA translation which is a bit costly.
  //

  DirtyGpa(Gpa_t(PhysicalAddress));
}

void BochscpuBackend_t::InterruptHook(/*void *Context, */ uint32_t,
                                      uint32_t Vector) {

  //
  // Hit an exception, dump it on stdout.
  //

  BochsHooksDebugPrint("InterruptHook: Vector({:#x})\n", Vector);

  //
  // If we trigger a breakpoint it's probably time to stop the cpu.
  //

  if (Vector != 3) {
    return;
  }

  //
  // This is an int3 so let's stop the cpu.
  //

  BochsDebugPrint("Stopping cpu.\n");
  TestcaseResult_ = Crash_t();
  bochscpu_cpu_stop(Cpu_);
}

void BochscpuBackend_t::ExceptionHook(/*void *Context, */ uint32_t,
                                      uint32_t Vector, uint32_t ErrorCode) {
  // https://wiki.osdev.org/Exceptions
  BochsHooksDebugPrint("ExceptionHook: Vector({:#x}), ErrorCode({:#x})\n",
                       Vector, ErrorCode);
}

void BochscpuBackend_t::TlbControlHook(/*void *Context, */ uint32_t,
                                       uint32_t What, uint64_t NewCrValue) {

  //
  // We only care about CR3 changes.
  //

  if (What != BOCHSCPU_HOOK_TLB_CR3) {
    return;
  }

  //
  // And we only care about it when the CR3 value is actually different from
  // when we started the testcase.
  //

  if (NewCrValue == InitialCr3_) {
    return;
  }

  //
  // Stop the cpu as we don't want to be context-switching.
  //

  BochsHooksDebugPrint("The cr3 register is getting changed ({:#x})\n",
                       NewCrValue);
  BochsHooksDebugPrint("Stopping cpu.\n");
  TestcaseResult_ = Cr3Change_t();
  bochscpu_cpu_stop(Cpu_);
}

void BochscpuBackend_t::OpcodeHook(/*void *Context, */ uint32_t,
                                   const void *Ins, const uint8_t *, uintptr_t,
                                   bool, bool) {
  const uint32_t Op = bochscpu_instr_bx_opcode(Ins);
  // if (Op >= 0x83e && Op <= 0x840) {
  //  fmt::print("rdrand @ {:#x}\n", bochscpu_cpu_rip(Cpu_));
  //  __debugbreak();
  //}
  // return;
  constexpr uint32_t BX_IA_CMP_RAXId = 0x491;
  constexpr uint32_t BX_IA_CMP_EqsIb = 0x4a3;
  constexpr uint32_t BX_IA_CMP_EqId = 0x49a;
  if (Op == BX_IA_CMP_RAXId || Op == BX_IA_CMP_EqId || Op == BX_IA_CMP_EqsIb) {
    fmt::print("cmp with imm64 {:#x}\n", bochscpu_instr_imm64(Ins));
  }

  constexpr uint32_t BX_IA_CMP_EAXId = 0x38;
  constexpr uint32_t BX_IA_CMP_EdId = 0x61;
  constexpr uint32_t BX_IA_CMP_EdsIb = 0x6a;
  if (Op == BX_IA_CMP_EAXId || Op == BX_IA_CMP_EdId || Op == BX_IA_CMP_EdsIb) {
    fmt::print("cmp with imm32 {:#x}\n", bochscpu_instr_imm32(Ins));
  }

  constexpr uint32_t BX_IA_CMP_AXIw = 0x2f;
  constexpr uint32_t BX_IA_CMP_EwIw = 0x4f;
  constexpr uint32_t BX_IA_CMP_EwsIb = 0x58;
  if (Op == BX_IA_CMP_AXIw || Op == BX_IA_CMP_EwIw || Op == BX_IA_CMP_EwsIb) {
    fmt::print("cmp with imm16 {:#x}\n", bochscpu_instr_imm16(Ins));
  }
}

void BochscpuBackend_t::OpcodeHlt(/*void *Context, */ uint32_t) {
  fmt::print("The emulator ran into a triple-fault exception or hit a HLT "
             "instruction.\n");
  fmt::print("If this is not an HLT instruction, please report it as a bug!\n");
  fmt::print("Stopping the cpu.\n");
  TestcaseResult_ = Crash_t();
  bochscpu_cpu_stop(Cpu_);
}

void BochscpuBackend_t::RecordEdge(/*void *Context, */ uint32_t Cpu,
                                   uint64_t Rip, uint64_t NextRip) {

  //
  // splitmix64 Rip, might be overkill, a single shift is probably sufficient
  // to avoid collisions?
  //

  uint64_t Edge = SplitMix64(Rip);

  //
  // XOR with NextRip.
  //

  Edge ^= NextRip;

  const auto &[_, NewCoverage] = AggregatedCodeCoverage_.emplace(Edge);
  if (NewCoverage) {
    LastNewCoverage_.emplace(Edge);
    RunStats_.NumberUniqueEdges++;
  }

  RunStats_.NumberEdges++;
}

bool BochscpuBackend_t::Restore(const CpuState_t &CpuState) {

  //
  // We keep the cr3 at the beginning to be able to know when it is getting
  // swapped.
  //

  InitialCr3_ = CpuState.Cr3;

  //
  // Load the state into the CPU.
  //

  LoadState(CpuState);

  //
  // Restore physical memory.
  //

  uint8_t ZeroPage[Page::Size];
  memset(ZeroPage, 0, sizeof(ZeroPage));
  for (const auto DirtyGpa : DirtyGpas_) {
    const uint8_t *Hva = DmpParser_.GetPhysicalPage(DirtyGpa.U64());

    //
    // As we allocate physical memory pages full of zeros when
    // the guest tries to access a GPA that isn't present in the dump,
    // we need to be able to restore those. It's easy, if the Hva is nullptr,
    // we point it to a zero page.
    //

    if (Hva == nullptr) {
      Hva = ZeroPage;
    }

    bochscpu_mem_phy_write(DirtyGpa.U64(), Hva, Page::Size);
  }

  //
  // Empty the set.
  //

  DirtyGpas_.clear();

  //
  // Close the trace file if we had one.
  //

  if (TraceFile_) {
    fclose(TraceFile_);
    TraceFile_ = nullptr;
    TraceType_ = TraceType_t::NoTrace;

    //
    // Empty the aggregated coverage set. When tracing we use it as a per-run
    // unique rips.
    //

    AggregatedCodeCoverage_.clear();
  }

  //
  // Reset the testcase result as well.
  //

  TestcaseResult_ = Ok_t();
  return true;
}

bool BochscpuBackend_t::SetTraceFile(const std::filesystem::path &TraceFile,
                                     const TraceType_t TraceType) {
  //
  // Open the trace file.
  //

  TraceFile_ = fopen(TraceFile.string().c_str(), "w");
  if (TraceFile_ == nullptr) {
    return false;
  }

  //
  // Keep track of the type of trace.
  //

  TraceType_ = TraceType;
  return true;
}

void BochscpuBackend_t::DirtyVirtualMemoryRange(const Gva_t Gva,
                                                const uint64_t Len) {

  //
  // Tracking the dirty GPAs. Note that bochscpu guarantees us
  // to get called twice if there's an access that straddles several
  // pages but external code doesn't guarantee that. So, to be safe
  // we iterate through the entire memory range and tag dirty pages.
  //

  const Gva_t EndGva = Gva + Gva_t(Len);
  const uint64_t Cr3 = bochscpu_cpu_cr3(Cpu_);
  for (Gva_t AlignedGva = Gva.Align(); AlignedGva < EndGva;
       AlignedGva += Gva_t(Page::Size)) {
    const Gpa_t AlignedGpa =
        Gpa_t(bochscpu_mem_virt_translate(Cr3, AlignedGva.U64()));
    BochsHooksDebugPrint(
        "DirtyVirtualMemoryRange: Adding GPA {:#x} to the dirty set..\n",
        AlignedGpa);

    if (AlignedGpa == Gpa_t(0xffffffffffffffff)) {
      fmt::print("Could not translate {:#x}\n", AlignedGva);
      __debugbreak();
    }

    DirtyGpa(AlignedGpa);
  }
}

void BochscpuBackend_t::DirtyPhysicalMemoryRange(const Gpa_t Gpa,
                                                 const uint64_t Len) {

  //
  // Tracking the dirty GPAs. Same comment as above applies here.
  //

  const Gpa_t EndGpa = Gpa + Gpa_t(Len);
  for (Gpa_t AlignedGpa = Gpa.Align(); AlignedGpa < EndGpa;
       AlignedGpa += Gpa_t(Page::Size)) {
    BochsHooksDebugPrint(
        "DirtyPhysicalMemoryRange: Adding GPA {:#x} to the dirty set..\n",
        AlignedGpa);
    DirtyGpa(AlignedGpa);
  }
}

const uint8_t *
BochscpuBackend_t::GetPhysicalPage(const Gpa_t PhysicalAddress) const {
  return DmpParser_.GetPhysicalPage(PhysicalAddress.U64());
}

void BochscpuBackend_t::Stop(const TestcaseResult_t &Res) {
  TestcaseResult_ = Res;
  bochscpu_cpu_stop(Cpu_);
}

uint64_t BochscpuBackend_t::Rdrand() {
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

bool BochscpuBackend_t::DirtyGpa(const Gpa_t Gpa) {
  return DirtyGpas_.emplace(Gpa.Align()).second;
}

bool BochscpuBackend_t::VirtTranslate(const Gva_t Gva, Gpa_t &Gpa,
                                      const MemoryValidate_t) const {
  const uint64_t Cr3 = bochscpu_cpu_cr3(Cpu_);
  Gpa = Gpa_t(bochscpu_mem_virt_translate(Cr3, Gva.U64()));
  return Gpa != Gpa_t(0xffffffffffffffff);
}

uint8_t *BochscpuBackend_t::PhysTranslate(const Gpa_t Gpa) const {
  return bochscpu_mem_phy_translate(Gpa.U64());
}

Gva_t BochscpuBackend_t::GetFirstVirtualPageToFault(const Gva_t Gva,
                                                    const size_t Size) {
  const uint64_t Cr3 = bochscpu_cpu_cr3(Cpu_);
  const Gva_t EndGva = Gva + Gva_t(Size);
  for (Gva_t AlignedGva = Gva.Align(); AlignedGva < EndGva;
       AlignedGva += Gva_t(Page::Size)) {
    const Gpa_t AlignedGpa =
        Gpa_t(bochscpu_mem_virt_translate(Cr3, AlignedGva.U64()));
    if (AlignedGpa == Gpa_t(0xffffffffffffffff))
      return AlignedGva;
  }

  return Gva_t(0xffffffffffffffff);
}

bool BochscpuBackend_t::PageFaultsMemoryIfNeeded(const Gva_t Gva,
                                                 const uint64_t Size) {

  //
  // The problem this function is solving is the following. Imagine that the
  // guest allocates memory, at which points the kernel sets-up the
  // appropriate page tables hierarchy but sets the leaf PTEs as non-present.
  // As usual, the memory management is lazy and you have to access the actual
  // pages for the page to be valid. This is actually documented in
  // 'VirtualAlloc' documentation for example: "Actual physical pages are not
  // allocated unless/until the virtual addresses are actually accessed."
  //
  // So ok. Now, the other piece of the puzzle is that we emulate / instrument
  // the guest systems in various ways. One of the thing we do a lot in
  // basically writing to the guest memory ourselves; it means it parses the
  // page tables and finds the backing page which we write to. This is a great
  // tool but if you consider the above case, then the leaf PTE structure
  // might not be present in which case you can't service the write. The idea
  // to solve this is to perform a memory translation of a virtual memory
  // range and check which pages are not translatable. When we encounter one
  // of those, we insert a #PF fault to have the kernel fix the PTE. That's
  // the idea.
  //
  // Cool - let's put things in perspective now. When one of our breakpoint
  // trigger, in bochs land we are in the `before_exec` hook. It means bochs
  // is in the process of executing an instruction. It is safe to inject any
  // number of page faults here because this could happen without us doing it;
  // imagine a `mov rax, [rcx]` instruction and the memory pointed by @rcx
  // hasn't been paged in yet. Well, it'll trigger a page fault, the kernel
  // fixes the PTE and bochs retries to re-executes the instruction. In the
  // above example, there's probably not going to be another #PF possible, but
  // you could imagine instructions where several ones could happen like a
  // 'movsb' for example where both the source and the destination are not
  // paged in. So anyways, this is good for us.
  //
  // Now the way the API works is that `bochscpu_cpu_set_exception` sets an
  // internal flag that gets read once we return from the hooks; then the
  // guest services the page fault and will try to re-execute the instruction
  // from the beginning. This means, our breakpoint triggers again but this
  // time hopefully the memory is paged in and we don't need to do anything.
  //
  // The way we achieve that is by trying to translate every GVA into a GPA,
  // if we can translate every pages in a range, then we bail because we have
  // no job to do. If the translation fails, `bochscpu_mem_virt_translate`
  // returns `0xffffffffffffffff` which is the signal we need to do some work.
  // When we see that, we know that we have a physical page of memory to page
  // in. So we set-up cr3 with the GVA that needs paging in, and dispatches
  // the exception.
  //
  // If you are wondering what happens if we have an entire range of memory to
  // page in, well after bochs does its work, the breakpoint triggers again at
  // which point we are going to be invoked to inspect the memory range and
  // will notice that the first page is now paged in, so we'll translate the
  // second one and notices now this one needs paging in. So we'll re-do the
  // same dance until the whole range is paged in at which point.
  //

  const Gva_t PageToFault = GetFirstVirtualPageToFault(Gva, Size);

  //
  // If we haven't found any GVA to fault-in then we have no job to do so we
  // return.
  //

  if (PageToFault == Gva_t(0xffffffffffffffff)) {
    return false;
  }

  //
  // At this point, we know we need to fault a page in.
  // Put the base GVA in cr2.
  //

  bochscpu_cpu_set_cr2(Cpu_, PageToFault.U64());

  //
  // Have bochs services the page fault.
  //

  const uint64_t PfVector = 14;
  bochscpu_cpu_set_exception(Cpu_, PfVector, ErrorWrite | ErrorUser);
  return true;
}

const tsl::robin_set<Gva_t> &BochscpuBackend_t::LastNewCoverage() const {
  return LastNewCoverage_;
}

bool BochscpuBackend_t::RevokeLastNewCoverage() {
  //
  // Revoking code coverage means removing it from the aggregated set.
  //

  for (const auto &Gva : LastNewCoverage_) {
    AggregatedCodeCoverage_.erase(Gva);
  }

  LastNewCoverage_.clear();
  return true;
}

bool BochscpuBackend_t::InsertCoverageEntry(const Gva_t Gva) {
  //
  // Inserting code coverage means adding it to the aggregated set.
  //

  const auto &Res = AggregatedCodeCoverage_.emplace(Gva);
  if (Res.second) {
    LastNewCoverage_.emplace(Gva);
  }

  return Res.second;
}

void BochscpuBackend_t::PrintRunStats() { RunStats_.Print(); }

void BochscpuBackend_t::IncCompcovUniqueHits() {
  RunStats_.NumberCompcovUniqueHits++;
}

const uint8_t *BochscpuBackend_t::GetTestcaseBuffer() {
  return TestcaseBuffer_;
}

uint64_t BochscpuBackend_t::GetTestcaseSize() { return TestcaseBufferSize_; }

void BochscpuBackend_t::LoadState(const CpuState_t &State) {
  bochscpu_cpu_state_t Bochs;
  memset(&Bochs, 0, sizeof(Bochs));

  Seed_ = State.Seed;
  Bochs.bochscpu_seed = State.Seed;
  Bochs.rax = State.Rax;
  Bochs.rbx = State.Rbx;
  Bochs.rcx = State.Rcx;
  Bochs.rdx = State.Rdx;
  Bochs.rsi = State.Rsi;
  Bochs.rdi = State.Rdi;
  Bochs.rip = State.Rip;
  Bochs.rsp = State.Rsp;
  Bochs.rbp = State.Rbp;
  Bochs.r8 = State.R8;
  Bochs.r9 = State.R9;
  Bochs.r10 = State.R10;
  Bochs.r11 = State.R11;
  Bochs.r12 = State.R12;
  Bochs.r13 = State.R13;
  Bochs.r14 = State.R14;
  Bochs.r15 = State.R15;
  Bochs.rflags = State.Rflags;
  Bochs.tsc = State.Tsc;
  Bochs.apic_base = State.ApicBase;
  Bochs.sysenter_cs = State.SysenterCs;
  Bochs.sysenter_esp = State.SysenterEsp;
  Bochs.sysenter_eip = State.SysenterEip;
  Bochs.pat = State.Pat;
  Bochs.efer = uint32_t(State.Efer.Flags);
  Bochs.star = State.Star;
  Bochs.lstar = State.Lstar;
  Bochs.cstar = State.Cstar;
  Bochs.sfmask = State.Sfmask;
  Bochs.kernel_gs_base = State.KernelGsBase;
  Bochs.tsc_aux = State.TscAux;
  Bochs.fpcw = State.Fpcw;
  Bochs.fpsw = State.Fpsw;
  Bochs.fptw = State.Fptw;
  Bochs.cr0 = uint32_t(State.Cr0.Flags);
  Bochs.cr2 = State.Cr2;
  Bochs.cr3 = State.Cr3;
  Bochs.cr4 = uint32_t(State.Cr4.Flags);
  Bochs.cr8 = State.Cr8;
  Bochs.xcr0 = State.Xcr0;
  Bochs.dr0 = State.Dr0;
  Bochs.dr1 = State.Dr1;
  Bochs.dr2 = State.Dr2;
  Bochs.dr3 = State.Dr3;
  Bochs.dr6 = State.Dr6;
  Bochs.dr7 = State.Dr7;
  Bochs.mxcsr = State.Mxcsr;
  Bochs.mxcsr_mask = State.MxcsrMask;
  Bochs.fpop = State.Fpop;

#define SEG(_Bochs_, _Whv_)                                                    \
  {                                                                            \
    Bochs._Bochs_.attr = State._Whv_.Attr;                                     \
    Bochs._Bochs_.base = State._Whv_.Base;                                     \
    Bochs._Bochs_.limit = State._Whv_.Limit;                                   \
    Bochs._Bochs_.present = State._Whv_.Present;                               \
    Bochs._Bochs_.selector = State._Whv_.Selector;                             \
  }

  SEG(es, Es);
  SEG(cs, Cs);
  SEG(ss, Ss);
  SEG(ds, Ds);
  SEG(fs, Fs);
  SEG(gs, Gs);
  SEG(tr, Tr);
  SEG(ldtr, Ldtr);

#undef SEG

#define GLOBALSEG(_Bochs_, _Whv_)                                              \
  {                                                                            \
    Bochs._Bochs_.base = State._Whv_.Base;                                     \
    Bochs._Bochs_.limit = State._Whv_.Limit;                                   \
  }

  GLOBALSEG(gdtr, Gdtr);
  GLOBALSEG(idtr, Idtr);

#undef GLOBALSEG

  for (uint64_t Idx = 0; Idx < 8; Idx++) {
    Bochs.fpst[Idx] = State.Fpst[Idx];
  }

  for (uint64_t Idx = 0; Idx < 10; Idx++) {
    memcpy(Bochs.zmm[Idx].q, State.Zmm[Idx].Q, sizeof(Zmm_t::Q));
  }

  bochscpu_cpu_set_state(Cpu_, &Bochs);
}

uint64_t BochscpuBackend_t::GetReg(const Registers_t Reg) {
  using BochscpuGetReg_t = uint64_t (*)(bochscpu_cpu_t);
  static const std::unordered_map<Registers_t, BochscpuGetReg_t>
      RegisterMappingGetters = {{Registers_t::Rax, bochscpu_cpu_rax},
                                {Registers_t::Rbx, bochscpu_cpu_rbx},
                                {Registers_t::Rcx, bochscpu_cpu_rcx},
                                {Registers_t::Rdx, bochscpu_cpu_rdx},
                                {Registers_t::Rsi, bochscpu_cpu_rsi},
                                {Registers_t::Rdi, bochscpu_cpu_rdi},
                                {Registers_t::Rip, bochscpu_cpu_rip},
                                {Registers_t::Rsp, bochscpu_cpu_rsp},
                                {Registers_t::Rbp, bochscpu_cpu_rbp},
                                {Registers_t::R8, bochscpu_cpu_r8},
                                {Registers_t::R9, bochscpu_cpu_r9},
                                {Registers_t::R10, bochscpu_cpu_r10},
                                {Registers_t::R11, bochscpu_cpu_r11},
                                {Registers_t::R12, bochscpu_cpu_r12},
                                {Registers_t::R13, bochscpu_cpu_r13},
                                {Registers_t::R14, bochscpu_cpu_r14},
                                {Registers_t::R15, bochscpu_cpu_r15},
                                {Registers_t::Rflags, bochscpu_cpu_rflags},
                                {Registers_t::Cr2, bochscpu_cpu_cr2},
                                {Registers_t::Cr3, bochscpu_cpu_cr3}};

  if (!RegisterMappingGetters.contains(Reg)) {
    fmt::print("There is no mapping for register {:x}.\n", Reg);
    __debugbreak();
  }

  const BochscpuGetReg_t &Getter = RegisterMappingGetters.at(Reg);
  return Getter(Cpu_);
}

uint64_t BochscpuBackend_t::SetReg(const Registers_t Reg,
                                   const uint64_t Value) {
  using BochscpuSetReg_t = void (*)(bochscpu_cpu_t, uint64_t);
  static const std::unordered_map<Registers_t, BochscpuSetReg_t>
      RegisterMappingSetters = {{Registers_t::Rax, bochscpu_cpu_set_rax},
                                {Registers_t::Rbx, bochscpu_cpu_set_rbx},
                                {Registers_t::Rcx, bochscpu_cpu_set_rcx},
                                {Registers_t::Rdx, bochscpu_cpu_set_rdx},
                                {Registers_t::Rsi, bochscpu_cpu_set_rsi},
                                {Registers_t::Rdi, bochscpu_cpu_set_rdi},
                                {Registers_t::Rip, bochscpu_cpu_set_rip},
                                {Registers_t::Rsp, bochscpu_cpu_set_rsp},
                                {Registers_t::Rbp, bochscpu_cpu_set_rbp},
                                {Registers_t::R8, bochscpu_cpu_set_r8},
                                {Registers_t::R9, bochscpu_cpu_set_r9},
                                {Registers_t::R10, bochscpu_cpu_set_r10},
                                {Registers_t::R11, bochscpu_cpu_set_r11},
                                {Registers_t::R12, bochscpu_cpu_set_r12},
                                {Registers_t::R13, bochscpu_cpu_set_r13},
                                {Registers_t::R14, bochscpu_cpu_set_r14},
                                {Registers_t::R15, bochscpu_cpu_set_r15},
                                {Registers_t::Rflags, bochscpu_cpu_set_rflags},
                                {Registers_t::Cr2, bochscpu_cpu_set_cr2},
                                {Registers_t::Cr3, bochscpu_cpu_set_cr3}};

  if (!RegisterMappingSetters.contains(Reg)) {
    fmt::print("There is no mapping for register {:x}.\n", Reg);
    __debugbreak();
  }

  const BochscpuSetReg_t &Setter = RegisterMappingSetters.at(Reg);
  Setter(Cpu_, Value);
  return Value;
}

[[nodiscard]] constexpr const char *
MemAccessToTenetLabel(const uint32_t MemAccess) {
  switch (MemAccess) {
  case BOCHSCPU_HOOK_MEM_READ: {
    return "mr";
  }

  case BOCHSCPU_HOOK_MEM_RW: {
    return "mrw";
  }

  case BOCHSCPU_HOOK_MEM_WRITE: {
    return "mw";
    break;
  }

  default: {
    fmt::print("Unexpected MemAccess type, aborting\n");
    std::abort();
  }
  }
}

void BochscpuBackend_t::DumpTenetDelta(const bool Force) {

  //
  // Dump register deltas.
  //

  bool NeedNewLine = false;

#define __DeltaRegister(Reg, Comma)                                            \
  {                                                                            \
    if (bochscpu_cpu_##Reg(Cpu_) != Tenet_.CpuStatePrev_.Reg || Force) {       \
      fmt::print(TraceFile_, #Reg "={:#x}", bochscpu_cpu_##Reg(Cpu_));         \
      NeedNewLine = true;                                                      \
      if (Comma) {                                                             \
        fmt::print(TraceFile_, ",");                                           \
      }                                                                        \
    }                                                                          \
  }

#define DeltaRegister(Reg) __DeltaRegister(Reg, true)
#define DeltaRegisterEnd(Reg) __DeltaRegister(Reg, false)

  DeltaRegister(rax);
  DeltaRegister(rbx);
  DeltaRegister(rcx);
  DeltaRegister(rdx);
  DeltaRegister(rbp);
  DeltaRegister(rsp);
  DeltaRegister(rsi);
  DeltaRegister(rdi);
  DeltaRegister(r8);
  DeltaRegister(r9);
  DeltaRegister(r10);
  DeltaRegister(r11);
  DeltaRegister(r12);
  DeltaRegister(r13);
  DeltaRegister(r14);
  DeltaRegister(r15);
  DeltaRegisterEnd(rip);

#undef DeltaRegisterEnd
#undef DeltaRegister
#undef __DeltaRegister

  //
  // Dump memory deltas.
  //

  for (const auto &AccessInfo : Tenet_.MemAccesses_) {

    //
    // Determine the label to use for this memory access.
    //

    const char *MemoryType = MemAccessToTenetLabel(AccessInfo.MemAccess);

    //
    // Fetch the memory that was read or written by the last executed
    // instruction. The largest load that can happen today is an AVX512
    // load which is 64 bytes long.
    //

    std::array<uint8_t, 64> Buffer;
    if (AccessInfo.Len > Buffer.size()) {
      fmt::print("A memory access was bigger than {} bytes, aborting\n",
                 AccessInfo.Len);
      std::abort();
    }

    if (!VirtRead(AccessInfo.VirtualAddress, Buffer.data(), AccessInfo.Len)) {
      fmt::print("VirtRead at {:#x} failed, aborting\n",
                 AccessInfo.VirtualAddress);
      std::abort();
    }

    //
    // Convert the raw memory bytes to a human-readable hex string.
    //

    std::string HexString;
    for (size_t Idx = 0; Idx < AccessInfo.Len; Idx++) {
      HexString = fmt::format("{}{:02X}", HexString, Buffer[Idx]);
    }

    //
    // Write the formatted memory access to file, eg
    // 'mr=0x140148040:0000000400080040'.
    //

    fmt::print(TraceFile_, ",{}={:#x}:{}", MemoryType,
               AccessInfo.VirtualAddress, HexString);

    NeedNewLine = true;
  }

  //
  // Clear out the saved memory accesses as they are no longer needed.
  //

  Tenet_.MemAccesses_.clear();

  //
  // End of deltas.
  //

  if (NeedNewLine) {
    fmt::print(TraceFile_, "\n");
  }
}
