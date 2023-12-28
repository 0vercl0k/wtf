#pragma once

#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <new>

static const uint32_t BX_INSTR_IS_JMP = 10;

static const uint32_t BOCHSCPU_INSTR_IS_JMP_INDIRECT = 11;

static const uint32_t BOCHSCPU_INSTR_IS_CALL = 12;

static const uint32_t BOCHSCPU_INSTR_IS_CALL_INDIRECT = 13;

static const uint32_t BOCHSCPU_INSTR_IS_RET = 14;

static const uint32_t BOCHSCPU_INSTR_IS_IRET = 15;

static const uint32_t BOCHSCPU_INSTR_IS_INT = 16;

static const uint32_t BOCHSCPU_INSTR_IS_SYSCALL = 17;

static const uint32_t BOCHSCPU_INSTR_IS_SYSRET = 18;

static const uint32_t BOCHSCPU_INSTR_IS_SYSENTER = 19;

static const uint32_t BOCHSCPU_INSTR_IS_SYSEXIT = 20;

static const uint32_t BOCHSCPU_HOOK_MEM_READ = 0;

static const uint32_t BOCHSCPU_HOOK_MEM_WRITE = 1;

static const uint32_t BOCHSCPU_HOOK_MEM_EXECUTE = 2;

static const uint32_t BOCHSCPU_HOOK_MEM_RW = 3;

static const uint32_t BOCHSCPU_HOOK_TLB_CR0 = 10;

static const uint32_t BOCHSCPU_HOOK_TLB_CR3 = 11;

static const uint32_t BOCHSCPU_HOOK_TLB_CR4 = 12;

static const uint32_t BOCHSCPU_HOOK_TLB_TASKSWITCH = 13;

static const uint32_t BOCHSCPU_HOOK_TLB_CONTEXTSWITCH = 14;

static const uint32_t BOCHSCPU_HOOK_TLB_INVLPG = 15;

static const uint32_t BOCHSCPU_HOOK_TLB_INVEPT = 16;

static const uint32_t BOCHSCPU_HOOK_TLB_INVVPID = 17;

static const uint32_t BOCHSCPU_HOOK_TLB_INVPCID = 18;

static const uint32_t BOCHSCPU_OPCODE_ERROR = 0;

static const uint32_t BOCHSCPU_OPCODE_INSERTED = 1;

enum class DisasmStyle : uint32_t {
  Intel = 0,
  Gas = 1,
};

enum class GpRegs : uint32_t {
  Rax = 0,
  Rcx = 1,
  Rdx = 2,
  Rbx = 3,
  Rsp = 4,
  Rbp = 5,
  Rsi = 6,
  Rdi = 7,
  R8 = 8,
  R9 = 9,
  R10 = 10,
  R11 = 11,
  R12 = 12,
  R13 = 13,
  R14 = 14,
  R15 = 15,
};

using bochscpu_cpu_t = void*;

/// FFI Hook object
///
/// Full desciptions of hook points can be found here:
/// http://bochs.sourceforge.net/cgi-bin/lxr/source/instrument/instrumentation.txt
///
/// If the hook value is NULL it will be treated as a no-op. The value of the
/// ctx field will be passed as the first paramter to every hook and is fully
/// controlled by the API author
struct bochscpu_hooks_t {
  void *ctx;
  void (*reset)(void*, uint32_t, uint32_t);
  void (*hlt)(void*, uint32_t);
  void (*mwait)(void*, uint32_t, uint64_t, uintptr_t, uint32_t);
  void (*cnear_branch_taken)(void*, uint32_t, uint64_t, uint64_t);
  void (*cnear_branch_not_taken)(void*, uint32_t, uint64_t, uint64_t);
  void (*ucnear_branch)(void*, uint32_t, uint32_t, uint64_t, uint64_t);
  void (*far_branch)(void*, uint32_t, uint32_t, uint16_t, uint64_t, uint16_t, uint64_t);
  void (*opcode)(void*, uint32_t, const void*, const uint8_t*, uintptr_t, bool, bool);
  void (*interrupt)(void*, uint32_t, uint32_t);
  void (*exception)(void*, uint32_t, uint32_t, uint32_t);
  void (*hw_interrupt)(void*, uint32_t, uint32_t, uint16_t, uint64_t);
  void (*tlb_cntrl)(void*, uint32_t, uint32_t, uint64_t);
  void (*cache_cntrl)(void*, uint32_t, uint32_t);
  void (*prefetch_hint)(void*, uint32_t, uint32_t, uint32_t, uint64_t);
  void (*clflush)(void*, uint32_t, uint64_t, uint64_t);
  void (*before_execution)(void*, uint32_t, void*);
  void (*after_execution)(void*, uint32_t, void*);
  void (*repeat_iteration)(void*, uint32_t, void*);
  void (*inp)(void*, uint16_t, uintptr_t);
  void (*inp2)(void*, uint16_t, uintptr_t, uint32_t);
  void (*outp)(void*, uint16_t, uintptr_t, uint32_t);
  void (*lin_access)(void*, uint32_t, uint64_t, uint64_t, uintptr_t, uint32_t, uint32_t);
  void (*phy_access)(void*, uint32_t, uint64_t, uintptr_t, uint32_t, uint32_t);
  void (*wrmsr)(void*, uint32_t, uint32_t, uint64_t);
  void (*vmexit)(void*, uint32_t, uint32_t, uint64_t);
};

using Address = uint64_t;

struct Seg {
  bool present;
  uint16_t selector;
  Address base;
  uint32_t limit;
  uint16_t attr;
};

struct GlobalSeg {
  Address base;
  uint16_t limit;
};

struct Zmm {
  uint64_t q[8];
};

struct State {
  uint64_t bochscpu_seed;
  uint64_t rax;
  uint64_t rcx;
  uint64_t rdx;
  uint64_t rbx;
  uint64_t rsp;
  uint64_t rbp;
  uint64_t rsi;
  uint64_t rdi;
  uint64_t r8;
  uint64_t r9;
  uint64_t r10;
  uint64_t r11;
  uint64_t r12;
  uint64_t r13;
  uint64_t r14;
  uint64_t r15;
  uint64_t rip;
  uint64_t rflags;
  Seg es;
  Seg cs;
  Seg ss;
  Seg ds;
  Seg fs;
  Seg gs;
  Seg ldtr;
  Seg tr;
  GlobalSeg gdtr;
  GlobalSeg idtr;
  uint32_t cr0;
  uint64_t cr2;
  uint64_t cr3;
  uint32_t cr4;
  uint64_t cr8;
  uint64_t dr0;
  uint64_t dr1;
  uint64_t dr2;
  uint64_t dr3;
  uint32_t dr6;
  uint32_t dr7;
  uint32_t xcr0;
  Zmm zmm[32];
  uint16_t fpcw;
  uint16_t fpsw;
  uint16_t fptw;
  uint16_t fpop;
  uint64_t fpst[8];
  uint32_t mxcsr;
  uint32_t mxcsr_mask;
  uint64_t tsc;
  uint32_t efer;
  uint64_t kernel_gs_base;
  uint64_t apic_base;
  uint64_t pat;
  uint64_t sysenter_cs;
  uint64_t sysenter_eip;
  uint64_t sysenter_esp;
  uint64_t star;
  uint64_t lstar;
  uint64_t cstar;
  uint64_t sfmask;
  uint64_t tsc_aux;
};

using bochscpu_cpu_state_t = State;

using bochscpu_cpu_seg_t = Seg;

using bochscpu_cpu_global_seg_t = GlobalSeg;

using bochscpu_cpu_zmm_t = Zmm;

using bochscpu_instr_t = const void*;

extern "C" {

/// Create a new Cpu
///
/// Create a new Cpu with the specified id. If SMP is not enabled, the id is
/// ignored.
bochscpu_cpu_t bochscpu_cpu_new(uint32_t id);

/// Create a new Cpu
///
/// Instantiate an already existing cpu with the specified id.
bochscpu_cpu_t bochscpu_cpu_from(uint32_t id);

void bochscpu_cpu_forget(bochscpu_cpu_t p);

/// Delete a cpu
void bochscpu_cpu_delete(bochscpu_cpu_t p);

void bochscpu_cpu_set_mode(bochscpu_cpu_t p);

uint32_t bochscpu_total_gpregs();

/// Start emulation
///
/// To hook emulation, pass in a NULL terminated list of one or more pointers to
/// bochscpu_hooks_t structs.
void bochscpu_cpu_run(bochscpu_cpu_t p, bochscpu_hooks_t **h);

/// Stop emulation
///
void bochscpu_cpu_stop(bochscpu_cpu_t p);

void bochscpu_cpu_state(bochscpu_cpu_t p, bochscpu_cpu_state_t *s);

void bochscpu_cpu_set_state(bochscpu_cpu_t p, const bochscpu_cpu_state_t *s);

void bochscpu_cpu_set_state_no_flush(bochscpu_cpu_t p, const bochscpu_cpu_state_t *s);

void bochscpu_cpu_set_exception(bochscpu_cpu_t p, uint32_t vector, uint16_t error);

uint64_t bochscpu_get_reg64(bochscpu_cpu_t p, GpRegs reg);

void bochscpu_set_reg64(bochscpu_cpu_t p, GpRegs reg, uint64_t val);

uint32_t bochscpu_get_reg32(bochscpu_cpu_t p, GpRegs reg);

void bochscpu_set_reg32(bochscpu_cpu_t p, GpRegs reg, uint32_t val);

uint16_t bochscpu_get_reg16(bochscpu_cpu_t p, GpRegs reg);

void bochscpu_set_reg16(bochscpu_cpu_t p, GpRegs reg, uint16_t val);

uint64_t bochscpu_cpu_rax(bochscpu_cpu_t p);

void bochscpu_cpu_set_rax(bochscpu_cpu_t p, uint64_t val);

uint64_t bochscpu_cpu_rcx(bochscpu_cpu_t p);

void bochscpu_cpu_set_rcx(bochscpu_cpu_t p, uint64_t val);

uint64_t bochscpu_cpu_rdx(bochscpu_cpu_t p);

void bochscpu_cpu_set_rdx(bochscpu_cpu_t p, uint64_t val);

uint64_t bochscpu_cpu_rbx(bochscpu_cpu_t p);

void bochscpu_cpu_set_rbx(bochscpu_cpu_t p, uint64_t val);

uint64_t bochscpu_cpu_rsp(bochscpu_cpu_t p);

void bochscpu_cpu_set_rsp(bochscpu_cpu_t p, uint64_t val);

uint64_t bochscpu_cpu_rbp(bochscpu_cpu_t p);

void bochscpu_cpu_set_rbp(bochscpu_cpu_t p, uint64_t val);

uint64_t bochscpu_cpu_rsi(bochscpu_cpu_t p);

void bochscpu_cpu_set_rsi(bochscpu_cpu_t p, uint64_t val);

uint64_t bochscpu_cpu_rdi(bochscpu_cpu_t p);

void bochscpu_cpu_set_rdi(bochscpu_cpu_t p, uint64_t val);

uint64_t bochscpu_cpu_r8(bochscpu_cpu_t p);

void bochscpu_cpu_set_r8(bochscpu_cpu_t p, uint64_t val);

uint64_t bochscpu_cpu_r9(bochscpu_cpu_t p);

void bochscpu_cpu_set_r9(bochscpu_cpu_t p, uint64_t val);

uint64_t bochscpu_cpu_r10(bochscpu_cpu_t p);

void bochscpu_cpu_set_r10(bochscpu_cpu_t p, uint64_t val);

uint64_t bochscpu_cpu_r11(bochscpu_cpu_t p);

void bochscpu_cpu_set_r11(bochscpu_cpu_t p, uint64_t val);

uint64_t bochscpu_cpu_r12(bochscpu_cpu_t p);

void bochscpu_cpu_set_r12(bochscpu_cpu_t p, uint64_t val);

uint64_t bochscpu_cpu_r13(bochscpu_cpu_t p);

void bochscpu_cpu_set_r13(bochscpu_cpu_t p, uint64_t val);

uint64_t bochscpu_cpu_r14(bochscpu_cpu_t p);

void bochscpu_cpu_set_r14(bochscpu_cpu_t p, uint64_t val);

uint64_t bochscpu_cpu_r15(bochscpu_cpu_t p);

void bochscpu_cpu_set_r15(bochscpu_cpu_t p, uint64_t val);

uint64_t bochscpu_cpu_rip(bochscpu_cpu_t p);

void bochscpu_cpu_set_rip(bochscpu_cpu_t p, uint64_t val);

uint64_t bochscpu_cpu_rflags(bochscpu_cpu_t p);

void bochscpu_cpu_set_rflags(bochscpu_cpu_t p, uint64_t val);

void bochscpu_cpu_es(bochscpu_cpu_t p, bochscpu_cpu_seg_t *s);

void bochscpu_cpu_set_es(bochscpu_cpu_t p, const bochscpu_cpu_seg_t *s);

void bochscpu_cpu_cs(bochscpu_cpu_t p, bochscpu_cpu_seg_t *s);

void bochscpu_cpu_set_cs(bochscpu_cpu_t p, const bochscpu_cpu_seg_t *s);

void bochscpu_cpu_ss(bochscpu_cpu_t p, bochscpu_cpu_seg_t *s);

void bochscpu_cpu_set_ss(bochscpu_cpu_t p, const bochscpu_cpu_seg_t *s);

void bochscpu_cpu_ds(bochscpu_cpu_t p, bochscpu_cpu_seg_t *s);

void bochscpu_cpu_set_ds(bochscpu_cpu_t p, const bochscpu_cpu_seg_t *s);

void bochscpu_cpu_fs(bochscpu_cpu_t p, bochscpu_cpu_seg_t *s);

void bochscpu_cpu_set_fs(bochscpu_cpu_t p, const bochscpu_cpu_seg_t *s);

void bochscpu_cpu_gs(bochscpu_cpu_t p, bochscpu_cpu_seg_t *s);

void bochscpu_cpu_set_gs(bochscpu_cpu_t p, const bochscpu_cpu_seg_t *s);

void bochscpu_cpu_ldtr(bochscpu_cpu_t p, bochscpu_cpu_seg_t *s);

void bochscpu_cpu_set_ldtr(bochscpu_cpu_t p, const bochscpu_cpu_seg_t *s);

void bochscpu_cpu_tr(bochscpu_cpu_t p, bochscpu_cpu_seg_t *s);

void bochscpu_cpu_set_tr(bochscpu_cpu_t p, const bochscpu_cpu_seg_t *s);

void bochscpu_cpu_gdtr(bochscpu_cpu_t p, bochscpu_cpu_global_seg_t *s);

void bochscpu_cpu_set_gdtr(bochscpu_cpu_t p, const bochscpu_cpu_global_seg_t *s);

void bochscpu_cpu_idtr(bochscpu_cpu_t p, bochscpu_cpu_global_seg_t *s);

void bochscpu_cpu_set_idtr(bochscpu_cpu_t p, const bochscpu_cpu_global_seg_t *s);

uint64_t bochscpu_cpu_cr2(bochscpu_cpu_t p);

void bochscpu_cpu_set_cr2(bochscpu_cpu_t p, uint64_t val);

uint64_t bochscpu_cpu_cr3(bochscpu_cpu_t p);

void bochscpu_cpu_set_cr3(bochscpu_cpu_t p, uint64_t val);

void bochscpu_cpu_zmm(bochscpu_cpu_t p, uintptr_t idx, bochscpu_cpu_zmm_t *z);

void bochscpu_cpu_set_zmm(bochscpu_cpu_t p, uintptr_t idx, const bochscpu_cpu_zmm_t *z);

uint32_t bochscpu_instr_bx_opcode(bochscpu_instr_t p);

uint16_t bochscpu_instr_imm16(bochscpu_instr_t p);

uint32_t bochscpu_instr_imm32(bochscpu_instr_t p);

uint64_t bochscpu_instr_imm64(bochscpu_instr_t p);

uint32_t bochscpu_instr_src(bochscpu_instr_t p);

uint32_t bochscpu_instr_dst(bochscpu_instr_t p);

uint32_t bochscpu_instr_seg(bochscpu_instr_t p);

uint32_t bochscpu_instr_modC0(bochscpu_instr_t p);

uint64_t bochscpu_instr_resolve_addr(bochscpu_instr_t p);

uint32_t bochscpu_opcode_disasm(uint32_t is32,
                                uint32_t is64,
                                Address *cs_base,
                                Address *ip,
                                uint8_t *instr_bytes,
                                const char *distbuf,
                                DisasmStyle disasm_style);

/// Add GPA mapping to HVA
///
/// If the GPA was already mapped, this replaces the existing mapping
///
/// # Panics
///
/// Panics if the added page is not page aligned.
void bochscpu_mem_page_insert(uint64_t gpa, uint8_t *hva);

/// Delete GPA mapping
///
/// If the GPA is not valid, this is a no-op.
void bochscpu_mem_page_remove(uint64_t gpa);

/// Install a physical page fault handler
///
/// This function will be called any time a request is made to physical memory
/// and the GPA is not present. This function should add a page using
/// `bochscpu_mem_page_insert()`
///
/// The paramter should have the type `void handler(gpa_t)`
///
/// This allows you to lazily page in your backing physical memory.
///
/// # Note
///
/// This is a global singleton, and installing a new physical page fault
/// handler will overwrite the existing handler.
void bochscpu_mem_missing_page(void (*handler)(uint64_t gpa));

/// Translate GPA to HVA
///
/// # Panics
///
/// If the GPA does not exit, it will call the missing page handler. If no
/// missing page handler is set or the missing page handler does not add the
/// appropriate page, this will panic.
///
/// # Example
uint8_t *bochscpu_mem_phy_translate(uint64_t gpa);

/// Translate GVA to GPA
///
/// Use the provided cr3 to translate the GVA into a GPA.
///
/// # Returns
///
/// Translated gpa on success, -1 on failure
uint64_t bochscpu_mem_virt_translate(uint64_t cr3, uint64_t gva);

/// Read from GPA
///
/// # Panics
///
/// If the GPA does not exist, it will call the missing page function. If
/// that function does not exist or does not resolve the fault, this routine
/// will panic
void bochscpu_mem_phy_read(uint64_t gpa, uint8_t *hva, uintptr_t sz);

/// Write to GPA
///
/// # Panics
///
/// If the GPA does not exist, it will call the missing page function. If
/// that function does not exist or does not resolve the fault, this routine
/// will panic
void bochscpu_mem_phy_write(uint64_t gpa, const uint8_t *hva, uintptr_t sz);

/// Write to GVA
///
/// Write to GVA, using specified cr3 to translate.
///
/// # Returns
///
/// Zero on success, non-zero on failure
int32_t bochscpu_mem_virt_write(uint64_t cr3, uint64_t gva, const uint8_t *hva, uintptr_t sz);

/// Read from GVA
///
/// Read from GVA, using specified cr3 to translate.
///
/// # Returns
///
/// Zero on success, non-zero on failure
int32_t bochscpu_mem_virt_read(uint64_t cr3, uint64_t gva, uint8_t *hva, uintptr_t sz);

void bochscpu_log_set_level(uintptr_t level);

} // extern "C"
