// Axel '0vercl0k' Souchet - May 24 2020
#include "whv_backend.h"

#ifdef WINDOWS
#include "blake3.h"
#include "globals.h"
#include <algorithm>
#include <fstream>

constexpr bool WhvLoggingOn = false;

template <typename... Args_t>
void WhvDebugPrint(const char *Format, const Args_t &...args) {
  if constexpr (WhvLoggingOn) {
    fmt::print("whv: ");
    fmt::print(fmt::runtime(Format), args...);
  }
}

//
// Auto-initialization for WHV_REGISTER_VALUE.
//

struct WHV_REGISTER_VALUE_t {
  WHV_REGISTER_VALUE Value;
  WHV_REGISTER_VALUE_t() { memset(&Value, 0, sizeof(Value)); }
  WHV_REGISTER_VALUE *operator->() { return &Value; }
  WHV_REGISTER_VALUE *operator&() { return &Value; }
};

//
// Mapping between the backend registers and the WHV registers.
//

const std::unordered_map<Registers_t, WHV_REGISTER_NAME> RegisterMapping = {
    {Registers_t::Rax, WHvX64RegisterRax},
    {Registers_t::Rbx, WHvX64RegisterRbx},
    {Registers_t::Rcx, WHvX64RegisterRcx},
    {Registers_t::Rdx, WHvX64RegisterRdx},
    {Registers_t::Rsi, WHvX64RegisterRsi},
    {Registers_t::Rdi, WHvX64RegisterRdi},
    {Registers_t::Rip, WHvX64RegisterRip},
    {Registers_t::Rsp, WHvX64RegisterRsp},
    {Registers_t::Rbp, WHvX64RegisterRbp},
    {Registers_t::R8, WHvX64RegisterR8},
    {Registers_t::R9, WHvX64RegisterR9},
    {Registers_t::R10, WHvX64RegisterR10},
    {Registers_t::R11, WHvX64RegisterR11},
    {Registers_t::R12, WHvX64RegisterR12},
    {Registers_t::R13, WHvX64RegisterR13},
    {Registers_t::R14, WHvX64RegisterR14},
    {Registers_t::R15, WHvX64RegisterR15},
    {Registers_t::Rflags, WHvX64RegisterRflags},
    {Registers_t::Cr2, WHvX64RegisterCr2},
    {Registers_t::Cr3, WHvX64RegisterCr3}};

//
// WHVExitReason to string conversion.
//

const char *ExitReasonToStr(const WHV_RUN_VP_EXIT_REASON Reason) {
  switch (Reason) {
  case WHvRunVpExitReasonNone:
    return "WHvRunVpExitReasonNone";
  case WHvRunVpExitReasonMemoryAccess:
    return "WHvRunVpExitReasonMemoryAccess";
  case WHvRunVpExitReasonX64IoPortAccess:
    return "WHvRunVpExitReasonX64IoPortAccess";
  case WHvRunVpExitReasonUnrecoverableException:
    return "WHvRunVpExitReasonUnrecoverableException";
  case WHvRunVpExitReasonInvalidVpRegisterValue:
    return "WHvRunVpExitReasonInvalidVpRegisterValue";
  case WHvRunVpExitReasonUnsupportedFeature:
    return "WHvRunVpExitReasonUnsupportedFeature";
  case WHvRunVpExitReasonX64InterruptWindow:
    return "WHvRunVpExitReasonX64InterruptWindow";
  case WHvRunVpExitReasonX64Halt:
    return "WHvRunVpExitReasonX64Halt";
  case WHvRunVpExitReasonX64ApicEoi:
    return "WHvRunVpExitReasonX64ApicEoi";
  case WHvRunVpExitReasonX64MsrAccess:
    return "WHvRunVpExitReasonX64MsrAccess";
  case WHvRunVpExitReasonX64Cpuid:
    return "WHvRunVpExitReasonX64Cpuid";
  case WHvRunVpExitReasonException:
    return "WHvRunVpExitReasonException";
  case WHvRunVpExitReasonCanceled:
    return "WHvRunVpExitReasonCanceled";
  default:
    return "Unknown";
  }
}

WhvBackend_t::~WhvBackend_t() {
  if (Partition_ != nullptr) {
    WHvDeleteVirtualProcessor(Partition_, Vp_);
    WHvDeletePartition(Partition_);
  }
}

bool WhvBackend_t::Initialize(const Options_t &Opts,
                              const CpuState_t &CpuState) {
  //
  // Keep a copy of a few paths.
  //

  CoveragePath_ = Opts.CoveragePath;

  //
  // Create the partition object.
  //

  HRESULT Hr = WHvCreatePartition(&Partition_);
  if (FAILED(Hr)) {
    fmt::print(
        "Failed WHvCreatePartition (Windows Hypervisor Platform enabled?)\n");
    return false;
  }

  //
  // Configuration of the partition.
  //

  //
  // Add one VP to the partition.
  //

  Hr = SetPartitionProperty(WHvPartitionPropertyCodeProcessorCount, 1);
  if (FAILED(Hr)) {
    fmt::print("Failed SetPartitionProperty/ProcessorCount\n");
    return false;
  }

  //
  // Turn on extended VM-exits.
  //

  Hr = SetPartitionProperty(WHvPartitionPropertyCodeExtendedVmExits, 1);
  if (FAILED(Hr)) {
    fmt::print("Failed SetPartitionProperty/ExtendedVmExits\n");
    return false;
  }

  //
  // Configure the exit bitmap with the event we want to VM-exit on.
  //

  uint64_t ExceptionExitBitmap = 0;
  ExceptionExitBitmap |= 1ULL << WHvX64ExceptionTypeDivideErrorFault;
  ExceptionExitBitmap |= 1ULL << WHvX64ExceptionTypeDebugTrapOrFault;
  ExceptionExitBitmap |= 1ULL << WHvX64ExceptionTypeBreakpointTrap;
  ExceptionExitBitmap |= 1ULL << WHvX64ExceptionTypeOverflowTrap;
  ExceptionExitBitmap |= 1ULL << WHvX64ExceptionTypeBoundRangeFault;
  ExceptionExitBitmap |= 1ULL << WHvX64ExceptionTypeInvalidOpcodeFault;
  ExceptionExitBitmap |= 1ULL << WHvX64ExceptionTypeDeviceNotAvailableFault;
  ExceptionExitBitmap |= 1ULL << WHvX64ExceptionTypeDoubleFaultAbort;
  ExceptionExitBitmap |= 1ULL
                         << WHvX64ExceptionTypeInvalidTaskStateSegmentFault;
  ExceptionExitBitmap |= 1ULL << WHvX64ExceptionTypeSegmentNotPresentFault;
  ExceptionExitBitmap |= 1ULL << WHvX64ExceptionTypeStackFault;
  ExceptionExitBitmap |= 1ULL << WHvX64ExceptionTypeGeneralProtectionFault;
  ExceptionExitBitmap |= 1ULL << WHvX64ExceptionTypePageFault;
  ExceptionExitBitmap |= 1ULL << WHvX64ExceptionTypeFloatingPointErrorFault;
  ExceptionExitBitmap |= 1ULL << WHvX64ExceptionTypeAlignmentCheckFault;
  ExceptionExitBitmap |= 1ULL << WHvX64ExceptionTypeMachineCheckAbort;
  ExceptionExitBitmap |= 1ULL << WHvX64ExceptionTypeSimdFloatingPointFault;
  //
  // XXX: Enable if we can get a vmexit for failfast exception in the future?
  //

  // ExceptionExitBitmap |= 1ULL << WHvX64ExceptionTypeFailFast;

  //
  // Set the exit bitmap in the partition.
  //

  Hr = SetPartitionProperty(WHvPartitionPropertyCodeExceptionExitBitmap,
                            ExceptionExitBitmap);
  if (FAILED(Hr)) {
    fmt::print("Failed SetPartitionProperty/ExceptionExitBitmap\n");
    return false;
  }

  //
  // The partition is now ready, light it up.
  //

  Hr = WHvSetupPartition(Partition_);
  if (FAILED(Hr)) {
    fmt::print("Failed WHvSetupPartition\n");
    return false;
  }

  //
  // Create the VP.
  //

  Hr = WHvCreateVirtualProcessor(Partition_, Vp_, 0);
  if (FAILED(Hr)) {
    fmt::print("Failed WHvCreateVirtualProcessor\n");
    return false;
  }

  //
  // Load the CPU state. as well as memory.
  //

  Hr = LoadState(CpuState);
  if (FAILED(Hr)) {
    fmt::print("Failed to LoadState\n");
    return false;
  }

  //
  // Populate the memory inside the partition. We need to do that so that cr3 is
  // populated and we can properly translate GVAs into GPAs for setting our
  // breakpoints later.
  //

  Hr = PopulateMemory(Opts);
  if (FAILED(Hr)) {
    fmt::print("Failed to PopulateMemory\n");
    return false;
  }

  //
  // Set the code coverage breakpoints if we want a 'unique rip' trace or if
  // we are simply fuzzing.
  //

  if (!SetCoverageBps()) {
    fmt::print("Failed to SetCoverageBps\n");
    return false;
  }

  //
  // And we are done!
  //

  return true;
}

HRESULT
WhvBackend_t::SetPartitionProperty(
    const WHV_PARTITION_PROPERTY_CODE PropertyCode,
    const uint64_t PropertyValue) {
  WHV_PARTITION_PROPERTY Property;
  memset(&Property, 0, sizeof(Property));

  switch (PropertyCode) {
  case WHvPartitionPropertyCodeProcessorCount: {
    Property.ProcessorCount = uint32_t(PropertyValue);
    break;
  }

  case WHvPartitionPropertyCodeExtendedVmExits: {
    Property.ExtendedVmExits.ExceptionExit = PropertyValue;
    break;
  }

  case WHvPartitionPropertyCodeExceptionExitBitmap: {
    Property.ExceptionExitBitmap = PropertyValue;
    break;
  }

  default: {
    fmt::print("Property not implemented.\n");
    return E_FAIL;
  }
  }

  const HRESULT Hr = WHvSetPartitionProperty(Partition_, PropertyCode,
                                             &Property, sizeof(Property));
  return Hr;
}

HRESULT WhvBackend_t::LoadState(const CpuState_t &CpuState) {
  Seed_ = CpuState.Seed;
#define REG64(_Whv_)                                                           \
  {                                                                            \
    const HRESULT Hr = SetReg64(WHvX64Register##_Whv_, CpuState._Whv_);        \
    if (FAILED(Hr)) {                                                          \
      fmt::print("Setting " #_Whv_ " failed\n");                               \
      return Hr;                                                               \
    }                                                                          \
  }

#define REG64FLAGS(_Whv_)                                                      \
  {                                                                            \
    const HRESULT Hr = SetReg64(WHvX64Register##_Whv_, CpuState._Whv_.Flags);  \
    if (FAILED(Hr)) {                                                          \
      fmt::print("Setting " #_Whv_ " failed\n");                               \
      return Hr;                                                               \
    }                                                                          \
  }

  REG64(Rax);
  REG64(Rbx);
  REG64(Rcx);
  REG64(Rdx);
  REG64(Rsi);
  REG64(Rdi);
  REG64(Rip);
  REG64(Rsp);
  REG64(Rbp);
  REG64(R8);
  REG64(R9);
  REG64(R10);
  REG64(R11);
  REG64(R12);
  REG64(R13);
  REG64(R14);
  REG64(R15);
  REG64(Rflags);
  REG64(Tsc)
  REG64(ApicBase);
  REG64(SysenterCs);
  REG64(SysenterEsp);
  REG64(SysenterEip);
  REG64(Pat);
  REG64FLAGS(Efer);
  REG64(Star);
  REG64(Lstar);
  REG64(Cstar);
  REG64(Sfmask);
  REG64(KernelGsBase);
  REG64(TscAux);
  REG64FLAGS(Cr0);
  REG64(Cr2);
  REG64(Cr3);
  REG64FLAGS(Cr4);
  REG64(Cr8);
  {
    const HRESULT Hr = SetReg64(WHvX64RegisterXCr0, CpuState.Xcr0);
    if (FAILED(Hr)) {
      fmt::print("Setting Xcr0 failed\n");
      return Hr;
    }
  }
  REG64(Dr0);
  REG64(Dr1);
  REG64(Dr2);
  REG64(Dr3);
  REG64(Dr6);
  REG64(Dr7);
  {
    WHV_REGISTER_VALUE_t Reg;
    Reg->XmmControlStatus.XmmStatusControl = CpuState.Mxcsr;
    Reg->XmmControlStatus.XmmStatusControlMask = CpuState.MxcsrMask;
    const HRESULT Hr = SetRegister(WHvX64RegisterXmmControlStatus, &Reg);
    if (FAILED(Hr)) {
      fmt::print("Setting XmmControlStatus failed\n");
      return Hr;
    }
  }

  {
    WHV_REGISTER_VALUE_t Reg;
    Reg->FpControlStatus.LastFpOp = CpuState.Fpop;
    Reg->FpControlStatus.FpControl = CpuState.Fpcw;
    Reg->FpControlStatus.FpStatus = CpuState.Fpsw;
    Reg->FpControlStatus.FpTag = uint8_t(CpuState.Fptw);
    Reg->FpControlStatus.Reserved = uint8_t(CpuState.Fptw >> 8);
    const HRESULT Hr = SetRegister(WHvX64RegisterFpControlStatus, &Reg);
    if (FAILED(Hr)) {
      fmt::print("Setting FpControlStatus failed\n");
      return Hr;
    }
  }

#define REG128(_Whv_, _Wtf_)                                                   \
  {                                                                            \
    WHV_REGISTER_VALUE_t Reg;                                                  \
    Reg->Reg128.Low64 = CpuState._Wtf_;                                        \
    const HRESULT Hr = SetRegister(WHvX64Register##_Whv_, &Reg);               \
    if (FAILED(Hr)) {                                                          \
      fmt::print("Setting " #_Wtf_ " failed\n");                               \
      return Hr;                                                               \
    }                                                                          \
  }

  REG128(FpMmx0, Fpst[0]);
  REG128(FpMmx1, Fpst[1]);
  REG128(FpMmx2, Fpst[2]);
  REG128(FpMmx3, Fpst[3]);
  REG128(FpMmx4, Fpst[4]);
  REG128(FpMmx5, Fpst[5]);
  REG128(FpMmx6, Fpst[6]);
  REG128(FpMmx7, Fpst[7]);

#undef REG128
#define REG128(_Whv_, _Wtf_)                                                   \
  {                                                                            \
    WHV_REGISTER_VALUE_t Reg;                                                  \
    Reg->Reg128.Low64 = CpuState._Wtf_.Q[0];                                   \
    Reg->Reg128.High64 = CpuState._Wtf_.Q[1];                                  \
    const HRESULT Hr = SetRegister(WHvX64Register##_Whv_, &Reg);               \
    if (FAILED(Hr)) {                                                          \
      fmt::print("Setting " #_Wtf_ " failed\n");                               \
      return Hr;                                                               \
    }                                                                          \
  }

  REG128(Xmm0, Zmm[0]);
  REG128(Xmm1, Zmm[1]);
  REG128(Xmm2, Zmm[2]);
  REG128(Xmm3, Zmm[3]);
  REG128(Xmm4, Zmm[4]);
  REG128(Xmm5, Zmm[5]);
  REG128(Xmm6, Zmm[6]);
  REG128(Xmm7, Zmm[7]);
  REG128(Xmm8, Zmm[8]);
  REG128(Xmm9, Zmm[9]);
  REG128(Xmm10, Zmm[10]);
  REG128(Xmm11, Zmm[11]);
  REG128(Xmm12, Zmm[12]);
  REG128(Xmm13, Zmm[13]);
  REG128(Xmm14, Zmm[14]);
  REG128(Xmm15, Zmm[15]);

#undef REG128
#define SEGMENT(_Whv_)                                                         \
  {                                                                            \
    WHV_REGISTER_VALUE_t Reg;                                                  \
    Reg->Segment.Base = CpuState._Whv_.Base;                                   \
    Reg->Segment.Limit = CpuState._Whv_.Limit;                                 \
    Reg->Segment.Selector = CpuState._Whv_.Selector;                           \
    Reg->Segment.Attributes = CpuState._Whv_.Attr;                             \
    const HRESULT Hr = SetRegister(WHvX64Register##_Whv_, &Reg);               \
    if (FAILED(Hr)) {                                                          \
      fmt::print("Setting " #_Whv_ " failed\n");                               \
      return Hr;                                                               \
    }                                                                          \
  }

  SEGMENT(Es);
  SEGMENT(Cs);
  SEGMENT(Ss);
  SEGMENT(Ds);
  SEGMENT(Fs);
  SEGMENT(Gs);
  SEGMENT(Tr);
  SEGMENT(Ldtr);
#undef SEGMENT

#define GLOBALSEGMENT(_Whv_)                                                   \
  {                                                                            \
    WHV_REGISTER_VALUE_t Reg;                                                  \
    Reg->Table.Base = CpuState._Whv_.Base;                                     \
    Reg->Table.Limit = CpuState._Whv_.Limit;                                   \
    const HRESULT Hr = SetRegister(WHvX64Register##_Whv_, &Reg);               \
    if (FAILED(Hr)) {                                                          \
      fmt::print("Setting " #_Whv_ " failed\n");                               \
      return Hr;                                                               \
    }                                                                          \
  }

  GLOBALSEGMENT(Gdtr);
  GLOBALSEGMENT(Idtr);

  //
  // Ensure that there's no pending event.
  //

  {
    WHV_REGISTER_VALUE_t Reg;
    const HRESULT Hr = SetRegister(WHvRegisterPendingEvent, &Reg);
    if (FAILED(Hr)) {
      fmt::print("Setting PendingEvent failed\n");
      return Hr;
    }
  }

  return S_OK;
}

HRESULT WhvBackend_t::MapGpaRange(const uint8_t *Hva, const Gpa_t Gpa,
                                  const uint64_t RangeSize,
                                  const WHV_MAP_GPA_RANGE_FLAGS Flags) {
  const HRESULT Hr =
      WHvMapGpaRange(Partition_, (void *)Hva, Gpa.U64(), RangeSize, Flags);
  return Hr;
}

bool WhvBackend_t::SetCoverageBps() {
  //
  // If the user didn't provided a path or that the folder doesn't exist, then
  // we have nothing to do.
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

HRESULT WhvBackend_t::PopulateMemory(const Options_t &Opts) {

  //
  // Populate the RAM.
  //

  if (!Ram_.Populate(Opts.DumpPath)) {
    fmt::print("Failed to initialize the RAM\n");
    return E_FAIL;
  }

  //
  // Map it in the partition. We map the whole RAM as rx in the EPT.
  // This allows us to track memory writes into pages so that we can restore
  // them. When the partition tries to write to a page set as r-x in the EPT,
  // we get a fault that we handle by keeping track of tracking the dirty GPA
  // as well as remapping the page as rwx.
  //

  const WHV_MAP_GPA_RANGE_FLAGS Flags =
      WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagExecute;
  const HRESULT Hr = MapGpaRange(Ram_.Hva(), Gpa_t(0), Ram_.Size(), Flags);
  return Hr;
}

bool WhvBackend_t::SetBreakpoint(const Gva_t Gva,
                                 const BreakpointHandler_t Handler) {

  //
  // To set a breakpoint we need to know the GPA.
  //

  Gpa_t Gpa;
  const bool Translated =
      VirtTranslate(Gva, Gpa, MemoryValidate_t::ValidateReadExecute);
  if (!Translated) {
    fmt::print("GVA {:#x} translation failed.\n", Gva);
    return false;
  }

  //
  // Once we have it, we push the breakpoint/handler in our structures, and
  // set a breakpoint.
  //

  WhvBreakpoint_t Breakpoint(Gpa, Handler);
  if (Breakpoints_.contains(Gva)) {
    fmt::print("/!\\ There already is a breakpoint at {:#x}\n", Gva);
    return false;
  }

  Breakpoints_.emplace(Gva, Breakpoint);
  const uint8_t *Hva = Ram_.AddBreakpoint(Gpa);

  fmt::print("Resolved breakpoint {:#x} at GPA {:#x} aka HVA {}\n", Gva, Gpa,
             fmt::ptr(Hva));

  return true;
}

void WhvBackend_t::Stop(const TestcaseResult_t &Res) {
  TestcaseRes_ = Res;
  Stop_ = true;
}

HRESULT WhvBackend_t::GetRegisters(const WHV_REGISTER_NAME *Names,
                                   WHV_REGISTER_VALUE *Values,
                                   const uint32_t Numb) const {
  const HRESULT Hr =
      WHvGetVirtualProcessorRegisters(Partition_, Vp_, Names, Numb, Values);
  return Hr;
}

HRESULT WhvBackend_t::GetRegister(const WHV_REGISTER_NAME Name,
                                  WHV_REGISTER_VALUE *Value) const {
  return GetRegisters(&Name, Value, 1);
}

uint64_t WhvBackend_t::GetReg64(const WHV_REGISTER_NAME Name) const {
  WHV_REGISTER_VALUE_t Reg;
  const HRESULT Hr = GetRegister(Name, &Reg);
  if (FAILED(Hr)) {
    __debugbreak();
  }

  return Reg->Reg64;
}

HRESULT WhvBackend_t::SetRegisters(const WHV_REGISTER_NAME *Names,
                                   const WHV_REGISTER_VALUE *Values,
                                   const uint32_t Numb) {
  const HRESULT Hr =
      WHvSetVirtualProcessorRegisters(Partition_, Vp_, Names, Numb, Values);
  return Hr;
}

HRESULT WhvBackend_t::SetRegister(const WHV_REGISTER_NAME Name,
                                  const WHV_REGISTER_VALUE *Value) {
  const HRESULT Hr =
      WHvSetVirtualProcessorRegisters(Partition_, Vp_, &Name, 1, Value);
  return Hr;
}

HRESULT WhvBackend_t::SetReg64(const WHV_REGISTER_NAME Name,
                               const uint64_t Value) {
  WHV_REGISTER_VALUE_t Reg;
  Reg->Reg64 = Value;
  const HRESULT Hr =
      WHvSetVirtualProcessorRegisters(Partition_, Vp_, &Name, 1, &Reg);
  return Hr;
}

HRESULT
WhvBackend_t::SlowTranslateGva(const Gva_t Gva,
                               const WHV_TRANSLATE_GVA_FLAGS Flags,
                               WHV_TRANSLATE_GVA_RESULT &TranslationResult,
                               Gpa_t &Gpa) {
  const HRESULT Hr = WHvTranslateGva(Partition_, Vp_, Gva.U64(), Flags,
                                     &TranslationResult, &Gpa);
  return Hr;
}

HRESULT
WhvBackend_t::TranslateGva(const Gva_t Gva, const WHV_TRANSLATE_GVA_FLAGS,
                           WHV_TRANSLATE_GVA_RESULT &TranslationResult,
                           Gpa_t &Gpa) const {

  //
  // Stole most of the logic from @yrp604's code so thx bro.
  //

  const VIRTUAL_ADDRESS GuestAddress = Gva.U64();
  const MMPTE_HARDWARE Pml4 = GetReg64(WHvX64RegisterCr3);
  const uint64_t Pml4Base = Pml4.PageFrameNumber * Page::Size;
  const Gpa_t Pml4eGpa = Gpa_t(Pml4Base + GuestAddress.Pml4Index * 8);
  const MMPTE_HARDWARE Pml4e = PhysRead8(Pml4eGpa);
  if (!Pml4e.Present) {
    TranslationResult.ResultCode = WHvTranslateGvaResultPageNotPresent;
    return S_OK;
  }

  const uint64_t PdptBase = Pml4e.PageFrameNumber * Page::Size;
  const Gpa_t PdpteGpa = Gpa_t(PdptBase + GuestAddress.PdPtIndex * 8);
  const MMPTE_HARDWARE Pdpte = PhysRead8(PdpteGpa);
  if (!Pdpte.Present) {
    TranslationResult.ResultCode = WHvTranslateGvaResultPageNotPresent;
    return S_OK;
  }

  //
  // huge pages:
  // 7 (PS) - Page size; must be 1 (otherwise, this entry references a page
  // directory; see Table 4-1
  //

  const uint64_t PdBase = Pdpte.PageFrameNumber * Page::Size;
  if (Pdpte.LargePage) {
    TranslationResult.ResultCode = WHvTranslateGvaResultSuccess;
    Gpa = Gpa_t(PdBase + (Gva.U64() & 0x3fff'ffff));
    return S_OK;
  }

  const Gpa_t PdeGpa = Gpa_t(PdBase + GuestAddress.PdIndex * 8);
  const MMPTE_HARDWARE Pde = PhysRead8(PdeGpa);
  if (!Pde.Present) {
    TranslationResult.ResultCode = WHvTranslateGvaResultPageNotPresent;
    return S_OK;
  }

  //
  // large pages:
  // 7 (PS) - Page size; must be 1 (otherwise, this entry references a page
  // table; see Table 4-18
  //

  const uint64_t PtBase = Pde.PageFrameNumber * Page::Size;
  if (Pde.LargePage) {
    TranslationResult.ResultCode = WHvTranslateGvaResultSuccess;
    Gpa = Gpa_t(PtBase + (Gva.U64() & 0x1f'ffff));
    return S_OK;
  }

  const Gpa_t PteGpa = Gpa_t(PtBase + GuestAddress.PtIndex * 8);
  const MMPTE_HARDWARE Pte = PhysRead8(PteGpa);
  if (!Pte.Present) {
    TranslationResult.ResultCode = WHvTranslateGvaResultPageNotPresent;
    return S_OK;
  }

  TranslationResult.ResultCode = WHvTranslateGvaResultSuccess;
  const uint64_t PageBase = Pte.PageFrameNumber * 0x1000;
  Gpa = Gpa_t(PageBase + GuestAddress.Offset);
  return S_OK;
}

bool WhvBackend_t::DirtyGpa(const Gpa_t Gpa) {
  return DirtyGpas_.emplace(Gpa.Align()).second;
}

bool WhvBackend_t::VirtTranslate(const Gva_t Gva, Gpa_t &Gpa,
                                 const MemoryValidate_t Validate) const {
  WHV_TRANSLATE_GVA_FLAGS Flags = WHvTranslateGvaFlagNone;
  if (Validate & MemoryValidate_t::ValidateRead) {
    Flags |= WHvTranslateGvaFlagValidateRead;
  }

  if (Validate & MemoryValidate_t::ValidateWrite) {
    Flags |= WHvTranslateGvaFlagValidateWrite;
  }

  if (Validate & MemoryValidate_t::ValidateExecute) {
    Flags |= WHvTranslateGvaFlagValidateExecute;
  }

  WHV_TRANSLATE_GVA_RESULT TranslationResult;
  if (FAILED(TranslateGva(Gva, Flags, TranslationResult, Gpa))) {
    return false;
  }

  return TranslationResult.ResultCode == WHvTranslateGvaResultSuccess;
}

bool WhvBackend_t::PhysRead(const Gpa_t Gpa, uint8_t *Buffer,
                            const uint64_t BufferSize) const {
  const uint8_t *Src = PhysTranslate(Gpa);
  memcpy(Buffer, Src, BufferSize);
  return true;
}

uint64_t WhvBackend_t::PhysRead8(const Gpa_t Gpa) const {
  uint64_t Qword;
  if (!PhysRead(Gpa, (uint8_t *)&Qword, sizeof(Qword))) {
    __debugbreak();
  }
  return Qword;
}

void WhvBackend_t::SetLimit(const uint64_t Limit) { Limit_ = uint32_t(Limit); }

HRESULT
WhvBackend_t::RunProcessor(WHV_RUN_VP_EXIT_CONTEXT &ExitContext) {
  const HRESULT Hr = WHvRunVirtualProcessor(Partition_, Vp_, &ExitContext,
                                            sizeof(ExitContext));
  return Hr;
}

std::optional<TestcaseResult_t> WhvBackend_t::Run(const uint8_t *Buffer,
                                                  const uint64_t BufferSize) {

  //
  // Reset our state.
  //

  TestcaseBuffer_ = Buffer;
  TestcaseBufferSize_ = BufferSize;
  Stop_ = false;
  TestcaseRes_ = Ok_t();
  Coverage_.clear();

  //
  // Configure a timer that will cancel the VP in case it runs for too long.
  //

  Timer_.SetTimer(Limit_);

  //
  // Let's go!
  //

  while (!Stop_) {

    //
    // Run the VP.
    //

    WHV_RUN_VP_EXIT_CONTEXT ExitContext;
    HRESULT Hr = RunProcessor(ExitContext);

    if (FAILED(Hr)) {
      fmt::print("Failed to RunProcessor\n");
      return {};
    }

    RunStats_.Vmexits++;

    switch (ExitContext.ExitReason) {
    case WHvRunVpExitReasonException: {

      //
      // We received an exception from the guest; could be a debug exception,
      // or a fault.
      //

      Hr = OnExitReasonException(ExitContext);
      break;
    }

    case WHvRunVpExitReasonMemoryAccess: {

      //
      // We received a memory/EPT fault; could be a write access to a read
      // page.
      //

      Hr = OnExitReasonMemoryAccess(ExitContext);
      break;
    }

    case WHvRunVpExitReasonCanceled: {

      //
      // If somebody cancelled us, let's make sure we stop the testcase.
      //

      Stop_ = true;
      TestcaseRes_ = Timedout_t();
      break;
    }

    default: {

      //
      // If we get there, it means something is not quite right.. so we want
      // to know about it.
      //

      fmt::print("WHvRunVirtualProcessor exited with {}\n",
                 ExitReasonToStr(ExitContext.ExitReason));
      PrintRegisters();
      std::abort();
      Stop_ = true;
      TestcaseRes_ = Crash_t();
      break;
    }
    }

    //
    // In case one of our handlers failed, let's stop here too.
    //

    if (FAILED(Hr)) {
      fmt::print("One of the handler failed\n");
      Stop_ = true;
    }
  }

  //
  // Don't forget to kill the timer.
  //

  Timer_.TerminateLastTimer();

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

bool WhvBackend_t::Restore(const CpuState_t &CpuState) {

  if (FAILED(LoadState(CpuState))) {
    return false;
  }

  //
  // Walk the dirty GPAs to restore their content.
  //

  for (const auto &DirtyGpa : DirtyGpas_) {
    const uint8_t *Hva = Ram_.Restore(DirtyGpa);

    //
    // Dirty pages have been remapped as rwx pages, so we need to map them
    // back to r-x.
    //

    const WHV_MAP_GPA_RANGE_FLAGS Flags =
        WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagExecute;
    const HRESULT Hr = MapGpaRange(Hva, DirtyGpa, Page::Size, Flags);

    if (FAILED(Hr)) {
      return false;
    }
  }

  RunStats_.Dirty = DirtyGpas_.size();

  DirtyGpas_.clear();
  return true;
}

HRESULT
WhvBackend_t::OnExitCoverageBp(const WHV_RUN_VP_EXIT_CONTEXT &Exception) {
  //
  // Get the GPA of the breakpoint.
  //

  const Gva_t Rip = Gva_t(Exception.VpContext.Rip);
  const Gpa_t Gpa = CovBreakpoints_.at(Rip);

  //
  // Remove the breakpoint from RAM.
  //

  Ram_.RemoveBreakpoint(Gpa);

  //
  // Log it if we are expected to.
  //

  if (TraceType_ == TraceType_t::UniqueRip) {
    fmt::print(TraceFile_, "{:#x}\n", Rip);
  }

  //
  // Remove the breakpoint from the coverage breakpoints as it just got hit.
  //

  CovBreakpoints_.erase(Rip);

  //
  // New breakpoint means new coverage.
  //

  Coverage_.emplace(Rip);
  return S_OK;
}

HRESULT
WhvBackend_t::OnBreakpointTrap(const WHV_RUN_VP_EXIT_CONTEXT &Exception) {
  //
  // First let's handle the case where the breakpoint is a coverage
  // breakpoint.
  //

  const Gva_t Rip = Gva_t(Exception.VpContext.Rip);
  const bool CoverageBp = CovBreakpoints_.contains(Rip);
  const bool IsBreakpoint = Breakpoints_.contains(Rip);

  if (!CoverageBp && !IsBreakpoint) {
    SaveCrash(Rip, EXCEPTION_BREAKPOINT);
    return S_OK;
  }

  //
  // Is it a coverage breakpoint?
  //

  if (CoverageBp) {
    const HRESULT Hr = OnExitCoverageBp(Exception);
    if (FAILED(Hr)) {
      return Hr;
    }
  }

  //
  // If this was JUST a coverage breakpoint, then we are done here as we have
  // nothing more to do.
  //

  if (!IsBreakpoint) {
    return S_OK;
  }

  //
  // Well this was also a normal breakpoint.. so we need to invoke the
  // handler.
  //

  WhvBreakpoint_t &Breakpoint = Breakpoints_.at(Rip);
  Breakpoint.Handler(this);

  //
  // If we hit a coverage breakpoint right before, it means that the 0xcc has
  // been removed
  // and restored by the original byte. In that case, we need to re-arm it,
  // otherwise we lose the breakpoint.
  //

  if (CoverageBp) {
    Ram_.AddBreakpoint(Breakpoint.Gpa);
  }

  const WHV_REGISTER_NAME Names[3] = {WHvX64RegisterRflags, WHvX64RegisterRip,
                                      WHvRegisterPendingEvent};
  WHV_REGISTER_VALUE Regs[3];
  WHV_REGISTER_VALUE *Rflags = &Regs[0], *NewRip = &Regs[1],
                     *PendingEvent = &Regs[2];
  HRESULT Hr = GetRegisters(Names, Regs, 3);
  if (FAILED(Hr)) {
    fmt::print("GetRegisters failed\n");
    return Hr;
  }

  //
  // If the breakpoint handler moved @rip, injected a pending pf or asked to
  // stop the testcase, then no need to single-step over the instruction.
  //

  if (NewRip->Reg64 != Rip.U64() ||
      (PendingEvent->ExceptionEvent.EventPending &&
       PendingEvent->ExceptionEvent.Vector == WHvX64ExceptionTypePageFault) ||
      Stop_) {
    return Hr;
  }

  //
  // Sounds like we need to step-over the instruction after all.
  // To do that we need to:
  //   - disarm the breakpoint,
  //   - turn on TF to step over the instruction
  // We'll take a fault and re-arm the breakpoint once we stepped over the
  // instruction.
  //

  WhvDebugPrint("Disarming bp and turning on RFLAGS.TF\n");
  LastBreakpointGpa_ = Breakpoint.Gpa;
  Ram_.RemoveBreakpoint(Breakpoint.Gpa);

  const uint64_t NewRflags = Exception.VpContext.Rflags | RFLAGS_TRAP_FLAG_FLAG;
  Rflags->Reg64 = NewRflags;
  Hr = SetRegisters(Names, Regs, 2);
  return Hr;
}

HRESULT
WhvBackend_t::OnDebugTrap(const WHV_RUN_VP_EXIT_CONTEXT &Exception) {
  uint64_t Rflags = Exception.VpContext.Rflags;

  //
  // We have a pending debug trap exception and we have previously unset a
  // breakpoint. In that case, we need to do the dance to rearm the breakpoint
  // to continue execution.
  //

  if (LastBreakpointGpa_ == Gpa_t(0xffffffffffffffff)) {
    fmt::print(
        "Got into OnDebugTrap with LastBreakpointGpa_ = 0xffffffffffffffff");
    std::abort();
  }

  //
  // Remember if we get there, it is because we hit a breakpoint, turned on
  // TF in order to step over the instruction, and now we get the chance to
  // turn it back on.
  //

  Ram_.AddBreakpoint(LastBreakpointGpa_);

  //
  // Strip TF off Rflags.
  //

  WhvDebugPrint("Turning off RFLAGS.TF\n");
  Rflags &= ~RFLAGS_TRAP_FLAG_FLAG;
  LastBreakpointGpa_ = Gpa_t(0xffffffffffffffff);

  return SetReg64(WHvX64RegisterRflags, Rflags);
}

HRESULT
WhvBackend_t::OnExitReasonException(const WHV_RUN_VP_EXIT_CONTEXT &Exception) {

  //
  // All right - let's untangle this shit boys.
  // We can end up in this function when:
  //   - We triggered a coverage breakpoint/execution breakpoint,
  //   - We step over an instruction and we get a debug exception,
  //   - We hit a crash.
  //

  switch (Exception.VpException.ExceptionType) {
  case WHvX64ExceptionTypeBreakpointTrap: {
    return OnBreakpointTrap(Exception);
  }

  case WHvX64ExceptionTypeDebugTrapOrFault: {
    return OnDebugTrap(Exception);
  }

  case WHvX64ExceptionTypeDivideErrorFault: {
    SaveCrash(Gva_t(Exception.VpContext.Rip), EXCEPTION_INT_DIVIDE_BY_ZERO);
    return S_OK;
  }

    //
    // XXX: If one day we get a vmexit for failfast exception, add the below:
    // SaveCrash(Exception.VpContext.Rip, STATUS_STACK_BUFFER_OVERRUN);
    //

  case WHvX64ExceptionTypePageFault: {
    RunStats_.PageFaults++;
    [[fallthrough]];
  }

  default: {
    WhvDebugPrint(
        "Received a {:#x} exception, letting the guest figure it out..\n",
        Exception.VpException.ExceptionType);

    //
    // If we haven't handled the fault, let's reinject it into the guest.
    //

    WHV_REGISTER_VALUE_t Reg;
    Reg->ExceptionEvent.EventPending = 1;
    Reg->ExceptionEvent.EventType = WHvX64PendingEventException;
    Reg->ExceptionEvent.DeliverErrorCode = 1;
    Reg->ExceptionEvent.Vector = Exception.VpException.ExceptionType;
    Reg->ExceptionEvent.ErrorCode = Exception.VpException.ErrorCode;
    Reg->ExceptionEvent.ExceptionParameter =
        Exception.VpException.ExceptionParameter;

    return SetRegister(WHvRegisterPendingEvent, &Reg);
  }
  }
}

HRESULT
WhvBackend_t::OnExitReasonMemoryAccess(
    const WHV_RUN_VP_EXIT_CONTEXT &Exception) {
  const Gpa_t Gpa = Gpa_t(Exception.MemoryAccess.Gpa);
  const bool WriteAccess =
      Exception.MemoryAccess.AccessInfo.AccessType == WHvMemoryAccessWrite;

  if (!WriteAccess) {
    fmt::print("Dont know how to handle this fault, exiting.\n");
    __debugbreak();
    return E_FAIL;
  }

  //
  // Remap the page as writeable.
  //

  const WHV_MAP_GPA_RANGE_FLAGS Flags = WHvMapGpaRangeFlagWrite |
                                        WHvMapGpaRangeFlagRead |
                                        WHvMapGpaRangeFlagExecute;

  const Gpa_t AlignedGpa = Gpa.Align();
  DirtyGpa(AlignedGpa);

  uint8_t *AlignedHva = PhysTranslate(AlignedGpa);
  return MapGpaRange(AlignedHva, AlignedGpa, Page::Size, Flags);
}

Gva_t WhvBackend_t::GetFirstVirtualPageToFault(const Gva_t Gva,
                                               const uint64_t Size) {
  const Gva_t EndGva = Gva + Gva_t(Size);
  for (Gva_t AlignedGva = Gva.Align(); AlignedGva < EndGva;
       AlignedGva += Gva_t(Page::Size)) {
    WHV_TRANSLATE_GVA_RESULT TranslationResult;
    Gpa_t AlignedGpa;
    if (FAILED(TranslateGva(AlignedGva, WHvTranslateGvaFlagValidateRead,
                            TranslationResult, AlignedGpa))) {
      __debugbreak();
    }

    const bool NonPresentPage =
        TranslationResult.ResultCode == WHvTranslateGvaResultPageNotPresent;

    if (NonPresentPage) {
      return AlignedGva;
    }

    if (TranslationResult.ResultCode != WHvTranslateGvaResultSuccess) {
      __debugbreak();
    }
  }

  return Gva_t(0xffffffffffffffff);
}

bool WhvBackend_t::PageFaultsMemoryIfNeeded(const Gva_t Gva,
                                            const uint64_t Size) {
  const Gva_t PageToFault = GetFirstVirtualPageToFault(Gva, Size);

  //
  // If we haven't found any GVA to fault-in then we have no job to do so we
  // return.
  //

  if (PageToFault == Gva_t(0xffffffffffffffff)) {
    return false;
  }

  WhvDebugPrint("Inserting page fault for GVA {:#x}\n", PageToFault);

  // cf 'VM-Entry Controls for Event Injection' in Intel 3C
  WHV_REGISTER_VALUE_t Exception;
  Exception->ExceptionEvent.EventPending = 1;
  Exception->ExceptionEvent.EventType = WHvX64PendingEventException;
  Exception->ExceptionEvent.DeliverErrorCode = 1;
  Exception->ExceptionEvent.Vector = WHvX64ExceptionTypePageFault;
  Exception->ExceptionEvent.ErrorCode = ErrorWrite | ErrorUser;
  Exception->ExceptionEvent.ExceptionParameter = PageToFault.U64();

  if (FAILED(SetRegister(WHvRegisterPendingEvent, &Exception))) {
    __debugbreak();
  }

  return true;
}

uint64_t WhvBackend_t::Rdrand() {
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

void WhvBackend_t::CancelRunVirtualProcessor() {
  fmt::print("Interrupting the virtual processor..\n");
  WHvCancelRunVirtualProcessor(Partition_, Vp_, 0);
}

const uint8_t *WhvBackend_t::GetTestcaseBuffer() { return TestcaseBuffer_; }

const uint64_t WhvBackend_t::GetTestcaseSize() { return TestcaseBufferSize_; }

const tsl::robin_set<Gva_t> &WhvBackend_t::LastNewCoverage() const {
  return Coverage_;
}

bool WhvBackend_t::RevokeLastNewCoverage() {

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

bool WhvBackend_t::InsertCoverageEntry(const Gva_t Gva) {
  // TODO: implement

  throw std::runtime_error(__func__ + std::string("is not implemented"));

  return false;
}

void WhvBackend_t::PrintRunStats() { RunStats_.Print(); }

uint8_t *WhvBackend_t::PhysTranslate(const Gpa_t Gpa) const {
  return Ram_.Hva() + Gpa.U64();
}

bool WhvBackend_t::SetTraceFile(const fs::path &TestcaseTracePath,
                                const TraceType_t TraceType) {

  if (TraceType == TraceType_t::Rip) {
    fmt::print("Rip traces can be only generated with bochscpu.\n");
    std::abort();
  }

  //
  // Open the trace file.
  //

  TraceType_ = TraceType;
  TraceFile_ = fopen(TestcaseTracePath.string().c_str(), "w");
  if (TraceFile_ == nullptr) {
    return false;
  }

  return true;
}

uint64_t WhvBackend_t::GetReg(const Registers_t Reg) {
  if (!RegisterMapping.contains(Reg)) {
    fmt::print("There is no mapping for register {:#x}.\n", Reg);
    __debugbreak();
  }

  return GetReg64(RegisterMapping.at(Reg));
}

uint64_t WhvBackend_t::SetReg(const Registers_t Reg, const uint64_t Value) {
  if (!RegisterMapping.contains(Reg)) {
    fmt::print("There is no mapping for register {:#x}.\n", Reg);
    __debugbreak();
  }

  if (FAILED(SetReg64(RegisterMapping.at(Reg), Value))) {
    __debugbreak();
  }

  return Value;
}

#endif
