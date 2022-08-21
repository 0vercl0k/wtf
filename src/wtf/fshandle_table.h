// Axel '0vercl0k' Souchet - April 25 2020
#pragma once
#include "globals.h"
#include "guestfile.h"
#include "platform.h"
#include "restorable.h"
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

using DecisionHandler_t = bool (*)(const std::u16string &Path);

class FsHandleTable_t : public Restorable_t {
  std::unordered_map<std::u16string, GuestFile_t *> TrackedGuestFiles_;
  std::unordered_map<std::u16string, GuestFile_t *> SavedTrackedGuestFiles_;
  std::unordered_map<HANDLE, GuestFile_t *> GuestFiles_;
  std::unordered_map<HANDLE, GuestFile_t *> SavedGuestFiles_;

public:
  //
  // Give the user the opportunity to give a 'yes' / 'no' decision as to
  // is a file a not found file. This allow to support files with variable
  // names. Imagine a case where you want to consider ghost files every files
  // that ends-up with the extension '.ids'.
  //

  DecisionHandler_t BlacklistDecisionHandler;

  ~FsHandleTable_t();

  //
  // ctor.
  //

  FsHandleTable_t();

  //
  // Save the state of the fs handle table. This is invoked by the handle table
  // itself.
  //

  void Save() override;

  //
  // Restore the state of the fs handle table. This is invoked by the handle
  // table itself.
  //

  void Restore() override;

  //
  // Associate a guest handle and a guest file.
  //

  bool AddHandle(const HANDLE GuestHandle, const GuestFile_t *GuestFile);

  //
  // Does this file exists in our world?
  //

  bool Exists(const std::u16string &Filename);

  //
  // Map a guest file. With, without a buffer. That existed before or not. That
  // allows writes or not.
  //

  void MapGuestFileStream(const char16_t *GuestPath, const uint8_t *Buffer,
                          const size_t BufferSize, const bool AlreadyExisted,
                          const bool AllowWrites);

  void MapExistingGuestFile(const char16_t *GuestPath,
                            const uint8_t *Buffer = nullptr,
                            const size_t BufferSize = 0);

  void MapNonExistingGuestFile(const char16_t *GuestPath,
                               const uint8_t *Buffer = nullptr,
                               const size_t BufferSize = 0);

  void MapExistingWriteableGuestFile(const char16_t *GuestPath);

  //
  // Get the guest file from a name or a guest handle.
  //

  GuestFile_t *GetGuestFile(const std::u16string &Filename);
  GuestFile_t *GetGuestFile(const HANDLE GuestHandle);

  //
  // Is this a handle we know about?
  //

  bool Known(const HANDLE GuestHandle);

  //
  // Is this a file  we know about?
  //

  bool Known(const std::u16string &Filename);

  //
  // Close a guest handle.
  //

  bool CloseGuestHandle(const HANDLE GuestHandle);

  //
  // Set a decision handler for picking if a file should appear as not existing.
  //

  void SetBlacklistDecisionHandler(DecisionHandler_t DecisionHandler);

private:
  static bool DefaultBlacklistDecisionHandler(const std::u16string &) {
    return false;
  }
};

extern FsHandleTable_t g_FsHandleTable;