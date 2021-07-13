// Axel '0vercl0k' Souchet - April 25 2020
#include "fshandle_table.h"
#include "handle_table.h"
#include <fmt/format.h>

FsHandleTable_t::FsHandleTable_t()
    : BlacklistDecisionHandler(DefaultBlacklistDecisionHandler) {}

FsHandleTable_t::~FsHandleTable_t() {
  for (const auto &[_, GuestFile] : TrackedGuestFiles_) {
    delete GuestFile;
  }

  TrackedGuestFiles_.clear();
  SavedGuestFiles_.clear();
}

void FsHandleTable_t::Save() {
  //
  // Save all the tracked files.
  //

  for (const auto &[Filename, GuestFile] : TrackedGuestFiles_) {
    GuestFile->Save();
    SavedTrackedGuestFiles_.emplace(Filename, GuestFile);
  }

  //
  // Save the guest files.
  //

  for (const auto &[Filename, GuestFile] : GuestFiles_) {
    SavedGuestFiles_.emplace(Filename, GuestFile);
  }
}

void FsHandleTable_t::Restore() {
  //
  // The problem here is to know about the GuestFile_t instances we need to free
  // and not leak them. What we do if we walk all the tracked files and store
  // the pointers. When we restore the `TrackedGuestFiles` field, we remove the
  // pointer from the set. What is left is the stuff we should be freeing.
  //

  std::unordered_set<GuestFile_t *> ToFree;
  for (const auto &[_, GuestFile] : TrackedGuestFiles_) {
    ToFree.emplace(GuestFile);
  }

  TrackedGuestFiles_.clear();
  for (const auto &[Filename, GuestFile] : SavedTrackedGuestFiles_) {
    TrackedGuestFiles_.emplace(Filename, GuestFile);
    GuestFile->Restore();
    ToFree.erase(GuestFile);
  }

  //
  // Free the people!
  //

  for (const auto *Ptr : ToFree) {
    delete Ptr;
  }

  ToFree.clear();

  GuestFiles_.clear();
  GuestFiles_.insert(SavedGuestFiles_.cbegin(), SavedGuestFiles_.cend());
}

bool FsHandleTable_t::Exists(const std::u16string &Filename) {

  //
  // We walk the file we are tracking and try to find one that matches.
  // If it does, then we check if it exists or not.
  //

  for (const auto &[_, GuestFile] : TrackedGuestFiles_) {
    if (GuestFile->Filename == Filename) {
      return GuestFile->Exists;
    }
  }

  return false;
}

bool FsHandleTable_t::AddHandle(const HANDLE GuestHandle,
                                const GuestFile_t *GuestFile) {
  const auto Res =
      GuestFiles_.emplace(GuestHandle, const_cast<GuestFile_t *>(GuestFile));
  const bool Inserted = Res.second;

  if (!Inserted) {
    fmt::print("Handle already existed?\n");
    __debugbreak();
  }

  g_HandleTable.AddHandle(GuestHandle, nullptr);
  return Inserted;
}

void FsHandleTable_t::MapGuestFileStream(const char16_t *GuestPath,
                                         const uint8_t *Buffer,
                                         const size_t BufferSize,
                                         const bool AlreadyExisted,
                                         const bool AllowWrites) {
  FsDebugPrint(
      "Mapping {} guest file {} with filestream({}) {}\n",
      (AlreadyExisted ? "already existing" : "previously non existing"),
      u16stringToString(GuestPath), BufferSize,
      (AllowWrites ? "with writes allowed" : ""));

  auto GuestFile = new GuestFile_t(GuestPath, Buffer, BufferSize,
                                   AlreadyExisted, AllowWrites);

  TrackedGuestFiles_.emplace(GuestPath, GuestFile);
}

void FsHandleTable_t::MapExistingGuestFile(const char16_t *GuestPath,
                                           const uint8_t *Buffer,
                                           const size_t BufferSize) {
  return MapGuestFileStream(GuestPath, Buffer, BufferSize, true, false);
}

void FsHandleTable_t::MapExistingWriteableGuestFile(const char16_t *GuestPath) {
  return MapGuestFileStream(GuestPath, nullptr, 0, true, true);
}

void FsHandleTable_t::MapNonExistingGuestFile(const char16_t *GuestPath,
                                              const uint8_t *Buffer,
                                              const size_t BufferSize) {
  return MapGuestFileStream(GuestPath, Buffer, BufferSize, false, false);
}

bool FsHandleTable_t::Known(const HANDLE GuestHandle) {
  return GuestFiles_.contains(GuestHandle);
}

bool FsHandleTable_t::Known(const std::u16string &Filename) {
  for (const auto &[CurrentFilename, _] : TrackedGuestFiles_) {
    if (CurrentFilename == Filename) {
      return true;
    }
  }

  return false;
}

bool FsHandleTable_t::CloseGuestHandle(const HANDLE GuestHandle) {
  FsDebugPrint("Closing {}\n", GuestHandle);
  GuestFile_t *GuestFile = GetGuestFile(GuestHandle);
  GuestFiles_.erase(GuestHandle);
  if (GuestFile->DeleteOnClose) {
    FsDebugPrint("Delete on close, so the file does not exist anymore.\n");
    GuestFile->Exists = false;
    GuestFile->DeleteOnClose = false;
  }

  return true;
}

void FsHandleTable_t::SetBlacklistDecisionHandler(
    DecisionHandler_t DecisionHandler) {
  BlacklistDecisionHandler = DecisionHandler;
}

GuestFile_t *FsHandleTable_t::GetGuestFile(const std::u16string &Filename) {
  if (TrackedGuestFiles_.contains(Filename)) {
    return TrackedGuestFiles_.at(Filename);
  }
  return nullptr;
}

GuestFile_t *FsHandleTable_t::GetGuestFile(const HANDLE GuestHandle) {
  return GuestFiles_.at(GuestHandle);
}

FsHandleTable_t g_FsHandleTable;
