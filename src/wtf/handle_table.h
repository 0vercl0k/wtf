// Axel '0vercl0k' Souchet - March 25 2020
#pragma once
#include "fshandle_table.h"
#include "fshooks.h"
#include "globals.h"
#include "platform.h"
#include "restorable.h"
#include <cstdint>
#include <fmt/format.h>
#include <unordered_map>
#include <unordered_set>

#ifdef HANDLETABLE_LOGGING_ON
#define HandleTableDebugPrint(Format, ...)                                     \
  fmt::print("handletable: " Format, __VA_ARGS__)
#else
#define HandleTableDebugPrint(Format, ...) /* nuthin */
#endif

class HandleTable_t : public Restorable {
  uint64_t LatestGuestHandle_;
  uint64_t SavedLatestGuestHandle_;

  //
  // This maps a guest handle to a host handle.
  //

  std::unordered_map<HANDLE, HANDLE> HandleMapping_;

  //
  // Same as above.
  //

  std::unordered_map<HANDLE, HANDLE> SavedHandleMapping_;

  //
  // This is a list of pseudo handles; we need to guarantee that
  // AllocateGuestHandle doesn't generate one of them.
  //

  std::unordered_set<uint32_t> PseudoHandles_;

  //
  // This is a list of handles that we don't want the AllocateGuestHandle
  // function to generate.
  //

  std::unordered_set<HANDLE> ReservedHandles_;

public:
  //
  // This is the last guest handle we can generate. The allocator go from there
  // downwards.
  //

  static const uint64_t LastGuestHandle = 0x7ffffffeULL;

  //
  // ctor.
  //

  HandleTable_t()
      : LatestGuestHandle_(LastGuestHandle),
        SavedLatestGuestHandle_(LastGuestHandle) {

    //
    // Do not clash with the pseudo handles (kernelbase!GetFileType uses them
    // for example).
    //

    PseudoHandles_.emplace(STD_INPUT_HANDLE);
    PseudoHandles_.emplace(STD_OUTPUT_HANDLE);
    PseudoHandles_.emplace(STD_ERROR_HANDLE);
  }

  void Save() override {

    //
    // Save the fs hooks.
    //

    g_FsHandleTable.Save();

    //
    // Save our state.
    //

    SavedLatestGuestHandle_ = LatestGuestHandle_;
    SavedHandleMapping_ = HandleMapping_;
  }

  void Restore() override {

    //
    // Restore the fs hooks.
    //

    g_FsHandleTable.Restore();

    //
    // Walk the handles that haven't been saved and close them all.
    //

    for (const auto &[GuestHandle, HostHandle] : HandleMapping_) {
      if (SavedHandleMapping_.contains(GuestHandle)) {
        continue;
      }

      HandleTableDebugPrint("FYI {} hasn't been closed.\n", GuestHandle);
      CloseGuestHandle(HostHandle);
    }

    //
    // Restore our state.
    //

    LatestGuestHandle_ = SavedLatestGuestHandle_;
    HandleMapping_ = SavedHandleMapping_;
  }

  bool Has(const HANDLE GuestHandle) {
    return HandleMapping_.contains(GuestHandle);
  }

  HANDLE AllocateGuestHandle() {
    HANDLE GuestHandle = nullptr;
    while (1) {
      GuestHandle = HANDLE(LatestGuestHandle_);
      const uint32_t LowerDword = uint32_t(LatestGuestHandle_);

      LatestGuestHandle_--;
      if (PseudoHandles_.contains(LowerDword) ||
          ReservedHandles_.contains(GuestHandle)) {
        continue;
      }

      break;
    }

    return GuestHandle;
  }

  bool AddHandle(const HANDLE GuestHandle, const HANDLE HostHandle) {
    //
    // Add a mapping between a guest handle and a host handle.
    //

    return HandleMapping_.emplace(GuestHandle, HostHandle).second;
  }

  bool CloseGuestHandle(const HANDLE GuestHandle) {

    //
    // Check if we know the handle.
    //

    if (!HandleMapping_.contains(GuestHandle)) {
      return false;
    }

    //
    // If this was a tracked handle (ghost or not), we can now remove it from
    // our state.
    //

    HandleMapping_.erase(GuestHandle);

    //
    // Let the other subsystems know about it too so that they can keep their
    // state in sync.
    //

    if (g_FsHandleTable.Known(GuestHandle)) {
      if (!g_FsHandleTable.CloseGuestHandle(GuestHandle)) {
        __debugbreak();
        return false;
      }
    }

    return true;
  }
};

//
// The global handle table.
//

extern HandleTable_t g_HandleTable;