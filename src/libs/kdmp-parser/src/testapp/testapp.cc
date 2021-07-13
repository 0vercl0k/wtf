// Axel '0vercl0k' Souchet - February 15 2019
#include "kdmp-parser.h"
#include <cstring>

int main(int argc, const char *argv[]) {
  if (argc != 2) {
    printf("test.exe <kdump path>\n");
    return EXIT_FAILURE;
  }

  kdmpparser::KernelDumpParser Dmp;
  if (!Dmp.Parse(argv[1])) {
    return EXIT_FAILURE;
  }

  //
  // kd> r
  // rax=0000000000000003 rbx=fffff8050f4e9f70 rcx=0000000000000001
  // rdx=fffff805135684d0 rsi=0000000000000100 rdi=fffff8050f4e9f80
  // rip=fffff805108776a0 rsp=fffff805135684f8 rbp=fffff80513568600
  // r8=0000000000000003  r9=fffff805135684b8 r10=0000000000000000
  // r11=ffffa8848825e000 r12=fffff8050f4e9f80 r13=fffff80510c3c958
  // r14=0000000000000000 r15=0000000000000052
  // iopl=0         nv up ei pl nz na pe nc
  // cs=0010  ss=0018  ds=002b  es=002b  fs=0053  gs=002b efl=00040202
  //

  const kdmpparser::CONTEXT *C = Dmp.GetContext();
  if (C->Rax != 0x0000000000000003ULL) {
    printf("Rax(0x%016" PRIx64 ") does not match with 0x0000000000000003.",
           C->Rax);
    return EXIT_FAILURE;
  }

  if (C->Rbx != 0xfffff8050f4e9f70ULL) {
    printf("Rbx(0x%016" PRIx64 ") does not match with 0xfffff8050f4e9f70.",
           C->Rbx);
    return EXIT_FAILURE;
  }

  if (C->Rcx != 0x0000000000000001ULL) {
    printf("Rcx(0x%016" PRIx64 ") does not match with 0x0000000000000001.",
           C->Rcx);
    return EXIT_FAILURE;
  }

  if (C->Rdx != 0xfffff805135684d0ULL) {
    printf("Rdx(0x%016" PRIx64 ") does not match with 0xfffff805135684d0.",
           C->Rdx);
    return EXIT_FAILURE;
  }

  if (C->Rsi != 0x0000000000000100ULL) {
    printf("Rsi(0x%016" PRIx64 ") does not match with 0x0000000000000100.",
           C->Rsi);
    return EXIT_FAILURE;
  }

  if (C->Rdi != 0xfffff8050f4e9f80ULL) {
    printf("Rdi(0x%016" PRIx64 ") does not match with 0xfffff8050f4e9f80.",
           C->Rdi);
    return EXIT_FAILURE;
  }

  if (C->Rip != 0xfffff805108776a0ULL) {
    printf("Rip(0x%016" PRIx64 ") does not match with 0xfffff805108776a0.",
           C->Rip);
    return EXIT_FAILURE;
  }

  if (C->Rsp != 0xfffff805135684f8ULL) {
    printf("Rsp(0x%016" PRIx64 ") does not match with 0xfffff805135684f8.",
           C->Rsp);
    return EXIT_FAILURE;
  }

  if (C->Rbp != 0xfffff80513568600ULL) {
    printf("Rbp(0x%016" PRIx64 ") does not match with 0xfffff80513568600.",
           C->Rbp);
    return EXIT_FAILURE;
  }

  if (C->R8 != 0x0000000000000003ULL) {
    printf("R8(0x%016" PRIx64 ") does not match with 0x0000000000000003.",
           C->R8);
    return EXIT_FAILURE;
  }

  if (C->R9 != 0xfffff805135684b8ULL) {
    printf("R9(0x%016" PRIx64 ") does not match with 0xfffff805135684b8.",
           C->R9);
    return EXIT_FAILURE;
  }

  if (C->R10 != 0x0000000000000000ULL) {
    printf("R10(0x%016" PRIx64 ") does not match with 0x0000000000000000.",
           C->R10);
    return EXIT_FAILURE;
  }

  if (C->R11 != 0xffffa8848825e000ULL) {
    printf("R11(0x%016" PRIx64 ") does not match with 0xffffa8848825e000.",
           C->R11);
    return EXIT_FAILURE;
  }

  if (C->R12 != 0xfffff8050f4e9f80ULL) {
    printf("R12(0x%016" PRIx64 ") does not match with 0xfffff8050f4e9f80.",
           C->R12);
    return EXIT_FAILURE;
  }

  if (C->R13 != 0xfffff80510c3c958ULL) {
    printf("R13(0x%016" PRIx64 ") does not match with 0xfffff80510c3c958.",
           C->R13);
    return EXIT_FAILURE;
  }

  if (C->R14 != 0x0000000000000000ULL) {
    printf("R14(0x%016" PRIx64 ") does not match with 0x0000000000000000.",
           C->R14);
    return EXIT_FAILURE;
  }

  if (C->R15 != 0x0000000000000052ULL) {
    printf("R15(0x%016" PRIx64 ") does not match with 0x0000000000000052.",
           C->R15);
    return EXIT_FAILURE;
  }

  printf("GPRs matches the testdatas.\n");

  const kdmpparser::DumpType_t Type = Dmp.GetDumpType();
  const auto &Physmem = Dmp.GetPhysmem();
  if (Type == kdmpparser::DumpType_t::BMPDump) {
    if (Physmem.size() != 0x544b) {
      printf("0x544b pages are expected but found %zd.\n", Physmem.size());
      return EXIT_FAILURE;
    }
  } else if (Type == kdmpparser::DumpType_t::FullDump) {
    if (Physmem.size() != 0x3fbe6) {
      printf("0x3fbe6 pages are expected but found %zd.\n", Physmem.size());
      return EXIT_FAILURE;
    }
  } else {
    printf("Unknown dump.\n");
    return EXIT_FAILURE;
  }

  const uint64_t Address = 0x6d4d22;
  const uint64_t AddressAligned = Address & 0xfffffffffffff000;
  const uint64_t AddressOffset = Address & 0xfff;
  const uint8_t ExpectedContent[] = {0x6d, 0x00, 0x00, 0x00, 0x00, 0x0a,
                                     0x63, 0x88, 0x75, 0x00, 0x00, 0x00,
                                     0x00, 0x0a, 0x63, 0x98};
  const uint8_t *Page = Dmp.GetPhysicalPage(AddressAligned);
  if (Page == nullptr) {
    printf("GetPhysicalPage failed for %p\n", (void *)Page);
    return EXIT_FAILURE;
  }

  if (memcmp(Page + AddressOffset, ExpectedContent, sizeof(ExpectedContent)) !=
      0) {
    printf("Physical memory is broken.\n");
    return EXIT_FAILURE;
  }

  printf("Physical memory page matches the testdatas.\n");
  return EXIT_SUCCESS;
}