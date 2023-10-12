// Axel '0vercl0k' Souchet - March 29 2020
#pragma once
#include <cstring>
#include <filesystem>
#include <fmt/format.h>
#include <random>
#include <string>
#include <string_view>
#include <variant>

#include "gxa.h"

//
// warning C4201: nonstandard extension used: nameless struct/union
//

#pragma warning(disable : 4201)

namespace fs = std::filesystem;

struct Zmm_t {
  uint64_t Q[8];

  Zmm_t() { memset(this, 0, sizeof(decltype(*this))); }

  bool operator==(const Zmm_t &B) const {
    bool Equal = true;
    for (size_t Idx = 0; Idx < 8; Idx++) {
      Equal = Equal && Q[Idx] == B.Q[Idx];
    }
    return Equal;
  }
};

struct Seg_t {
  uint16_t Selector;
  uint64_t Base;
  uint32_t Limit;
  union {
    struct {
      uint16_t SegmentType : 4;
      uint16_t NonSystemSegment : 1;
      uint16_t DescriptorPrivilegeLevel : 2;
      uint16_t Present : 1;
      uint16_t Reserved : 4;
      uint16_t Available : 1;
      uint16_t Long : 1;
      uint16_t Default : 1;
      uint16_t Granularity : 1;
    };

    uint16_t Attr;
  };

  Seg_t() { memset(this, 0, sizeof(decltype(*this))); }

  bool operator==(const Seg_t &B) const {
    bool Equal = Attr == B.Attr;
    Equal = Equal && Base == B.Base;
    Equal = Equal && Limit == B.Limit;
    Equal = Equal && Present == B.Present;
    Equal = Equal && Selector == B.Selector;
    return Equal;
  }
};

struct GlobalSeg_t {
  uint64_t Base;
  uint16_t Limit;

  GlobalSeg_t() { memset(this, 0, sizeof(decltype(*this))); }

  bool operator==(const GlobalSeg_t &B) const {
    bool Equal = Base == B.Base;
    Equal = Equal && Limit == B.Limit;
    return Equal;
  }
};

//
// This is stolen from linux/v5.7.2/source/arch/x86/include/asm/apicdef.h
//

constexpr uint32_t APIC_DEFAULT_PHYS_BASE = 0xfee00000;
constexpr uint32_t APIC_SPIV = 0xF0;
constexpr uint32_t APIC_LVTPC = 0x340;
constexpr uint32_t APIC_MODE_FIXED = 0x0;
constexpr uint32_t APIC_MODE_NMI = 0x4;
constexpr uint32_t APIC_MODE_EXTINT = 0x7;

//
// This has been stolen from ia32-docs's ia32.h:
//   https://github.com/wbenny/ia32-doc/blob/master/out/ia32.h
//

union Cr0_t {
  Cr0_t() { Flags = 0; }

  Cr0_t(const uint64_t Value) { Flags = Value; }

  bool operator==(const Cr0_t &B) const { return Flags == B.Flags; }

  void Print() const {
    fmt::print("CR0: {:#x}\n", Flags);
    fmt::print("CR0.ProtectionEnable: {}\n", ProtectionEnable);
    fmt::print("CR0.MonitorCoprocessor: {}\n", MonitorCoprocessor);
    fmt::print("CR0.EmulateFpu: {}\n", EmulateFpu);
    fmt::print("CR0.TaskSwitched: {}\n", TaskSwitched);
    fmt::print("CR0.ExtensionType: {}\n", ExtensionType);
    fmt::print("CR0.NumericError: {}\n", NumericError);
    fmt::print("CR0.WriteProtect: {}\n", WriteProtect);
    fmt::print("CR0.AlignmentMask: {}\n", AlignmentMask);
    fmt::print("CR0.NotWriteThrough: {}\n", NotWriteThrough);
    fmt::print("CR0.CacheDisable: {}\n", CacheDisable);
    fmt::print("CR0.PagingEnable: {}\n", PagingEnable);
  }

  struct {
    /**
     * @brief Protection Enable
     *
     * [Bit 0] Enables protected mode when set; enables real-address mode when
     * clear. This flag does not enable paging directly. It only enables
     * segment-level protection. To enable paging, both the PE and PG flags must
     * be set.
     *
     * @see Vol3A[9.9(Mode Switching)]
     */
    uint64_t ProtectionEnable : 1;
#define CR0_PROTECTION_ENABLE_BIT 0
#define CR0_PROTECTION_ENABLE_FLAG 0x01
#define CR0_PROTECTION_ENABLE(_) (((_) >> 0) & 0x01)

    /**
     * @brief Monitor Coprocessor
     *
     * [Bit 1] Controls the interaction of the WAIT (or FWAIT) instruction with
     * the TS flag (bit 3 of CR0). If the MP flag is set, a WAIT instruction
     * generates a device-not-available exception (\#NM) if the TS flag is also
     * set. If the MP flag is clear, the WAIT instruction ignores the setting of
     * the TS flag.
     */
    uint64_t MonitorCoprocessor : 1;
#define CR0_MONITOR_COPROCESSOR_BIT 1
#define CR0_MONITOR_COPROCESSOR_FLAG 0x02
#define CR0_MONITOR_COPROCESSOR(_) (((_) >> 1) & 0x01)

    /**
     * @brief FPU Emulation
     *
     * [Bit 2] Indicates that the processor does not have an internal or
     * external x87 FPU when set; indicates an x87 FPU is present when clear.
     * This flag also affects the execution of MMX/SSE/SSE2/SSE3/SSSE3/SSE4
     * instructions. When the EM flag is set, execution of an x87 FPU
     * instruction generates a device-not-available exception (\#NM). This flag
     * must be set when the processor does not have an internal x87 FPU or is
     * not connected to an external math coprocessor. Setting this flag forces
     * all floating-point instructions to be handled by software emulation.
     * Also, when the EM flag is set, execution of an MMX instruction causes an
     * invalid-opcode exception (\#UD) to be generated. Thus, if an IA-32 or
     * Intel 64 processor incorporates MMX technology, the EM flag must be set
     * to 0 to enable execution of MMX instructions. Similarly for
     * SSE/SSE2/SSE3/SSSE3/SSE4 extensions, when the EM flag is set, execution
     * of most SSE/SSE2/SSE3/SSSE3/SSE4 instructions causes an invalid opcode
     * exception (\#UD) to be generated. If an IA-32 or Intel 64 processor
     * incorporates the SSE/SSE2/SSE3/SSSE3/SSE4 extensions, the EM flag must be
     * set to 0 to enable execution of these extensions.
     * SSE/SSE2/SSE3/SSSE3/SSE4 instructions not affected by the EM flag
     * include: PAUSE, PREFETCHh, SFENCE, LFENCE, MFENCE, MOVNTI, CLFLUSH,
     * CRC32, and POPCNT.
     */
    uint64_t EmulateFpu : 1;
#define CR0_EMULATE_FPU_BIT 2
#define CR0_EMULATE_FPU_FLAG 0x04
#define CR0_EMULATE_FPU(_) (((_) >> 2) & 0x01)

    /**
     * @brief Task Switched
     *
     * [Bit 3] Allows the saving of the x87 FPU/MMX/SSE/SSE2/SSE3/SSSE3/SSE4
     * context on a task switch to be delayed until an x87
     * FPU/MMX/SSE/SSE2/SSE3/SSSE3/SSE4 instruction is actually executed by the
     * new task. The processor sets this flag on every task switch and tests it
     * when executing x87 FPU/MMX/SSE/SSE2/SSE3/SSSE3/SSE4 instructions.
     * - If the TS flag is set and the EM flag (bit 2 of CR0) is clear, a
     * device-not-available exception (\#NM) is raised prior to the execution of
     * any x87 FPU/MMX/SSE/SSE2/SSE3/SSSE3/SSE4 instruction; with the exception
     * of PAUSE, PREFETCHh, SFENCE, LFENCE, MFENCE, MOVNTI, CLFLUSH, CRC32, and
     * POPCNT.
     * - If the TS flag is set and the MP flag (bit 1 of CR0) and EM flag are
     * clear, an \#NM exception is not raised prior to the execution of an x87
     * FPU WAIT/FWAIT instruction.
     * - If the EM flag is set, the setting of the TS flag has no effect on the
     * execution of x87 FPU/MMX/SSE/SSE2/SSE3/SSSE3/SSE4 instructions. The
     * processor does not automatically save the context of the x87 FPU, XMM,
     * and MXCSR registers on a task switch. Instead, it sets the TS flag, which
     * causes the processor to raise an \#NM exception whenever it encounters an
     * x87 FPU/MMX/SSE/SSE2/SSE3/SSSE3/SSE4 instruction in the instruction
     * stream for the new task (with the exception of the instructions listed
     * above). The fault handler for the \#NM exception can then be used to
     * clear the TS flag (with the CLTS instruction) and save the context of the
     * x87 FPU, XMM, and MXCSR registers. If the task never encounters an x87
     *   FPU/MMX/SSE/SSE2/SSE3/SSSE3/SSE4 instruction, the x87
     * FPU/MMX/SSE/SSE2/SSE3/SSSE3/SSE4 context is never saved.
     */
    uint64_t TaskSwitched : 1;
#define CR0_TASK_SWITCHED_BIT 3
#define CR0_TASK_SWITCHED_FLAG 0x08
#define CR0_TASK_SWITCHED(_) (((_) >> 3) & 0x01)

    /**
     * @brief Extension Type
     *
     * [Bit 4] Reserved in the Pentium 4, Intel Xeon, P6 family, and Pentium
     * processors. In the Pentium 4, Intel Xeon, and P6 family processors, this
     * flag is hardcoded to 1. In the Intel386 and Intel486 processors, this
     * flag indicates support of Intel 387 DX math coprocessor instructions when
     * set.
     */
    uint64_t ExtensionType : 1;
#define CR0_EXTENSION_TYPE_BIT 4
#define CR0_EXTENSION_TYPE_FLAG 0x10
#define CR0_EXTENSION_TYPE(_) (((_) >> 4) & 0x01)

    /**
     * @brief Numeric Error
     *
     * [Bit 5] Enables the native (internal) mechanism for reporting x87 FPU
     * errors when set; enables the PC-style x87 FPU error reporting mechanism
     * when clear. When the NE flag is clear and the IGNNE\# input is asserted,
     * x87 FPU errors are ignored. When the NE flag is clear and the IGNNE\#
     * input is deasserted, an unmasked x87 FPU error causes the processor to
     * assert the FERR\# pin to generate an external interrupt and to stop
     * instruction execution immediately before executing the next waiting
     * floating-point instruction or WAIT/FWAIT instruction. The FERR\# pin is
     * intended to drive an input to an external interrupt controller (the
     * FERR\# pin emulates the ERROR\# pin of the Intel 287 and Intel 387 DX
     * math coprocessors). The NE flag, IGNNE\# pin, and FERR\# pin are used
     * with external logic to implement PC-style error reporting. Using FERR\#
     * and IGNNE\# to handle floating-point exceptions is deprecated by modern
     * operating systems; this non-native approach also limits newer processors
     * to operate with one logical processor active.
     *
     * @see Vol1[8.7(Handling x87 FPU Exceptions in Software)]
     * @see Vol1[A.1(APPENDIX A | EFLAGS Cross-Reference)]
     */
    uint64_t NumericError : 1;
#define CR0_NUMERIC_ERROR_BIT 5
#define CR0_NUMERIC_ERROR_FLAG 0x20
#define CR0_NUMERIC_ERROR(_) (((_) >> 5) & 0x01)
    uint64_t Reserved1 : 10;

    /**
     * @brief Write Protect
     *
     * [Bit 16] When set, inhibits supervisor-level procedures from writing into
     * readonly pages; when clear, allows supervisor-level procedures to write
     * into read-only pages (regardless of the U/S bit setting). This flag
     * facilitates implementation of the copy-onwrite method of creating a new
     * process (forking) used by operating systems such as UNIX.
     *
     * @see Vol3A[4.1.3(Paging-Mode Modifiers)]
     * @see Vol3A[4.6(ACCESS RIGHTS)]
     */
    uint64_t WriteProtect : 1;
#define CR0_WRITE_PROTECT_BIT 16
#define CR0_WRITE_PROTECT_FLAG 0x10000
#define CR0_WRITE_PROTECT(_) (((_) >> 16) & 0x01)
    uint64_t Reserved2 : 1;

    /**
     * @brief Alignment Mask
     *
     * [Bit 18] Enables automatic alignment checking when set; disables
     * alignment checking when clear. Alignment checking is performed only when
     * the AM flag is set, the AC flag in the EFLAGS register is set, CPL is 3,
     * and the processor is operating in either protected or virtual-8086 mode.
     */
    uint64_t AlignmentMask : 1;
#define CR0_ALIGNMENT_MASK_BIT 18
#define CR0_ALIGNMENT_MASK_FLAG 0x40000
#define CR0_ALIGNMENT_MASK(_) (((_) >> 18) & 0x01)
    uint64_t Reserved3 : 10;

    /**
     * @brief Not Write-through
     *
     * [Bit 29] When the NW and CD flags are clear, write-back (for Pentium 4,
     * Intel Xeon, P6 family, and Pentium processors) or write-through (for
     * Intel486 processors) is enabled for writes that hit the cache and
     * invalidation cycles are enabled.
     */
    uint64_t NotWriteThrough : 1;
#define CR0_NOT_WRITE_THROUGH_BIT 29
#define CR0_NOT_WRITE_THROUGH_FLAG 0x20000000
#define CR0_NOT_WRITE_THROUGH(_) (((_) >> 29) & 0x01)

    /**
     * @brief Cache Disable
     *
     * [Bit 30] When the CD and NW flags are clear, caching of memory locations
     * for the whole of physical memory in the processor's internal (and
     * external) caches is enabled. When the CD flag is set, caching is
     * restricted. To prevent the processor from accessing and updating its
     * caches, the CD flag must be set and the caches must be invalidated so
     * that no cache hits can occur.
     *
     * @see Vol3A[11.5.3(Preventing Caching)]
     * @see Vol3A[11.5(CACHE CONTROL)]
     */
    uint64_t CacheDisable : 1;
#define CR0_CACHE_DISABLE_BIT 30
#define CR0_CACHE_DISABLE_FLAG 0x40000000
#define CR0_CACHE_DISABLE(_) (((_) >> 30) & 0x01)

    /**
     * @brief Paging Enable
     *
     * [Bit 31] Enables paging when set; disables paging when clear. When paging
     * is disabled, all linear addresses are treated as physical addresses. The
     * PG flag has no effect if the PE flag (bit 0 of register CR0) is not also
     * set; setting the PG flag when the PE flag is clear causes a
     * general-protection exception (\#GP). On Intel 64 processors, enabling and
     * disabling IA-32e mode operation also requires modifying CR0.PG.
     *
     * @see Vol3A[4(PAGING)]
     */
    uint64_t PagingEnable : 1;
#define CR0_PAGING_ENABLE_BIT 31
#define CR0_PAGING_ENABLE_FLAG 0x80000000
#define CR0_PAGING_ENABLE(_) (((_) >> 31) & 0x01)
    uint64_t Reserved4 : 32;
  };

  uint64_t Flags;
};

union Cr4_t {
  Cr4_t() { Flags = 0; }

  Cr4_t(const uint64_t Value) { Flags = Value; }

  bool operator==(const Cr4_t &B) const { return Flags == B.Flags; }

  void Print() const {
    fmt::print("CR4: {:#x}\n", Flags);
    fmt::print("CR4.VirtualModeExtensions: {}\n", VirtualModeExtensions);
    fmt::print("CR4.ProtectedModeVirtualInterrupts: {}\n",
               ProtectedModeVirtualInterrupts);
    fmt::print("CR4.TimestampDisable: {}\n", TimestampDisable);
    fmt::print("CR4.DebuggingExtensions: {}\n", DebuggingExtensions);
    fmt::print("CR4.PageSizeExtensions: {}\n", PageSizeExtensions);
    fmt::print("CR4.PhysicalAddressExtension: {}\n", PhysicalAddressExtension);
    fmt::print("CR4.MachineCheckEnable: {}\n", MachineCheckEnable);
    fmt::print("CR4.PageGlobalEnable: {}\n", PageGlobalEnable);
    fmt::print("CR4.PerformanceMonitoringCounterEnable: {}\n",
               PerformanceMonitoringCounterEnable);
    fmt::print("CR4.OsFxsaveFxrstorSupport: {}\n", OsFxsaveFxrstorSupport);
    fmt::print("CR4.OsXmmExceptionSupport: {}\n", OsXmmExceptionSupport);
    fmt::print("CR4.UsermodeInstructionPrevention: {}\n",
               UsermodeInstructionPrevention);
    fmt::print("CR4.LA57: {}\n", LA57);
    fmt::print("CR4.VmxEnable: {}\n", VmxEnable);
    fmt::print("CR4.SmxEnable: {}\n", SmxEnable);
    fmt::print("CR4.FsgsbaseEnable: {}\n", FsgsbaseEnable);
    fmt::print("CR4.PcidEnable: {}\n", PcidEnable);
    fmt::print("CR4.OsXsave: {}\n", OsXsave);
    fmt::print("CR4.SmepEnable: {}\n", SmepEnable);
    fmt::print("CR4.SmapEnable: {}\n", SmapEnable);
    fmt::print("CR4.ProtectionKeyEnable: {}\n", ProtectionKeyEnable);
  }

  struct {
    /**
     * @brief Virtual-8086 Mode Extensions
     *
     * [Bit 0] Enables interrupt- and exception-handling extensions in
     * virtual-8086 mode when set; disables the extensions when clear. Use of
     * the virtual mode extensions can improve the performance of virtual-8086
     * applications by eliminating the overhead of calling the virtual- 8086
     * monitor to handle interrupts and exceptions that occur while executing an
     * 8086 program and, instead, redirecting the interrupts and exceptions back
     * to the 8086 program's handlers. It also provides hardware support for a
     * virtual interrupt flag (VIF) to improve reliability of running 8086
     * programs in multitasking and multiple-processor environments.
     *
     * @see Vol3B[20.3(INTERRUPT AND EXCEPTION HANDLING IN VIRTUAL-8086 MODE)]
     */
    uint64_t VirtualModeExtensions : 1;
#define CR4_VIRTUAL_MODE_EXTENSIONS_BIT 0
#define CR4_VIRTUAL_MODE_EXTENSIONS_FLAG 0x01
#define CR4_VIRTUAL_MODE_EXTENSIONS(_) (((_) >> 0) & 0x01)

    /**
     * @brief Protected-Mode Virtual Interrupts
     *
     * [Bit 1] Enables hardware support for a virtual interrupt flag (VIF) in
     * protected mode when set; disables the VIF flag in protected mode when
     * clear.
     *
     * @see Vol3B[20.4(PROTECTED-MODE VIRTUAL INTERRUPTS)]
     */
    uint64_t ProtectedModeVirtualInterrupts : 1;
#define CR4_PROTECTED_MODE_VIRTUAL_INTERRUPTS_BIT 1
#define CR4_PROTECTED_MODE_VIRTUAL_INTERRUPTS_FLAG 0x02
#define CR4_PROTECTED_MODE_VIRTUAL_INTERRUPTS(_) (((_) >> 1) & 0x01)

    /**
     * @brief Time Stamp Disable
     *
     * [Bit 2] Restricts the execution of the RDTSC instruction to procedures
     * running at privilege level 0 when set; allows RDTSC instruction to be
     * executed at any privilege level when clear. This bit also applies to the
     * RDTSCP instruction if supported (if CPUID.80000001H:EDX[27] = 1).
     */
    uint64_t TimestampDisable : 1;
#define CR4_TIMESTAMP_DISABLE_BIT 2
#define CR4_TIMESTAMP_DISABLE_FLAG 0x04
#define CR4_TIMESTAMP_DISABLE(_) (((_) >> 2) & 0x01)

    /**
     * @brief Debugging Extensions
     *
     * [Bit 3] References to debug registers DR4 and DR5 cause an undefined
     * opcode (\#UD) exception to be generated when set; when clear, processor
     * aliases references to registers DR4 and DR5 for compatibility with
     * software written to run on earlier IA-32 processors.
     *
     * @see Vol3B[17.2.2(Debug Registers DR4 and DR5)]
     */
    uint64_t DebuggingExtensions : 1;
#define CR4_DEBUGGING_EXTENSIONS_BIT 3
#define CR4_DEBUGGING_EXTENSIONS_FLAG 0x08
#define CR4_DEBUGGING_EXTENSIONS(_) (((_) >> 3) & 0x01)

    /**
     * @brief Page Size Extensions
     *
     * [Bit 4] Enables 4-MByte pages with 32-bit paging when set; restricts
     * 32-bit paging to pages of 4 KBytes when clear.
     *
     * @see Vol3A[4.3(32-BIT PAGING)]
     */
    uint64_t PageSizeExtensions : 1;
#define CR4_PAGE_SIZE_EXTENSIONS_BIT 4
#define CR4_PAGE_SIZE_EXTENSIONS_FLAG 0x10
#define CR4_PAGE_SIZE_EXTENSIONS(_) (((_) >> 4) & 0x01)

    /**
     * @brief Physical Address Extension
     *
     * [Bit 5] When set, enables paging to produce physical addresses with more
     * than 32 bits. When clear, restricts physical addresses to 32 bits. PAE
     * must be set before entering IA-32e mode.
     *
     * @see Vol3A[4(PAGING)]
     */
    uint64_t PhysicalAddressExtension : 1;
#define CR4_PHYSICAL_ADDRESS_EXTENSION_BIT 5
#define CR4_PHYSICAL_ADDRESS_EXTENSION_FLAG 0x20
#define CR4_PHYSICAL_ADDRESS_EXTENSION(_) (((_) >> 5) & 0x01)

    /**
     * @brief Machine-Check Enable
     *
     * [Bit 6] Enables the machine-check exception when set; disables the
     * machine-check exception when clear.
     *
     * @see Vol3B[15(MACHINE-CHECK ARCHITECTURE)]
     */
    uint64_t MachineCheckEnable : 1;
#define CR4_MACHINE_CHECK_ENABLE_BIT 6
#define CR4_MACHINE_CHECK_ENABLE_FLAG 0x40
#define CR4_MACHINE_CHECK_ENABLE(_) (((_) >> 6) & 0x01)

    /**
     * @brief Page Global Enable
     *
     * [Bit 7] (Introduced in the P6 family processors.) Enables the global page
     * feature when set; disables the global page feature when clear. The global
     * page feature allows frequently used or shared pages to be marked as
     * global to all users (done with the global flag, bit 8, in a
     * page-directory or page-table entry). Global pages are not flushed from
     * the translation-lookaside buffer (TLB) on a task switch or a write to
     * register CR3. When enabling the global page feature, paging must be
     * enabled (by setting the PG flag in control register CR0) before the PGE
     * flag is set. Reversing this sequence may affect program correctness, and
     * processor performance will be impacted.
     *
     * @see Vol3A[4.10(CACHING TRANSLATION INFORMATION)]
     */
    uint64_t PageGlobalEnable : 1;
#define CR4_PAGE_GLOBAL_ENABLE_BIT 7
#define CR4_PAGE_GLOBAL_ENABLE_FLAG 0x80
#define CR4_PAGE_GLOBAL_ENABLE(_) (((_) >> 7) & 0x01)

    /**
     * @brief Performance-Monitoring Counter Enable
     *
     * [Bit 8] Enables execution of the RDPMC instruction for programs or
     * procedures running at any protection level when set; RDPMC instruction
     * can be executed only at protection level 0 when clear.
     */
    uint64_t PerformanceMonitoringCounterEnable : 1;
#define CR4_PERFORMANCE_MONITORING_COUNTER_ENABLE_BIT 8
#define CR4_PERFORMANCE_MONITORING_COUNTER_ENABLE_FLAG 0x100
#define CR4_PERFORMANCE_MONITORING_COUNTER_ENABLE(_) (((_) >> 8) & 0x01)

    /**
     * @brief Operating System Support for FXSAVE and FXRSTOR instructions
     *
     * [Bit 9] When set, this flag:
     * -# indicates to software that the operating system supports the use of
     * the FXSAVE and FXRSTOR instructions,
     * -# enables the FXSAVE and FXRSTOR instructions to save and restore the
     * contents of the XMM and MXCSR registers along with the contents of the
     * x87 FPU and MMX registers, and
     * -# enables the processor to execute SSE/SSE2/SSE3/SSSE3/SSE4
     * instructions, with the exception of the PAUSE, PREFETCHh, SFENCE, LFENCE,
     * MFENCE, MOVNTI, CLFLUSH, CRC32, and POPCNT. If this flag is clear, the
     * FXSAVE and FXRSTOR instructions will save and restore the contents of the
     * x87 FPU and MMX registers, but they may not save and restore the contents
     * of the XMM and MXCSR registers. Also, the processor will generate an
     * invalid opcode exception (\#UD) if it attempts to execute any
     * SSE/SSE2/SSE3 instruction, with the exception of PAUSE, PREFETCHh,
     * SFENCE, LFENCE, MFENCE, MOVNTI, CLFLUSH, CRC32, and POPCNT. The operating
     * system or executive must explicitly set this flag.
     *
     * @remarks CPUID feature flag FXSR indicates availability of the
     * FXSAVE/FXRSTOR instructions. The OSFXSR bit provides operating system
     * software with a means of enabling FXSAVE/FXRSTOR to save/restore the
     * contents of the X87 FPU, XMM and MXCSR registers. Consequently OSFXSR bit
     * indicates that the operating system provides context switch support for
     *          SSE/SSE2/SSE3/SSSE3/SSE4.
     */
    uint64_t OsFxsaveFxrstorSupport : 1;
#define CR4_OS_FXSAVE_FXRSTOR_SUPPORT_BIT 9
#define CR4_OS_FXSAVE_FXRSTOR_SUPPORT_FLAG 0x200
#define CR4_OS_FXSAVE_FXRSTOR_SUPPORT(_) (((_) >> 9) & 0x01)

    /**
     * @brief Operating System Support for Unmasked SIMD Floating-Point
     * Exceptions
     *
     * [Bit 10] Operating System Support for Unmasked SIMD Floating-Point
     * Exceptions - When set, indicates that the operating system supports the
     * handling of unmasked SIMD floating-point exceptions through an exception
     * handler that is invoked when a SIMD floating-point exception (\#XM) is
     * generated. SIMD floating-point exceptions are only generated by
     * SSE/SSE2/SSE3/SSE4.1 SIMD floatingpoint instructions.
     * The operating system or executive must explicitly set this flag. If this
     * flag is not set, the processor will generate an invalid opcode exception
     * (\#UD) whenever it detects an unmasked SIMD floating-point exception.
     */
    uint64_t OsXmmExceptionSupport : 1;
#define CR4_OS_XMM_EXCEPTION_SUPPORT_BIT 10
#define CR4_OS_XMM_EXCEPTION_SUPPORT_FLAG 0x400
#define CR4_OS_XMM_EXCEPTION_SUPPORT(_) (((_) >> 10) & 0x01)

    /**
     * @brief User-Mode Instruction Prevention
     *
     * [Bit 11] When set, the following instructions cannot be executed if CPL >
     * 0: SGDT, SIDT, SLDT, SMSW, and STR. An attempt at such execution causes a
     * generalprotection exception (\#GP).
     */
    uint64_t UsermodeInstructionPrevention : 1;
#define CR4_USERMODE_INSTRUCTION_PREVENTION_BIT 11
#define CR4_USERMODE_INSTRUCTION_PREVENTION_FLAG 0x800
#define CR4_USERMODE_INSTRUCTION_PREVENTION(_) (((_) >> 11) & 0x01)

    uint64_t LA57 : 1;
#define CR4_LA57_BIT 12
#define CR4_LA57_FLAG 0x1000
#define CR4_LA57(_) (((_) >> 12) & 0x01)

    /**
     * @brief VMX-Enable
     *
     * [Bit 13] Enables VMX operation when set.
     *
     * @see Vol3C[23(INTRODUCTION TO VIRTUAL MACHINE EXTENSIONS)]
     */
    uint64_t VmxEnable : 1;
#define CR4_VMX_ENABLE_BIT 13
#define CR4_VMX_ENABLE_FLAG 0x2000
#define CR4_VMX_ENABLE(_) (((_) >> 13) & 0x01)

    /**
     * @brief SMX-Enable
     *
     * [Bit 14] Enables SMX operation when set.
     *
     * @see Vol2[6(SAFER MODE EXTENSIONS REFERENCE)]
     */
    uint64_t SmxEnable : 1;
#define CR4_SMX_ENABLE_BIT 14
#define CR4_SMX_ENABLE_FLAG 0x4000
#define CR4_SMX_ENABLE(_) (((_) >> 14) & 0x01)
    uint64_t Reserved2 : 1;

    /**
     * @brief FSGSBASE-Enable
     *
     * [Bit 16] Enables the instructions RDFSBASE, RDGSBASE, WRFSBASE, and
     * WRGSBASE.
     */
    uint64_t FsgsbaseEnable : 1;
#define CR4_FSGSBASE_ENABLE_BIT 16
#define CR4_FSGSBASE_ENABLE_FLAG 0x10000
#define CR4_FSGSBASE_ENABLE(_) (((_) >> 16) & 0x01)

    /**
     * @brief PCID-Enable
     *
     * [Bit 17] Enables process-context identifiers (PCIDs) when set. Can be set
     * only in IA-32e mode (if IA32_EFER.LMA = 1).
     *
     * @see Vol3A[4.10.1(Process-Context Identifiers (PCIDs))]
     */
    uint64_t PcidEnable : 1;
#define CR4_PCID_ENABLE_BIT 17
#define CR4_PCID_ENABLE_FLAG 0x20000
#define CR4_PCID_ENABLE(_) (((_) >> 17) & 0x01)

    /**
     * @brief XSAVE and Processor Extended States-Enable
     *
     * [Bit 18] When set, this flag:
     * -# indicates (via CPUID.01H:ECX.OSXSAVE[bit 27]) that the operating
     * system supports the use of the XGETBV, XSAVE and XRSTOR instructions by
     * general software;
     * -# enables the XSAVE and XRSTOR instructions to save and restore the x87
     * FPU state (including MMX registers), the SSE state (XMM registers and
     * MXCSR), along with other processor extended states enabled in XCR0;
     * -# enables the processor to execute XGETBV and XSETBV instructions in
     * order to read and write XCR0.
     *
     * @see Vol3A[2.6(EXTENDED CONTROL REGISTERS (INCLUDING XCR0))]
     * @see Vol3A[13(SYSTEM PROGRAMMING FOR INSTRUCTION SET EXTENSIONS AND
     * PROCESSOR EXTENDED)]
     */
    uint64_t OsXsave : 1;
#define CR4_OS_XSAVE_BIT 18
#define CR4_OS_XSAVE_FLAG 0x40000
#define CR4_OS_XSAVE(_) (((_) >> 18) & 0x01)
    uint64_t Reserved3 : 1;

    /**
     * @brief SMEP-Enable
     *
     * [Bit 20] Enables supervisor-mode execution prevention (SMEP) when set.
     *
     * @see Vol3A[4.6(ACCESS RIGHTS)]
     */
    uint64_t SmepEnable : 1;
#define CR4_SMEP_ENABLE_BIT 20
#define CR4_SMEP_ENABLE_FLAG 0x100000
#define CR4_SMEP_ENABLE(_) (((_) >> 20) & 0x01)

    /**
     * @brief SMAP-Enable
     *
     * [Bit 21] Enables supervisor-mode access prevention (SMAP) when set.
     *
     * @see Vol3A[4.6(ACCESS RIGHTS)]
     */
    uint64_t SmapEnable : 1;
#define CR4_SMAP_ENABLE_BIT 21
#define CR4_SMAP_ENABLE_FLAG 0x200000
#define CR4_SMAP_ENABLE(_) (((_) >> 21) & 0x01)

    /**
     * @brief Protection-Key-Enable
     *
     * [Bit 22] Enables 4-level paging to associate each linear address with a
     * protection key. The PKRU register specifies, for each protection key,
     * whether user-mode linear addresses with that protection key can be read
     * or written. This bit also enables access to the PKRU register using the
     * RDPKRU and WRPKRU instructions.
     */
    uint64_t ProtectionKeyEnable : 1;
#define CR4_PROTECTION_KEY_ENABLE_BIT 22
#define CR4_PROTECTION_KEY_ENABLE_FLAG 0x400000
#define CR4_PROTECTION_KEY_ENABLE(_) (((_) >> 22) & 0x01)
    uint64_t Reserved4 : 41;
  };

  uint64_t Flags;
};

union Efer_t {
  Efer_t() { Flags = 0; }

  Efer_t(const uint64_t Value) { Flags = Value; }

  bool operator==(const Efer_t &B) const { return Flags == B.Flags; }

  void Print() const {
    fmt::print("EFER: {:#x}\n", Flags);
    fmt::print("EFER.SyscallEnable: {}\n", SyscallEnable);
    fmt::print("EFER.Ia32EModeEnable: {}\n", Ia32EModeEnable);
    fmt::print("EFER.Ia32EModeActive: {}\n", Ia32EModeActive);
    fmt::print("EFER.ExecuteDisableBitEnable: {}\n", ExecuteDisableBitEnable);
  }

  struct {
    /**
     * @brief SYSCALL Enable <b>(R/W)</b>
     *
     * [Bit 0] Enables SYSCALL/SYSRET instructions in 64-bit mode.
     */
    uint64_t SyscallEnable : 1;
#define IA32_EFER_SYSCALL_ENABLE_BIT 0
#define IA32_EFER_SYSCALL_ENABLE_FLAG 0x01
#define IA32_EFER_SYSCALL_ENABLE(_) (((_) >> 0) & 0x01)
    uint64_t Reserved1 : 7;

    /**
     * @brief IA-32e Mode Enable <b>(R/W)</b>
     *
     * [Bit 8] Enables IA-32e mode operation.
     */
    uint64_t Ia32EModeEnable : 1;
#define IA32_EFER_IA32E_MODE_ENABLE_BIT 8
#define IA32_EFER_IA32E_MODE_ENABLE_FLAG 0x100
#define IA32_EFER_IA32E_MODE_ENABLE(_) (((_) >> 8) & 0x01)
    uint64_t Reserved2 : 1;

    /**
     * @brief IA-32e Mode Active <b>(R)</b>
     *
     * [Bit 10] Indicates IA-32e mode is active when set.
     */
    uint64_t Ia32EModeActive : 1;
#define IA32_EFER_IA32E_MODE_ACTIVE_BIT 10
#define IA32_EFER_IA32E_MODE_ACTIVE_FLAG 0x400
#define IA32_EFER_IA32E_MODE_ACTIVE(_) (((_) >> 10) & 0x01)

    /**
     * [Bit 11] Execute Disable Bit Enable.
     */
    uint64_t ExecuteDisableBitEnable : 1;
#define IA32_EFER_EXECUTE_DISABLE_BIT_ENABLE_BIT 11
#define IA32_EFER_EXECUTE_DISABLE_BIT_ENABLE_FLAG 0x800
#define IA32_EFER_EXECUTE_DISABLE_BIT_ENABLE(_) (((_) >> 11) & 0x01)
    uint64_t Reserved3 : 52;
  };

  uint64_t Flags;
};

#define MSR_IA32_APICBASE 0x0000001b
#define MSR_IA32_TSC 0x00000010
#define MSR_IA32_SYSENTER_CS 0x00000174
#define MSR_IA32_SYSENTER_ESP 0x00000175
#define MSR_IA32_SYSENTER_EIP 0x00000176
#define MSR_IA32_CR_PAT 0x00000277
#define MSR_IA32_EFER 0xc0000080
#define MSR_IA32_STAR 0xC0000081
#define MSR_IA32_LSTAR 0xc0000082
#define MSR_IA32_CSTAR 0xc0000083
#define MSR_IA32_SFMASK 0xC0000084
#define MSR_IA32_KERNEL_GS_BASE 0xc0000102
#define MSR_IA32_TSC_AUX 0xc0000103
#define MSR_IA32_PERF_GLOBAL_STATUS 0x0000038E
#define MSR_IA32_FIXED_CTR_CTRL 0x0000038D

/**
 * @defgroup IA32_FIXED_CTR \
 *           IA32_FIXED_CTR(n)
 *
 * Fixed-Function Performance Counter n.
 *
 * @remarks If CPUID.0AH: EDX[4:0] > n
 * @{
 */
/**
 * Counts Instr_Retired.Any.
 */
#define MSR_IA32_FIXED_CTR0 0x00000309

/**
 * @brief Global Performance Counter Control <b>(R/W)</b>
 *
 * Global Performance Counter Control. Counter increments while the result of
 * ANDing the respective enable bit in this MSR with the corresponding OS or USR
 * bits in the general-purpose or fixed counter control MSR is true.
 *
 * @remarks If CPUID.0AH: EAX[7:0] > 0
 */
#define MSR_IA32_PERF_GLOBAL_CTRL 0x0000038F
struct IA32_PERF_GLOBAL_CTRL_REGISTER_t {
  IA32_PERF_GLOBAL_CTRL_REGISTER_t() { Flags = 0; }

  bool operator==(const IA32_PERF_GLOBAL_CTRL_REGISTER_t &B) const {
    return Flags == B.Flags;
  }

  union {
    struct {
      /**
       * EN_PMC(n). Enable bitmask. Only the first n-1 bits are valid. Bits 31:n
       * are reserved.
       *
       * @remarks If CPUID.0AH: EAX[15:8] > n
       */
      uint64_t EnPmcn : 32;

      /**
       * EN_FIXED_CTR(n). Enable bitmask. Only the first n-1 bits are valid.
       * Bits 31:n are reserved.
       *
       * @remarks If CPUID.0AH: EDX[4:0] > n
       */
      uint64_t EnFixedCtrn : 32;
    };
    uint64_t Flags;
  };
};

/**
 * The 64-bit RFLAGS register contains a group of status flags, a control flag,
 * and a group of system flags in 64-bit mode. The upper 32 bits of RFLAGS
 * register is reserved. The lower 32 bits of RFLAGS is the same as EFLAGS.
 *
 * @see EFLAGS
 * @see Vol1[3.4.3.4(RFLAGS Register in 64-Bit Mode)] (reference)
 */
union Rflags_t {
  Rflags_t(const uint64_t Rflags = 0) { Flags = Rflags; }

  struct {
    /**
     * @brief Carry flag
     *
     * [Bit 0] See the description in EFLAGS.
     */
    uint64_t CarryFlag : 1;
#define RFLAGS_CARRY_FLAG_BIT 0
#define RFLAGS_CARRY_FLAG_FLAG 0x01
#define RFLAGS_CARRY_FLAG(_) (((_) >> 0) & 0x01)

    /**
     * [Bit 1] Reserved - always 1
     */
    uint64_t ReadAs1 : 1;
#define RFLAGS_READ_AS_1_BIT 1
#define RFLAGS_READ_AS_1_FLAG 0x02
#define RFLAGS_READ_AS_1(_) (((_) >> 1) & 0x01)

    /**
     * @brief Parity flag
     *
     * [Bit 2] See the description in EFLAGS.
     */
    uint64_t ParityFlag : 1;
#define RFLAGS_PARITY_FLAG_BIT 2
#define RFLAGS_PARITY_FLAG_FLAG 0x04
#define RFLAGS_PARITY_FLAG(_) (((_) >> 2) & 0x01)
    uint64_t Reserved1 : 1;

    /**
     * @brief Auxiliary Carry flag
     *
     * [Bit 4] See the description in EFLAGS.
     */
    uint64_t AuxiliaryCarryFlag : 1;
#define RFLAGS_AUXILIARY_CARRY_FLAG_BIT 4
#define RFLAGS_AUXILIARY_CARRY_FLAG_FLAG 0x10
#define RFLAGS_AUXILIARY_CARRY_FLAG(_) (((_) >> 4) & 0x01)
    uint64_t Reserved2 : 1;

    /**
     * @brief Zero flag
     *
     * [Bit 6] See the description in EFLAGS.
     */
    uint64_t ZeroFlag : 1;
#define RFLAGS_ZERO_FLAG_BIT 6
#define RFLAGS_ZERO_FLAG_FLAG 0x40
#define RFLAGS_ZERO_FLAG(_) (((_) >> 6) & 0x01)

    /**
     * @brief Sign flag
     *
     * [Bit 7] See the description in EFLAGS.
     */
    uint64_t SignFlag : 1;
#define RFLAGS_SIGN_FLAG_BIT 7
#define RFLAGS_SIGN_FLAG_FLAG 0x80
#define RFLAGS_SIGN_FLAG(_) (((_) >> 7) & 0x01)

    /**
     * @brief Trap flag
     *
     * [Bit 8] See the description in EFLAGS.
     */
    uint64_t TrapFlag : 1;
#define RFLAGS_TRAP_FLAG_BIT 8
#define RFLAGS_TRAP_FLAG_FLAG 0x100
#define RFLAGS_TRAP_FLAG(_) (((_) >> 8) & 0x01)

    /**
     * @brief Interrupt enable flag
     *
     * [Bit 9] See the description in EFLAGS.
     */
    uint64_t InterruptEnableFlag : 1;
#define RFLAGS_INTERRUPT_ENABLE_FLAG_BIT 9
#define RFLAGS_INTERRUPT_ENABLE_FLAG_FLAG 0x200
#define RFLAGS_INTERRUPT_ENABLE_FLAG(_) (((_) >> 9) & 0x01)

    /**
     * @brief Direction flag
     *
     * [Bit 10] See the description in EFLAGS.
     */
    uint64_t DirectionFlag : 1;
#define RFLAGS_DIRECTION_FLAG_BIT 10
#define RFLAGS_DIRECTION_FLAG_FLAG 0x400
#define RFLAGS_DIRECTION_FLAG(_) (((_) >> 10) & 0x01)

    /**
     * @brief Overflow flag
     *
     * [Bit 11] See the description in EFLAGS.
     */
    uint64_t OverflowFlag : 1;
#define RFLAGS_OVERFLOW_FLAG_BIT 11
#define RFLAGS_OVERFLOW_FLAG_FLAG 0x800
#define RFLAGS_OVERFLOW_FLAG(_) (((_) >> 11) & 0x01)

    /**
     * @brief I/O privilege level field
     *
     * [Bits 13:12] See the description in EFLAGS.
     */
    uint64_t IoPrivilegeLevel : 2;
#define RFLAGS_IO_PRIVILEGE_LEVEL_BIT 12
#define RFLAGS_IO_PRIVILEGE_LEVEL_FLAG 0x3000
#define RFLAGS_IO_PRIVILEGE_LEVEL(_) (((_) >> 12) & 0x03)

    /**
     * @brief Nested task flag
     *
     * [Bit 14] See the description in EFLAGS.
     */
    uint64_t NestedTaskFlag : 1;
#define RFLAGS_NESTED_TASK_FLAG_BIT 14
#define RFLAGS_NESTED_TASK_FLAG_FLAG 0x4000
#define RFLAGS_NESTED_TASK_FLAG(_) (((_) >> 14) & 0x01)
    uint64_t Reserved3 : 1;

    /**
     * @brief Resume flag
     *
     * [Bit 16] See the description in EFLAGS.
     */
    uint64_t ResumeFlag : 1;
#define RFLAGS_RESUME_FLAG_BIT 16
#define RFLAGS_RESUME_FLAG_FLAG 0x10000
#define RFLAGS_RESUME_FLAG(_) (((_) >> 16) & 0x01)

    /**
     * @brief Virtual-8086 mode flag
     *
     * [Bit 17] See the description in EFLAGS.
     */
    uint64_t Virtual8086ModeFlag : 1;
#define RFLAGS_VIRTUAL_8086_MODE_FLAG_BIT 17
#define RFLAGS_VIRTUAL_8086_MODE_FLAG_FLAG 0x20000
#define RFLAGS_VIRTUAL_8086_MODE_FLAG(_) (((_) >> 17) & 0x01)

    /**
     * @brief Alignment check (or access control) flag
     *
     * [Bit 18] See the description in EFLAGS.
     *
     * @see Vol3A[4.6(ACCESS RIGHTS)]
     */
    uint64_t AlignmentCheckFlag : 1;
#define RFLAGS_ALIGNMENT_CHECK_FLAG_BIT 18
#define RFLAGS_ALIGNMENT_CHECK_FLAG_FLAG 0x40000
#define RFLAGS_ALIGNMENT_CHECK_FLAG(_) (((_) >> 18) & 0x01)

    /**
     * @brief Virtual interrupt flag
     *
     * [Bit 19] See the description in EFLAGS.
     */
    uint64_t VirtualInterruptFlag : 1;
#define RFLAGS_VIRTUAL_INTERRUPT_FLAG_BIT 19
#define RFLAGS_VIRTUAL_INTERRUPT_FLAG_FLAG 0x80000
#define RFLAGS_VIRTUAL_INTERRUPT_FLAG(_) (((_) >> 19) & 0x01)

    /**
     * @brief Virtual interrupt pending flag
     *
     * [Bit 20] See the description in EFLAGS.
     */
    uint64_t VirtualInterruptPendingFlag : 1;
#define RFLAGS_VIRTUAL_INTERRUPT_PENDING_FLAG_BIT 20
#define RFLAGS_VIRTUAL_INTERRUPT_PENDING_FLAG_FLAG 0x100000
#define RFLAGS_VIRTUAL_INTERRUPT_PENDING_FLAG(_) (((_) >> 20) & 0x01)

    /**
     * @brief Identification flag
     *
     * [Bit 21] See the description in EFLAGS.
     */
    uint64_t IdentificationFlag : 1;
#define RFLAGS_IDENTIFICATION_FLAG_BIT 21
#define RFLAGS_IDENTIFICATION_FLAG_FLAG 0x200000
#define RFLAGS_IDENTIFICATION_FLAG(_) (((_) >> 21) & 0x01)
    uint64_t Reserved4 : 42;
  };

  uint64_t Flags;
};

struct CpuState_t {
  uint64_t Seed;
  uint64_t Rax;
  uint64_t Rcx;
  uint64_t Rdx;
  uint64_t Rbx;
  uint64_t Rsp;
  uint64_t Rbp;
  uint64_t Rsi;
  uint64_t Rdi;
  uint64_t R8;
  uint64_t R9;
  uint64_t R10;
  uint64_t R11;
  uint64_t R12;
  uint64_t R13;
  uint64_t R14;
  uint64_t R15;
  uint64_t Rip;
  uint64_t Rflags;
  Seg_t Es;
  Seg_t Cs;
  Seg_t Ss;
  Seg_t Ds;
  Seg_t Fs;
  Seg_t Gs;
  Seg_t Ldtr;
  Seg_t Tr;
  GlobalSeg_t Gdtr;
  GlobalSeg_t Idtr;
  Cr0_t Cr0;
  uint64_t Cr2;
  uint64_t Cr3;
  Cr4_t Cr4;
  uint64_t Cr8;
  uint64_t Dr0;
  uint64_t Dr1;
  uint64_t Dr2;
  uint64_t Dr3;
  uint32_t Dr6;
  uint32_t Dr7;
  uint32_t Xcr0;
  Zmm_t Zmm[32];
  uint16_t Fpcw;
  uint16_t Fpsw;
  uint16_t Fptw;
  uint16_t Fpop;
  uint64_t Fpst[8];
  uint32_t Mxcsr;
  uint32_t MxcsrMask;
  uint64_t Tsc;
  Efer_t Efer;
  uint64_t KernelGsBase;
  uint64_t ApicBase;
  uint64_t Pat;
  uint64_t SysenterCs;
  uint64_t SysenterEip;
  uint64_t SysenterEsp;
  uint64_t Star;
  uint64_t Lstar;
  uint64_t Cstar;
  uint64_t Sfmask;
  uint64_t TscAux;

  CpuState_t() { memset(this, 0, sizeof(decltype(*this))); }

  bool operator==(const CpuState_t &B) const {
    bool Equal = Seed == B.Seed;
    Equal = Equal && Rax == B.Rax;
    Equal = Equal && Rcx == B.Rcx;
    Equal = Equal && Rdx == B.Rdx;
    Equal = Equal && Rbx == B.Rbx;
    Equal = Equal && Rsp == B.Rsp;
    Equal = Equal && Rbp == B.Rbp;
    Equal = Equal && Rsi == B.Rsi;
    Equal = Equal && Rdi == B.Rdi;
    Equal = Equal && R8 == B.R8;
    Equal = Equal && R9 == B.R9;
    Equal = Equal && R10 == B.R10;
    Equal = Equal && R11 == B.R11;
    Equal = Equal && R12 == B.R12;
    Equal = Equal && R13 == B.R13;
    Equal = Equal && R14 == B.R14;
    Equal = Equal && R15 == B.R15;
    Equal = Equal && Rip == B.Rip;
    Equal = Equal && Rflags == B.Rflags;
    Equal = Equal && Es == B.Es;
    Equal = Equal && Cs == B.Cs;
    Equal = Equal && Ss == B.Ss;
    Equal = Equal && Ds == B.Ds;
    Equal = Equal && Fs == B.Fs;
    Equal = Equal && Gs == B.Gs;
    Equal = Equal && Ldtr == B.Ldtr;
    Equal = Equal && Tr == B.Tr;
    Equal = Equal && Gdtr == B.Gdtr;
    Equal = Equal && Idtr == B.Idtr;
    Equal = Equal && Cr0 == B.Cr0;
    Equal = Equal && Cr2 == B.Cr2;
    Equal = Equal && Cr3 == B.Cr3;
    Equal = Equal && Cr4 == B.Cr4;
    Equal = Equal && Cr8 == B.Cr8;
    Equal = Equal && Dr0 == B.Dr0;
    Equal = Equal && Dr1 == B.Dr1;
    Equal = Equal && Dr2 == B.Dr2;
    Equal = Equal && Dr3 == B.Dr3;
    Equal = Equal && Dr6 == B.Dr6;
    Equal = Equal && Dr7 == B.Dr7;
    Equal = Equal && Xcr0 == B.Xcr0;

    for (size_t Idx = 0; Idx < 32; Idx++) {
      Equal = Equal && Zmm[Idx] == B.Zmm[Idx];
    }

    Equal = Equal && Fpcw == B.Fpcw;
    Equal = Equal && Fpsw == B.Fpsw;
    Equal = Equal && Fptw == B.Fptw;
    Equal = Equal && Fpop == B.Fpop;

    for (size_t Idx = 0; Idx < 8; Idx++) {
      Equal = Equal && Fpst[Idx] == B.Fpst[Idx];
    }

    Equal = Equal && Mxcsr == B.Mxcsr;
    Equal = Equal && MxcsrMask == B.MxcsrMask;
    Equal = Equal && Tsc == B.Tsc;
    Equal = Equal && Efer.Flags == B.Efer.Flags;
    Equal = Equal && KernelGsBase == B.KernelGsBase;
    Equal = Equal && ApicBase == B.ApicBase;
    Equal = Equal && Pat == B.Pat;
    Equal = Equal && SysenterCs == B.SysenterCs;
    Equal = Equal && SysenterEip == B.SysenterEip;
    Equal = Equal && SysenterEsp == B.SysenterEsp;
    Equal = Equal && Star == B.Star;
    Equal = Equal && Lstar == B.Lstar;
    Equal = Equal && Cstar == B.Cstar;
    Equal = Equal && Sfmask == B.Sfmask;
    Equal = Equal && TscAux == B.TscAux;
    return Equal;
  }
};

enum class TraceType_t {
  NoTrace,

  //
  // This is a trace of execution.
  //

  Rip,

  //
  // This is a trace of only unique rip locations.
  //

  UniqueRip,

  //
  // This is a Tenet trace of register & mem changes.
  //

  Tenet

};

//
// The backends supported.
//

enum class BackendType_t { Bochscpu, Whv, Kvm };

//
// LAF/Compcov supported modes.
//

enum class LafCompcovOptions_t {
  Disabled,
  OnlyUser,
  OnlyKernel,
  KernelAndUser
};

struct FuzzOptions_t {

  //
  // Path to the target folder.
  //

  fs::path TargetPath;

  //
  // Seed for RNG.
  //

  uint32_t Seed = 0;

  //
  // Address to connect to the master node.
  //

  std::string Address;
};

struct RunOptions_t {
  //
  // Base path to trace file(e).
  //

  fs::path BaseTracePath;

  //
  // Trace type.
  //

  TraceType_t TraceType = TraceType_t::NoTrace;

  //
  // Input path or input folder.
  //

  fs::path InputPath;

  //
  // Number of time to reexecute the testcase(s) (depending on if InputPath is a
  // file or a folder).
  //

  uint64_t Runs = 0;
};

struct MasterOptions_t {
  //
  // Address to listen to for the master.
  //

  std::string Address;

  //
  // The maximum size of a generated testcase.
  //

  uint64_t TestcaseBufferMaxSize = 0;

  //
  // Path to the target folder.
  //

  fs::path TargetPath;

  //
  // Path to the corpus directory.
  //

  fs::path InputsPath;

  //
  // Path to the output directory.
  //

  fs::path OutputsPath;

  //
  // Path to the crashes directory.
  //

  fs::path CrashesPath;

  //
  // Number of testcases to generate in the fuzz command.
  //

  uint64_t Runs = 0;

  //
  // Seed for the RNG.
  //

  uint64_t Seed = 0;
};

//
// Options passed to the program.
//

struct Options_t {

  //
  // Turn on verbose mode.
  //

  bool Verbose = false;

  //
  // Execution backend.
  //

  BackendType_t Backend = BackendType_t::Bochscpu;

  //
  // Target name.
  //

  std::string TargetName;

  //
  // Path to the state directory (it contains both the kernel dump as well as
  // the cpu state).
  //

  fs::path StatePath;

  //
  // Path to the kernel dump file.
  //

  fs::path DumpPath;

  //
  // Path to the cpu state file.
  //

  fs::path CpuStatePath;

  //
  // Path to the symbol store file.
  //

  fs::path SymbolFilePath;

  //
  // Guest-files path.
  //

  fs::path GuestFilesPath;

  //
  // The limit per testcase: for bochscpu it is an instruction number, for whv
  // it is a number of seconds.
  //

  uint64_t Limit = 0;

  //
  // Sanitized cpu state.
  //

  CpuState_t CpuState;

  //
  // Path to the code coverage file.
  //

  fs::path CoveragePath;

  //
  // Use edge coverage (only with bxcpu).
  //

  bool Edges = false;

  //
  // Use compare coverage (memcmp, strcmp, ...) (only with bxcpu).
  //

  bool Compcov = false;

  //
  // Use LAF split-compares (only with bxcpu).
  //

  LafCompcovOptions_t Laf = LafCompcovOptions_t::Disabled;

  //
  // LAF allowed ranges.
  //

  std::vector<std::pair<Gva_t, Gva_t>> LafAllowedRanges;

  //
  // Options for the subcommand 'run'.
  //

  RunOptions_t Run;

  //
  // Options for the subcommand 'fuzz'.
  //

  FuzzOptions_t Fuzz;

  //
  // Options for the subcommand 'master'.
  //

  MasterOptions_t Master;
};
