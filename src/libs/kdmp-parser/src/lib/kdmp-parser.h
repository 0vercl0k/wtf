// Axel '0vercl0k' Souchet - February 15 2019
#pragma once

#include "filemap.h"
#include "kdmp-parser-structs.h"
#include "kdmp-parser-version.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

namespace kdmpparser {

using Page_t = std::array<uint8_t, kdmpparser::Page::Size>;
using Physmem_t = std::unordered_map<uint64_t, const uint8_t *>;

struct BugCheckParameters_t {
  uint32_t BugCheckCode;
  std::array<uint64_t, 4> BugCheckCodeParameter;
};

class KernelDumpParser {

  //
  // The mapped file.
  //

  FileMap_t FileMap_;

  //
  // Header of the crash-dump.
  //

  HEADER64 *DmpHdr_ = nullptr;

  //
  // File path to the crash-dump.
  //

  std::filesystem::path PathFile_;

  //
  // Mapping between physical addresses / page data.
  //

  Physmem_t Physmem_;

public:
  //
  // Actually do the parsing of the file.
  //

  bool Parse(const char *PathFile) {

    //
    // Copy the path file.
    //

    PathFile_ = std::filesystem::path(PathFile);
    if (!std::filesystem::exists(PathFile_)) {
      printf("Invalid file: %s.\n", (char *)PathFile_.string().c_str());
      return false;
    }

    //
    // Map a view of the file.
    //

    if (!MapFile()) {
      printf("MapFile failed.\n");
      return false;
    }

    //
    // Parse the DMP_HEADER.
    //

    if (!ParseDmpHeader()) {
      printf("ParseDmpHeader failed.\n");
      return false;
    }

    //
    // Retrieve the physical memory according to the type of dump we have.
    //

    switch (DmpHdr_->DumpType) {
    case DumpType_t::FullDump: {
      if (!BuildPhysmemFullDump()) {
        printf("BuildPhysmemFullDump failed.\n");
        return false;
      }
      break;
    }
    case DumpType_t::LiveKernelBitmapDump:
    case DumpType_t::BMPDump: {
      if (!BuildPhysmemBMPDump()) {
        printf("BuildPhysmemBMPDump failed.\n");
        return false;
      }
      break;
    }

    case DumpType_t::CompleteMemoryDump:
    case DumpType_t::KernelAndUserMemoryDump:
    case DumpType_t::KernelMemoryDump: {
      if (!BuildPhysicalMemoryFromDump(DmpHdr_->DumpType)) {
        printf("BuildPhysicalMemoryFromDump failed.\n");
        return false;
      }
      break;
    }

    default: {
      printf("Invalid type\n");
      return false;
    }
    }

    return true;
  }

  //
  // Give the Context record to the user.
  //

  constexpr const CONTEXT &GetContext() const {

    //
    // Give the user a view of the context record.
    //

    return DmpHdr_->u2.ContextRecord;
  }

  //
  // Give the bugcheck parameters to the user.
  //

  constexpr BugCheckParameters_t GetBugCheckParameters() const {

    //
    // Give the user a view of the bugcheck parameters.
    //

    return {DmpHdr_->BugCheckCode,
            {DmpHdr_->BugCheckCodeParameters[0],
             DmpHdr_->BugCheckCodeParameters[1],
             DmpHdr_->BugCheckCodeParameters[2],
             DmpHdr_->BugCheckCodeParameters[3]}};
  }

  //
  // Get the path of dump.
  //

  const std::filesystem::path &GetDumpPath() const { return PathFile_; }

  //
  // Get the type of dump.
  //

  constexpr DumpType_t GetDumpType() const { return DmpHdr_->DumpType; }

  //
  // Get the physmem.
  //

  constexpr const Physmem_t &GetPhysmem() const { return Physmem_; }

  //
  // Show the exception record.
  //

  void ShowExceptionRecord(const uint32_t Prefix) const {
    DmpHdr_->Exception.Show(Prefix);
  }

  //
  // Show the context record.
  //

  void ShowContextRecord(const uint32_t Prefix) const {
    const CONTEXT &Context = GetContext();
    printf("%*srax=%016" PRIx64 " rbx=%016" PRIx64 " rcx=%016" PRIx64 "\n",
           Prefix, "", Context.Rax, Context.Rbx, Context.Rcx);
    printf("%*srdx=%016" PRIx64 " rsi=%016" PRIx64 " rdi=%016" PRIx64 "\n",
           Prefix, "", Context.Rdx, Context.Rsi, Context.Rdi);
    printf("%*srip=%016" PRIx64 " rsp=%016" PRIx64 " rbp=%016" PRIx64 "\n",
           Prefix, "", Context.Rip, Context.Rsp, Context.Rbp);
    printf("%*s r8=%016" PRIx64 "  r9=%016" PRIx64 " r10=%016" PRIx64 "\n",
           Prefix, "", Context.R8, Context.R9, Context.R10);
    printf("%*sr11=%016" PRIx64 " r12=%016" PRIx64 " r13=%016" PRIx64 "\n",
           Prefix, "", Context.R11, Context.R12, Context.R13);
    printf("%*sr14=%016" PRIx64 " r15=%016" PRIx64 "\n", Prefix, "",
           Context.R14, Context.R15);
    printf("%*scs=%04x ss=%04x ds=%04x es=%04x fs=%04x gs=%04x    "
           "             efl=%08x\n",
           Prefix, "", Context.SegCs, Context.SegSs, Context.SegDs,
           Context.SegEs, Context.SegFs, Context.SegGs, Context.EFlags);
    printf("%*sfpcw=%04x    fpsw=%04x    fptw=%04x\n", Prefix, "",
           Context.ControlWord, Context.StatusWord, 1);
    printf("%*s  st0=%016" PRIx64 "%016" PRIx64 "       st1=%016" PRIx64
           "%016" PRIx64 "\n",
           Prefix, "", Context.FloatRegisters[0].High,
           Context.FloatRegisters[0].Low, Context.FloatRegisters[1].High,
           Context.FloatRegisters[1].Low);
    printf("%*s  st2=%016" PRIx64 "%016" PRIx64 "       st3=%016" PRIx64
           "%016" PRIx64 "\n",
           Prefix, "", Context.FloatRegisters[2].High,
           Context.FloatRegisters[2].Low, Context.FloatRegisters[3].High,
           Context.FloatRegisters[3].Low);
    printf("%*s  st4=%016" PRIx64 "%016" PRIx64 "       st5=%016" PRIx64
           "%016" PRIx64 "\n",
           Prefix, "", Context.FloatRegisters[4].High,
           Context.FloatRegisters[4].Low, Context.FloatRegisters[5].High,
           Context.FloatRegisters[5].Low);
    printf("%*s  st6=%016" PRIx64 "%016" PRIx64 "       st7=%016" PRIx64
           "%016" PRIx64 "\n",
           Prefix, "", Context.FloatRegisters[6].High,
           Context.FloatRegisters[6].Low, Context.FloatRegisters[7].High,
           Context.FloatRegisters[7].Low);
    printf("%*s xmm0=%016" PRIx64 "%016" PRIx64 "      xmm1=%016" PRIx64
           "%016" PRIx64 "\n",
           Prefix, "", Context.Xmm0.High, Context.Xmm0.Low, Context.Xmm1.High,
           Context.Xmm1.Low);
    printf("%*s xmm2=%016" PRIx64 "%016" PRIx64 "      xmm3=%016" PRIx64
           "%016" PRIx64 "\n",
           Prefix, "", Context.Xmm2.High, Context.Xmm2.Low, Context.Xmm3.High,
           Context.Xmm3.Low);
    printf("%*s xmm4=%016" PRIx64 "%016" PRIx64 "      xmm5=%016" PRIx64
           "%016" PRIx64 "\n",
           Prefix, "", Context.Xmm4.High, Context.Xmm4.Low, Context.Xmm5.High,
           Context.Xmm5.Low);
    printf("%*s xmm6=%016" PRIx64 "%016" PRIx64 "      xmm7=%016" PRIx64
           "%016" PRIx64 "\n",
           Prefix, "", Context.Xmm6.High, Context.Xmm6.Low, Context.Xmm7.High,
           Context.Xmm7.Low);
    printf("%*s xmm8=%016" PRIx64 "%016" PRIx64 "      xmm9=%016" PRIx64
           "%016" PRIx64 "\n",
           Prefix, "", Context.Xmm8.High, Context.Xmm8.Low, Context.Xmm9.High,
           Context.Xmm9.Low);
    printf("%*sxmm10=%016" PRIx64 "%016" PRIx64 "     xmm11=%016" PRIx64
           "%016" PRIx64 "\n",
           Prefix, "", Context.Xmm10.High, Context.Xmm10.Low,
           Context.Xmm11.High, Context.Xmm11.Low);
    printf("%*sxmm12=%016" PRIx64 "%016" PRIx64 "     xmm13=%016" PRIx64
           "%016" PRIx64 "\n",
           Prefix, "", Context.Xmm12.High, Context.Xmm12.Low,
           Context.Xmm13.High, Context.Xmm13.Low);
    printf("%*sxmm14=%016" PRIx64 "%016" PRIx64 "     xmm15=%016" PRIx64
           "%016" PRIx64 "\n",
           Prefix, "", Context.Xmm14.High, Context.Xmm14.Low,
           Context.Xmm15.High, Context.Xmm15.Low);
  }

  //
  // Show all the structures of the dump.
  //

  void ShowAllStructures(const uint32_t Prefix) const { DmpHdr_->Show(Prefix); }

  //
  // Get the content of a physical address.
  //

  const uint8_t *GetPhysicalPage(const uint64_t PhysicalAddress) const {

    //
    // Attempt to find the physical address.
    //

    const auto &Pair = Physmem_.find(PhysicalAddress);

    //
    // If it doesn't exist then return nullptr.
    //

    if (Pair == Physmem_.end()) {
      return nullptr;
    }

    //
    // Otherwise we return a pointer to the content of the page.
    //

    return Pair->second;
  }

  //
  // Get the directory table base.
  //

  constexpr uint64_t GetDirectoryTableBase() const {
    return DmpHdr_->DirectoryTableBase;
  }

  //
  // Translate a virtual address to physical address using a directory table
  // base.
  //

  std::optional<uint64_t>
  VirtTranslate(const uint64_t VirtualAddress,
                const uint64_t DirectoryTableBase = 0) const {

    //
    // If DirectoryTableBase is null ; use the one from the dump header and
    // clear PCID bits (bits 11:0).
    //

    uint64_t LocalDTB = Page::Align(GetDirectoryTableBase());

    if (DirectoryTableBase) {
      LocalDTB = Page::Align(DirectoryTableBase);
    }

    //
    // Stole code from @yrp604 and @0vercl0k.
    //

    const VIRTUAL_ADDRESS GuestAddress(VirtualAddress);
    const MMPTE_HARDWARE Pml4(LocalDTB);
    const uint64_t Pml4Base = Pml4.u.PageFrameNumber * Page::Size;
    const uint64_t Pml4eGpa = Pml4Base + GuestAddress.u.Pml4Index * 8;
    const MMPTE_HARDWARE Pml4e(PhyRead8(Pml4eGpa));
    if (!Pml4e.u.Present) {
      printf("Invalid page map level 4, address translation failed!\n");
      return {};
    }

    const uint64_t PdptBase = Pml4e.u.PageFrameNumber * Page::Size;
    const uint64_t PdpteGpa = PdptBase + GuestAddress.u.PdPtIndex * 8;
    const MMPTE_HARDWARE Pdpte(PhyRead8(PdpteGpa));
    if (!Pdpte.u.Present) {
      printf("Invalid page directory pointer table, address translation "
             "failed!\n");
      return {};
    }

    //
    // huge pages:
    // 7 (PS) - Page size; must be 1 (otherwise, this entry references a page
    // directory; see Table 4-1
    //

    const uint64_t PdBase = Pdpte.u.PageFrameNumber * Page::Size;
    if (Pdpte.u.LargePage) {
      return PdBase + (VirtualAddress & 0x3fff'ffff);
    }

    const uint64_t PdeGpa = PdBase + GuestAddress.u.PdIndex * 8;
    const MMPTE_HARDWARE Pde(PhyRead8(PdeGpa));
    if (!Pde.u.Present) {
      printf("Invalid page directory entry, address translation failed!\n");
      return {};
    }

    //
    // large pages:
    // 7 (PS) - Page size; must be 1 (otherwise, this entry references a page
    // table; see Table 4-18
    //

    const uint64_t PtBase = Pde.u.PageFrameNumber * Page::Size;
    if (Pde.u.LargePage) {
      return PtBase + (VirtualAddress & 0x1f'ffff);
    }

    const uint64_t PteGpa = PtBase + GuestAddress.u.PtIndex * 8;
    const MMPTE_HARDWARE Pte(PhyRead8(PteGpa));
    if (!Pte.u.Present) {
      printf("Invalid page table entry, address translation failed!\n");
      return {};
    }

    const uint64_t PageBase = Pte.u.PageFrameNumber * Page::Size;
    return PageBase + GuestAddress.u.Offset;
  }

  //
  // Get the content of a virtual address.
  //

  const uint8_t *GetVirtualPage(const uint64_t VirtualAddress,
                                const uint64_t DirectoryTableBase = 0) const {

    //
    // First remove offset and translate the virtual address.
    //

    const auto &PhysicalAddress =
        VirtTranslate(Page::Align(VirtualAddress), DirectoryTableBase);

    if (!PhysicalAddress) {
      return nullptr;
    }

    //
    // Then get the physical page.
    //

    return GetPhysicalPage(*PhysicalAddress);
  }

  const HEADER64 &GetDumpHeader() const {
    if (!DmpHdr_) {
      std::abort();
    }

    return *DmpHdr_;
  }

private:
  //
  // Utility function to read an uint64_t from a physical address.
  //

  uint64_t PhyRead8(const uint64_t PhysicalAddress) const {

    //
    // Get the physical page and read from the offset.
    //

    const uint8_t *PhysicalPage = GetPhysicalPage(Page::Align(PhysicalAddress));

    if (!PhysicalPage) {
      printf("Internal page table parsing failed!\n");
      return 0;
    }

    const uint64_t *Ptr =
        (uint64_t *)(PhysicalPage + Page::Offset(PhysicalAddress));
    return *Ptr;
  }

  //
  // Build a map of physical addresses / page data pointers for full dump.
  //

  bool BuildPhysmemFullDump() {

    //
    // Walk through the runs.
    //

    uint8_t *RunBase = (uint8_t *)&DmpHdr_->u3.BmpHeader;
    const uint32_t NumberOfRuns = DmpHdr_->u1.PhysicalMemoryBlock.NumberOfRuns;

    //
    // Back at it, this time building the index!
    //

    for (uint32_t RunIdx = 0; RunIdx < NumberOfRuns; RunIdx++) {

      //
      // Grab the current run as well as its base page and page count.
      //

      const PHYSMEM_RUN *Run = DmpHdr_->u1.PhysicalMemoryBlock.Run + RunIdx;

      const uint64_t BasePage = Run->BasePage;
      const uint64_t PageCount = Run->PageCount;

      //
      // Walk the pages from the run.
      //

      for (uint64_t PageIdx = 0; PageIdx < PageCount; PageIdx++) {

        //
        // Compute the current PFN as well as the actual physical address of
        // the page.
        //

        const uint64_t Pfn = BasePage + PageIdx;
        const uint64_t Pa = Pfn * Page::Size;

        //
        // Now one thing to understand is that the Runs structure allows to
        // skip for holes in memory. Instead of, padding them with empty
        // spaces to conserve a 1:1 mapping between physical address and file
        // offset, the Run gives you the base Pfn. This means that we don't
        // have a 1:1 mapping between file offset and physical addresses so we
        // need to keep track of where the Run starts in memory and then we
        // can simply access our pages one after the other.
        //
        // If this is not clear enough, here is a small example:
        //  Run[0]
        //    BasePage = 1337, PageCount = 2
        //  Run[1]
        //    BasePage = 1400, PageCount = 1
        //
        // In the above we clearly see that there is a hole between the two
        // runs; the dump file has 2+1 memory pages. Their Pfns are: 1337+0,
        // 1337+1, 1400+0.
        //
        // Now if we want to get the file offset of those pages we start at
        // Run0:
        //   Run0 starts at file offset 0x2000 so Page0 is at file offset
        //   0x2000, Page1 is at file offset 0x3000. Run1 starts at file
        //   offset 0x2000+(2*0x1000) so Page3 is at file offset
        //   0x2000+(2*0x1000)+0x1000.
        //
        // That is the reason why the computation below is RunBase + (PageIdx
        // * 0x1000) instead of RunBase + (Pfn * 0x1000).

        const uint8_t *PageBase = RunBase + (PageIdx * Page::Size);

        //
        // Map the Pfn to a page.
        //

        Physmem_.try_emplace(Pa, PageBase);
      }

      //
      // Move the run base past all the pages in the current run.
      //

      RunBase += PageCount * Page::Size;
    }

    return true;
  }

  //
  // Build a map of physical addresses / page data pointers for BMP dump.
  //

  bool BuildPhysmemBMPDump() {
    const uint8_t *Page = (uint8_t *)DmpHdr_ + DmpHdr_->u3.BmpHeader.FirstPage;
    const uint64_t BitmapSize = DmpHdr_->u3.BmpHeader.Pages / 8;
    const uint8_t *Bitmap = DmpHdr_->u3.BmpHeader.Bitmap.data();

    //
    // Walk the bitmap byte per byte.
    //

    for (uint64_t BitmapIdx = 0; BitmapIdx < BitmapSize; BitmapIdx++) {

      //
      // Now walk the bits of the current byte.
      //

      const uint8_t Byte = Bitmap[BitmapIdx];
      for (uint8_t BitIdx = 0; BitIdx < 8; BitIdx++) {

        //
        // If the bit is not set we just skip to the next.
        //

        const bool BitSet = ((Byte >> BitIdx) & 1) == 1;
        if (!BitSet) {
          continue;
        }

        //
        // If the bit is one we add the page to the physmem.
        //

        const uint64_t Pfn = (BitmapIdx * 8) + BitIdx;
        const uint64_t Pa = Pfn * Page::Size;
        Physmem_.try_emplace(Pa, Page);
        Page += Page::Size;
      }
    }

    return true;
  }

  //
  // Populate the physical memory map for the 'new' dump types.
  // `Type` must be either `KernelMemoryDump`, `KernelAndUserMemoryDump`,
  // or `CompleteMemoryDump`.
  //
  // Returns true on success, false otherwise.
  //

  bool BuildPhysicalMemoryFromDump(const DumpType_t Type) {
    uint64_t FirstPageOffset = 0;
    uint8_t *Page = nullptr;
    uint64_t MetadataSize = 0;
    uint8_t *Bitmap = nullptr;
    uint64_t TotalNumberOfPages = 0;
    uint64_t CurrentPageCount = 0;

    switch (Type) {
    case DumpType_t::KernelMemoryDump:
    case DumpType_t::KernelAndUserMemoryDump: {
      FirstPageOffset = DmpHdr_->u3.RdmpHeader.Hdr.FirstPageOffset;
      Page = (uint8_t *)DmpHdr_ + FirstPageOffset;
      MetadataSize = DmpHdr_->u3.RdmpHeader.Hdr.MetadataSize;
      Bitmap = DmpHdr_->u3.RdmpHeader.Bitmap.data();
      break;
    }

    case DumpType_t::CompleteMemoryDump: {
      FirstPageOffset = DmpHdr_->u3.FullRdmpHeader.Hdr.FirstPageOffset;
      Page = (uint8_t *)DmpHdr_ + FirstPageOffset;
      MetadataSize = DmpHdr_->u3.FullRdmpHeader.Hdr.MetadataSize;
      Bitmap = DmpHdr_->u3.FullRdmpHeader.Bitmap.data();
      TotalNumberOfPages = DmpHdr_->u3.FullRdmpHeader.TotalNumberOfPages;
      break;
    }

    default: {
      return false;
    }
    }

    if (!FirstPageOffset || !Page || !MetadataSize || !Bitmap) {
      return false;
    }

    auto IsPageInBounds = [&](const uint8_t *Ptr) {
      return FileMap_.InBounds(Ptr, Page::Size);
    };

    if (!IsPageInBounds(Page)) {
      return false;
    }

    struct PfnRange {
      uint64_t PageFileNumber;
      uint64_t NumberOfPages;
    };

    // Sanity check
    if (MetadataSize % sizeof(PfnRange)) {
      return false;
    }

    for (uint64_t Offset = 0; Offset < MetadataSize;
         Offset += sizeof(PfnRange)) {

      if (Type == DumpType_t::CompleteMemoryDump) {
        // `CompleteMemoryDump` type seems to be bound by the
        // `TotalNumberOfPages` field, *not* by `MetadataSize`.
        if (CurrentPageCount == TotalNumberOfPages) {
          break;
        }

        if (CurrentPageCount > TotalNumberOfPages) {
          return false;
        }
      }

      const PfnRange &Entry = (PfnRange &)Bitmap[Offset];
      if (!FileMap_.InBounds(&Entry, sizeof(Entry))) {
        return false;
      }

      CurrentPageCount += Entry.NumberOfPages;

      const uint64_t Pfn = Entry.PageFileNumber;
      if (!Pfn) {
        break;
      }

      for (uint64_t PageIdx = 0; PageIdx < Entry.NumberOfPages; PageIdx++) {
        if (!IsPageInBounds(Page)) {
          return false;
        }

        const uint64_t Pa = (Pfn * Page::Size) + (PageIdx * Page::Size);
        Physmem_.try_emplace(Pa, Page);
        Page += Page::Size;
      }
    }

    return true;
  }

  //
  // Parse the DMP_HEADER.
  //

  bool ParseDmpHeader() {

    //
    // The base of the view points on the HEADER64.
    //

    DmpHdr_ = (HEADER64 *)FileMap_.ViewBase();

    //
    // Now let's make sure the structures look right.
    //

    if (!DmpHdr_->LooksGood()) {
      printf("The header looks wrong.\n");
      return false;
    }

    return true;
  }

  //
  // Map a view of the file in memory.
  //

  bool MapFile() { return FileMap_.MapFile(PathFile_.string().c_str()); }
};

struct Version_t {
  static inline const uint16_t Major = KDMPPARSER_VERSION_MAJOR;
  static inline const uint16_t Minor = KDMPPARSER_VERSION_MINOR;
  static inline const uint16_t Patch = KDMPPARSER_VERSION_PATCH;
  static inline const std::string Release = KDMPPARSER_VERSION_RELEASE;
};

} // namespace kdmpparser
