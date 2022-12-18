// Axel '0vercl0k' Souchet - January 16 2020
#pragma once
#include "gxa.h"
#include "kdmp-parser.h"
#include "platform.h"
#include <cstdint>
#include <fmt/format.h>
#include <unordered_map>

struct Page {

  //
  // Page size.
  //

  static constexpr uint64_t Size = 0x1000;
};

//
// The RAM abstraction solves a simple problem: in order to
// track code coverage in WHV/KVM, we use breakpoints. We set breakpoints on
// every start of basic block, when we hit one we hit a new piece of code which
// is great. Those breakpoints are one shot breakpoint as we don't reapply them
// once they got triggered once. So basically, this should be a free way of
// measuring code coverage as we take a one time overhead. Now, when we need to
// restore memory the naive solution would be to restore all the dirty memory
// and reapply the remaining coverage breakpoints on those pages. This is the
// catch. The cost of reapplying breakpoints is obviously directly function of
// the number of breakpoints. After measurements, after ~500k breakpoints the
// cost of reapplying becomes pretty big. Which makes coverage breakpoints far
// from being free as we expected.
//
// This class offers a solution to this problem. Every time a breakpoint is
// added it creates a copy of the page and set the breakpoint into this fork as
// well as the current RAM view. When it is time to restore, we restore first
// pages that we have 'forked' and then fallback to restore from the
// crash-dump's physical memory.
//

class Ram_t {
  static constexpr uint64_t LargestTestedRamSize = 0x01'08'00'00'00;

  //
  // This is the kernel dump we forward requests to when we don't have our own
  // copy.
  //

  kdmpparser::KernelDumpParser Dmp_;

  //
  // This maps an aligned GPA to an HVA.
  //

  std::unordered_map<Gpa_t, uint8_t *> Cache_;

  //
  // Base of the RAM.
  //

  uint8_t *Ram_ = nullptr;

  //
  // Size of the RAM.
  //

  uint64_t RamSize_ = 0;

public:
  Ram_t() = default;

  //
  // Rule of three.
  //

  ~Ram_t() {
    for (const auto &[_, Page] : Cache_) {
      free(Page);
    }

    if (Ram_ != nullptr) {
#ifdef WINDOWS
      VirtualFree(Ram_, 0, MEM_RELEASE);
#else
      munmap(Ram_, RamSize_);
#endif
    }
  }

  Ram_t(const Ram_t &) = delete;
  Ram_t &operator=(const Ram_t &) = delete;

  //
  // Parse the dump file as well as initialize the RAM view.
  //

  [[nodiscard]] bool Populate(const fs::path &PathFile) {
    if (!Dmp_.Parse(PathFile.string().c_str())) {
      fmt::print("Parse failed\n");
      return false;
    }

    //
    // Scan the physmem to calculate the amount of RAM size we need.
    //

    const auto &Physmem = Dmp_.GetPhysmem();
    const uint64_t BiggestGpa =
        std::max_element(
            begin(Physmem), end(Physmem),
            [](const auto &A, const auto &B) { return A.first < B.first; })
            ->first;

    RamSize_ = BiggestGpa + Page::Size;
    if (RamSize_ > Ram_t::LargestTestedRamSize) {
      fmt::print("/!\\ The file size ({}) is larger than what the author "
                 "tested, running at your own risk :)!\n",
                 RamSize_);
    }

#ifdef WINDOWS
    Ram_ = (uint8_t *)VirtualAlloc(nullptr, RamSize_, MEM_RESERVE | MEM_COMMIT,
                                   PAGE_READWRITE);
#else
    Ram_ = (uint8_t *)mmap(nullptr, RamSize_, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif

    if (Ram_ == nullptr) {
      fmt::print("Failed to allocate backing memory for the RAM.\n");
      return false;
    }

    //
    // On Windows, there is no way to do on-demand paging with WHV, so we are
    // forced to fully initialized the RAM using the crash-dump.
    // On Linux with KVM we can receive faults when the guest is accessing
    // memory for the first time and so we don't need to initialize the RAM yet.
    //

#ifdef WINDOWS
    //
    // Populate the RAM using the crash-dump.
    //

    for (const auto &[AlignedGpa, Page] : Physmem) {
      uint8_t *Dest = Ram_ + AlignedGpa;
      memcpy(Dest, Page, Page::Size);
    }
#endif

    return true;
  }

  //
  // Add a breakpoint to a GPA.
  //

  uint8_t *AddBreakpoint(const Gpa_t Gpa) {
    const Gpa_t AlignedGpa = Gpa.Align();
    uint8_t *Page = nullptr;

    //
    // Grab the page if we have it in the cache
    //

    if (Cache_.contains(Gpa.Align())) {
      Page = Cache_.at(AlignedGpa);
    }

    //
    // Or allocate and initialize one!
    //

    else {
      Page = (uint8_t *)malloc(Page::Size);
      if (Page == nullptr) {
        fmt::print("Failed to allocate memory.\n");
        return nullptr;
      }

      const uint8_t *Virgin =
          Dmp_.GetPhysicalPage(AlignedGpa.U64()) + AlignedGpa.Offset().U64();
      if (Virgin == nullptr) {
        fmt::print(
            "The dump does not have a page backing GPA {:#x}, exiting.\n",
            AlignedGpa);
        return nullptr;
      }

      memcpy(Page, Virgin, Page::Size);
    }

    //
    // Apply the breakpoint.
    //

    const uint64_t Offset = Gpa.Offset().U64();
    Page[Offset] = 0xcc;
    Cache_.emplace(AlignedGpa, Page);

    //
    // And also update the RAM.
    //

    Ram_[Gpa.U64()] = 0xcc;
    return &Page[Offset];
  }

  //
  // Remove a breakpoint from a GPA.
  //

  void RemoveBreakpoint(const Gpa_t Gpa) {
    const uint8_t *Virgin = GetHvaFromDump(Gpa);
    uint8_t *Cache = GetHvaFromCache(Gpa);

    //
    // Update the RAM.
    //

    Ram_[Gpa.U64()] = *Virgin;

    //
    // Update the cache. We assume that an entry is available in the cache.
    //

    *Cache = *Virgin;
  }

  //
  // Restore a GPA from the cache or from the dump file if no entry is
  // available in the cache.
  //

  const uint8_t *Restore(const Gpa_t Gpa) {
    //
    // Get the HVA for the page we want to restore.
    //

    const uint8_t *SrcHva = GetHva(Gpa);

    //
    // Get the HVA for the page in RAM.
    //

    uint8_t *DstHva = Ram_ + Gpa.Align().U64();

    //
    // It is possible for a GPA to not exist in our cache and in the dump file.
    // For this to make sense, you have to remember that the crash-dump does not
    // contain the whole amount of RAM. In which case, the guest OS can decide
    // to allocate new memory backed by physical pages that were not dumped
    // because not currently used by the OS.
    //
    // When this happens, we simply zero initialize the page as.. this is
    // basically the best we can do. The hope is that if this behavior is not
    // correct, the rest of the execution simply explodes pretty fast.
    //

    if (!SrcHva) {
      memset(DstHva, 0, Page::Size);
    }

    //
    // Otherwise, this is straight forward, we restore the source into the
    // destination. If we had a copy, then that is what we are writing to the
    // destination, and if we didn't have a copy then we are restoring the
    // content from the crash-dump.
    //

    else {
      memcpy(DstHva, SrcHva, Page::Size);
    }

    //
    // Return the HVA to the user in case it needs to know about it.
    //

    return DstHva;
  }

  //
  // Get an HVA to the RAM.
  //

  [[nodiscard]] uint8_t *Hva() const { return Ram_; }

  //
  // Get the RAM size in bytes.
  //

  [[nodiscard]] uint64_t Size() const { return RamSize_; }

  //
  // Get an HVA for a GPA by looking at the crash-dump. Note that the return
  // pointer is const because the crash-dump is not writeable.
  //

  [[nodiscard]] const uint8_t *GetHvaFromDump(const Gpa_t Gpa) const {
    return Dmp_.GetPhysicalPage(Gpa.Align().U64()) + Gpa.Offset().U64();
  }

private:
  //
  // Get an HVA for a GPA by looking at our cache.
  //

  [[nodiscard]] uint8_t *GetHvaFromCache(const Gpa_t Gpa) {
    if (!Cache_.contains(Gpa.Align())) {
      return nullptr;
    }

    return Cache_.at(Gpa.Align()) + Gpa.Offset().U64();
  }

  //
  // Get an HVA for a GPA by looking first at our cache, and then at the
  // crash-dump.
  //

  [[nodiscard]] const uint8_t *GetHva(const Gpa_t Gpa) {
    const uint8_t *Hva = GetHvaFromCache(Gpa);
    if (Hva != nullptr) {
      return Hva;
    }

    return GetHvaFromDump(Gpa);
  }
};
