// Axel '0vercl0k' Souchet - April 20 2020
#include "globals.h"
#include "nt.h"
#include "utils.h"
#include <algorithm>
#include <cinttypes>
#include <cstdint>
#include <cstring>

#ifdef FILESTREAM_LOGGING_ON
#define FileStreamDebugPrint(Format, ...)                                      \
  fmt::print("filestream: " Format, ##__VA_ARGS__)
#else
#define FileStreamDebugPrint(Format, ...) /* nuthin */
#endif

constexpr uint64_t AlignUp(const uint64_t Size, const uint64_t Alignment) {
  const uint64_t Remainder = Size % Alignment;
  return Remainder ? Size + (Alignment - Remainder) : Size;
}

class GuestFile_t {
  //
  // Those basically tracks the buffer bounds. Those never change during the
  // lifetime.
  //

  std::unique_ptr<uint8_t[]> BufferStart_;
  const uint8_t *BufferEnd_;
  size_t BufferSize_;

  //
  // This is the current cursor.
  //

  const uint8_t *SavedCurrent_;
  uint8_t *Current_;

  //
  // This tracks what the guest thinks the file size is (it can be smaller than
  // the backing buffer).
  //

  size_t SavedGuestSize_;
  size_t GuestSize_;

  const bool AllowWrites_ : 1;
  bool SavedDeleteOnClose_ : 1;
  bool SavedExists_ : 1;

public:
  const std::u16string Filename;

  bool DeleteOnClose : 1;
  bool Exists : 1;

  GuestFile_t(const char16_t *Filename, const uint8_t *Buffer,
              const size_t BufferSize, const bool Exists_,
              const bool AllowWrites)
      : BufferStart_(nullptr), BufferEnd_(nullptr), BufferSize_(BufferSize),
        SavedCurrent_(nullptr), Current_(nullptr), SavedGuestSize_(0),
        GuestSize_(BufferSize), AllowWrites_(AllowWrites),
        SavedDeleteOnClose_(false), SavedExists_(false), Filename(Filename),
        DeleteOnClose(false), Exists(Exists_) {
    if (AllowWrites_) {

      //
      // Initialize a backing buffer of 1MB.
      //

      BufferSize_ = _1MB;
    }

    BufferStart_ = std::make_unique<uint8_t[]>(BufferSize_);
    BufferEnd_ = BufferStart_.get() + BufferSize_;

    Current_ = BufferStart_.get();
    GuestSize_ = BufferSize_;

    if (Buffer) {
      memcpy(BufferStart_.get(), Buffer, BufferSize_);
    }
  }

  void Save() {
    SavedCurrent_ = Current_;
    SavedGuestSize_ = GuestSize_;
    SavedExists_ = Exists;
    SavedDeleteOnClose_ = DeleteOnClose;
  }

  void Restore() {
    Current_ = const_cast<uint8_t *>(SavedCurrent_);
    GuestSize_ = SavedGuestSize_;
    Exists = SavedExists_;
    DeleteOnClose = SavedDeleteOnClose_;

    if (AllowWrites_) {
      memset(BufferStart_.get(), 0, BufferSize_);
    }
  }

  void SetGuestSize(const size_t GuestSize) { GuestSize_ = GuestSize; }

  void ResetCursor() { Current_ = BufferStart_.get(); }

  bool NtReadFile(NTSTATUS &NtStatus, IO_STATUS_BLOCK *HostIoStatusBlock,
                  uint8_t *Buffer, const uint32_t Length) {
    if (BufferStart_ == nullptr) {
      FileStreamDebugPrint("Cannot read on file with empty stream.\n");
      return false;
    }

    const uint64_t MaxRead = BufferEnd_ - Current_;
    uint32_t Size2Read = std::min(uint32_t(MaxRead), Length);
    if (Current_ > BufferEnd_) {
      Size2Read = 0;
    } else {
      memcpy(Buffer, Current_, Size2Read);
      FileStreamDebugPrint("Reading {:#x} ({:#x} asked)\n", Size2Read, Length);

#ifdef FILESTREAM_SNOOP_READS
      Hexdump(0, Current_, Size2Read);
#endif

      Current_ += Size2Read;
    }

    //
    // Populate the IOB.
    //

    NtStatus = STATUS_SUCCESS;
    HostIoStatusBlock->Status = NtStatus;
    HostIoStatusBlock->Information = Size2Read;
    return true;
  }

  bool NtWriteFile(NTSTATUS &NtStatus, IO_STATUS_BLOCK *HostIoStatusBlock,
                   const uint8_t *Buffer, const uint32_t Length) {

#ifdef FILESTREAM_SNOOP_WRITES
    Hexdump(0, Buffer, Length);
#endif

    if (AllowWrites_) {
      const uint8_t *PredictedEndBuffer = Current_ + Length;
      if (PredictedEndBuffer > BufferEnd_) {
        FileStreamDebugPrint(
            "The buffer backing the write stream is too small, so walling "
            "it off\n");
      } else {
        FileStreamDebugPrint("Writing {:#x} bytes in file..\n", Length);
        memcpy(Current_, Buffer, Length);
        Current_ += Length;
      }

      const uint8_t *GuestEndBuffer = BufferStart_.get() + GuestSize_;
      if (PredictedEndBuffer > GuestEndBuffer) {
        const uint64_t NewBytes = PredictedEndBuffer - GuestEndBuffer;
        const size_t NewGuestSize = GuestSize_ + NewBytes;
        FileStreamDebugPrint("Extending guest size from {:#x} to {:#x}..\n",
                             GuestSize_, NewGuestSize);
        GuestSize_ = NewGuestSize;
      }
    } else {
      FileStreamDebugPrint("Walling off this write.\n");
    }

    //
    // Populate the IOB.
    //

    NtStatus = STATUS_SUCCESS;
    HostIoStatusBlock->Status = NtStatus;
    HostIoStatusBlock->Information = Length;
    return true;
  }

  bool NtQueryVolumeInformationFile(
      NTSTATUS &NtStatus, IO_STATUS_BLOCK *HostIoStatusBlock,
      const uint8_t *HostFsInformation, const uint32_t Length,
      const FS_INFORMATION_CLASS FsInformationClass) const {
    NtStatus = STATUS_INVALID_PARAMETER;
    const bool IsFsDeviceInformation =
        FsInformationClass == FS_INFORMATION_CLASS::FileFsDeviceInformation &&
        Length == 8;
    if (IsFsDeviceInformation) {
      FileStreamDebugPrint("FileFsDeviceInformation.\n");
      auto *HostDeviceInformation =
          (FILE_FS_DEVICE_INFORMATION *)HostFsInformation;
      HostDeviceInformation->DeviceType = FILE_DEVICE_DISK;
      HostDeviceInformation->MaximumComponentNameLength = 0x00020020;
    } else {
      FileStreamDebugPrint("Unknown FsInformationClass.\n");
      return false;
    }

    //
    // Populate the IOB.
    //

    NtStatus = STATUS_SUCCESS;
    HostIoStatusBlock->Status = NtStatus;
    HostIoStatusBlock->Information = Length;
    return true;
  }

  bool NtQueryInformationFile(
      NTSTATUS &NtStatus, IO_STATUS_BLOCK *HostIoStatusBlock,
      const uint8_t *HostFileInformation, const uint32_t Length,
      const FILE_INFORMATION_CLASS FileInformationClass) const {
    NtStatus = STATUS_INVALID_PARAMETER;
    const bool IsFileAttributeTagInfo =
        FileInformationClass ==
            FILE_INFORMATION_CLASS::FileAttributeTagInformation &&
        Length == sizeof(FILE_ATTRIBUTE_TAG_INFORMATION);

    const bool IsFilePositionInfo =
        FileInformationClass ==
            FILE_INFORMATION_CLASS::FilePositionInformation &&
        Length == sizeof(FILE_POSITION_INFORMATION);

    const bool IsFileStandardInfo =
        FileInformationClass ==
            FILE_INFORMATION_CLASS::FileStandardInformation &&
        Length == sizeof(FILE_STANDARD_INFORMATION);

    if (IsFileAttributeTagInfo) {
      FileStreamDebugPrint("FileAttributeTagInformation.\n");
      const auto FileAttributeTagInfo =
          (FILE_ATTRIBUTE_TAG_INFORMATION *)HostFileInformation;

      FileAttributeTagInfo->FileAttributes = 0;
      FileAttributeTagInfo->ReparseTag = 0;
    } else if (IsFilePositionInfo) {
      const auto FilePositionInfo =
          (FILE_POSITION_INFORMATION *)HostFileInformation;

      const uint64_t Offset = Current_ - BufferStart_.get();
      FilePositionInfo->CurrentByteOffset = Offset;
      FileStreamDebugPrint("FilePositionInformation({:#x}).\n", Offset);
    } else if (IsFileStandardInfo) {
      const auto FileStandardInfo =
          (FILE_STANDARD_INFORMATION *)HostFileInformation;
      FileStandardInfo->AllocationSize = AlignUp(GuestSize_, 0x1000);
      FileStandardInfo->EndOfFile = GuestSize_;
      FileStandardInfo->NumberOfLinks = 1;
      FileStandardInfo->DeletePending = DeleteOnClose;
      FileStandardInfo->Directory = 0;

      FileStreamDebugPrint(
          "FileStandardInformation(AllocationSize={:#x}, EndOfFile={:#x}).\n",
          FileStandardInfo->AllocationSize, FileStandardInfo->EndOfFile);
    } else {
      FileStreamDebugPrint("Unsupported class.\n");
      return false;
    }

    //
    // Populate the IOB.
    //

    NtStatus = STATUS_SUCCESS;
    HostIoStatusBlock->Status = NtStatus;
    HostIoStatusBlock->Information = Length;
    return true;
  }

  bool NtSetInformationFile(NTSTATUS &NtStatus,
                            IO_STATUS_BLOCK *HostIoStatusBlock,
                            const uint8_t *HostFileInformation,
                            const uint32_t Length,
                            const FILE_INFORMATION_CLASS FileInformationClass) {
    const bool IsFilePositionInfo =
        FileInformationClass ==
            FILE_INFORMATION_CLASS::FilePositionInformation &&
        Length == sizeof(FILE_POSITION_INFORMATION);

    const bool IsFileDispositionInfo =
        FileInformationClass ==
            FILE_INFORMATION_CLASS::FileDispositionInformation &&
        Length == sizeof(FILE_DISPOSITION_INFORMATION);

    const bool IsFileEndOfFileInfo =
        FileInformationClass ==
            FILE_INFORMATION_CLASS::FileEndOfFileInformation &&
        Length == sizeof(FILE_END_OF_FILE_INFORMATION);

    const bool IsFileAllocationInfo =
        FileInformationClass ==
            FILE_INFORMATION_CLASS::FileAllocationInformation &&
        Length == sizeof(FILE_ALLOCATION_INFORMATION);

    if (IsFilePositionInfo) {
      FileStreamDebugPrint("FilePositionInformation.\n");
      const auto FilePositionInfo =
          (FILE_POSITION_INFORMATION *)HostFileInformation;

      //
      // Only move the cursor if we have a buffer attached.
      //

      if (BufferStart_) {
        const uint64_t Offset = FilePositionInfo->CurrentByteOffset;
        FileStreamDebugPrint("Moving cursor to offset {:#x}.\n", Offset);
        Current_ = BufferStart_.get() + Offset;
      }
    } else if (IsFileDispositionInfo) {
      const auto FileDispositionInfo =
          (FILE_DISPOSITION_INFORMATION *)HostFileInformation;

      FileStreamDebugPrint("FileDispositionInfo(DeleteOnClose={}).\n",
                           FileDispositionInfo->DeleteFile);
      DeleteOnClose = FileDispositionInfo->DeleteFile;
    } else if (IsFileEndOfFileInfo) {
      const auto FileEndOfFileInfo =
          (FILE_END_OF_FILE_INFORMATION *)HostFileInformation;

      FileStreamDebugPrint("FileEndOfFileInformation({:#x}).\n",
                           FileEndOfFileInfo->EndOfFile);
      GuestSize_ = FileEndOfFileInfo->EndOfFile;
    } else if (IsFileAllocationInfo) {
      const auto FileAllocationInfo_ =
          (FILE_ALLOCATION_INFORMATION *)HostFileInformation;

      FileStreamDebugPrint("FileAllocationInformation({:#x}).\n",
                           FileAllocationInfo->AllocationSize);
    } else {
      FileStreamDebugPrint("Unsupported class.\n");
      return false;
    }

    //
    // Populate the IOB.
    //

    NtStatus = STATUS_SUCCESS;
    HostIoStatusBlock->Status = NtStatus;
    HostIoStatusBlock->Information = Length;
    return true;
  }
};