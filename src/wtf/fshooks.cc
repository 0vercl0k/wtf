// Axel '0vercl0k' Souchet - March 19 2020
#include "fshooks.h"
#include "backend.h"
#include "fshandle_table.h"
#include "globals.h"
#include "handle_table.h"
#include "nt.h"
#include "utils.h"

std::string
BitfieldToStr(const uint32_t Value,
              const std::unordered_map<uint32_t, const char *> &Map) {
  std::string S;
  for (const auto &[Key, Val] : Map) {
    if ((Value & Key) != Key) {
      continue;
    }

    if (S != "") {
      S += " | ";
    }
    S += Val;
  }

  return S;
}

const char *ValueToStr(const uint32_t Value,
                       const std::unordered_map<uint32_t, const char *> &Map) {
  for (const auto &[Key, Val] : Map) {
    if (Value == Key) {
      return Val;
    }
  }

  return nullptr;
}

std::string OpenOptionsToStr(const uint32_t OpenOptions) {
#define ENTRY(Entry)                                                           \
  { Entry, #Entry }
  const std::unordered_map<uint32_t, const char *> Options = {
      ENTRY(FILE_DIRECTORY_FILE),
      ENTRY(FILE_WRITE_THROUGH),
      ENTRY(FILE_SEQUENTIAL_ONLY),
      ENTRY(FILE_NO_INTERMEDIATE_BUFFERING),
      ENTRY(FILE_SYNCHRONOUS_IO_ALERT),
      ENTRY(FILE_SYNCHRONOUS_IO_NONALERT),
      ENTRY(FILE_NON_DIRECTORY_FILE),
      ENTRY(FILE_CREATE_TREE_CONNECTION),
      ENTRY(FILE_COMPLETE_IF_OPLOCKED),
      ENTRY(FILE_NO_EA_KNOWLEDGE),
      ENTRY(FILE_OPEN_FOR_RECOVERY),
      ENTRY(FILE_RANDOM_ACCESS),
      ENTRY(FILE_DELETE_ON_CLOSE),
      ENTRY(FILE_OPEN_BY_FILE_ID),
      ENTRY(FILE_OPEN_FOR_BACKUP_INTENT),
      ENTRY(FILE_NO_COMPRESSION),
      ENTRY(FILE_OPEN_REQUIRING_OPLOCK),
      ENTRY(FILE_DISALLOW_EXCLUSIVE),
      ENTRY(FILE_SESSION_AWARE),
      ENTRY(FILE_RESERVE_OPFILTER),
      ENTRY(FILE_OPEN_REPARSE_POINT),
      ENTRY(FILE_OPEN_NO_RECALL),
      ENTRY(FILE_OPEN_FOR_FREE_SPACE_QUERY)};
#undef ENTRY
  return BitfieldToStr(OpenOptions, Options);
}

std::string ShareAccessToStr(const uint32_t ShareAccess) {
#define ENTRY(Entry)                                                           \
  { Entry, #Entry }
  const std::unordered_map<uint32_t, const char *> Shares = {
      ENTRY(FILE_SHARE_READ), ENTRY(FILE_SHARE_WRITE),
      ENTRY(FILE_SHARE_DELETE)};
#undef ENTRY
  return BitfieldToStr(ShareAccess, Shares);
}

std::string CreateDispositionToStr(const uint32_t CreateDisposition) {
#define ENTRY(Entry)                                                           \
  { Entry, #Entry }
  const std::unordered_map<uint32_t, const char *> Dispositions = {
      ENTRY(FILE_SUPERSEDE), ENTRY(FILE_OPEN),      ENTRY(FILE_CREATE),
      ENTRY(FILE_OPEN_IF),   ENTRY(FILE_OVERWRITE), ENTRY(FILE_OVERWRITE_IF),
  };
#undef ENTRY
  return ValueToStr(CreateDisposition, Dispositions);
}

const uint32_t CreateDispositionToIob(const uint32_t CreateDisposition) {
  switch (CreateDisposition) {
  case FILE_SUPERSEDE: {
    return FILE_SUPERSEDE;
  }
  case FILE_OPEN: {
    return FILE_OPENED;
  }
  case FILE_CREATE: {
    return FILE_CREATED;
  }
  case FILE_OVERWRITE:
  case FILE_OVERWRITE_IF: {
    return FILE_OVERWRITTEN;
  }
  default: {
    __debugbreak();
    return 0;
  }
  }
}

bool SetupFilesystemHooks() {

  if (!g_Backend->SetBreakpoint("ntdll!NtClose", [](Backend_t *Backend) {
        //__kernel_entry NTSTATUS NtClose(
        //  IN HANDLE Handle
        //);
        const HANDLE Handle = HANDLE(Backend->GetArg(0));

        FsDebugPrint("ntdll!NtClose(Handle={})\n", fmt::ptr(Handle));

        //
        // We ask the handle table here because NtClose is not only used
        // to close file handles.
        //

        if (!g_HandleTable.Has(Handle)) {
          FsDebugPrint("Unrecognized file handle.\n");
          return;
        }

        const bool Closed = g_HandleTable.CloseGuestHandle(Handle);
        if (!Closed) {
          __debugbreak();
        }

        const NTSTATUS NtStatus =
            Closed ? STATUS_SUCCESS : STATUS_INVALID_PARAMETER;
        Backend->SimulateReturnFromFunction(NtStatus);
      })) {
    return false;
  }

  if (!g_Backend->SetBreakpoint(
          "ntdll!NtQueryAttributesFile", [](Backend_t *Backend) {
            // NTSTATUS NtQueryAttributesFile(
            //  _In_  POBJECT_ATTRIBUTES      ObjectAttributes,
            //  _Out_ PFILE_BASIC_INFORMATION FileInformation
            //);
            const Gva_t GuestObjectAttributes = Backend->GetArgGva(0);
            const Gva_t GuestFileInformation = Backend->GetArgGva(1);

            HostObjectAttributes_t HostObjectAttributes;
            if (!HostObjectAttributes.ReadFromGuest(Backend,
                                                    GuestObjectAttributes)) {
              fmt::print("ReadFromGuest failed.\n");
              ExitProcess(0);
            }

            const std::u16string Filename(HostObjectAttributes.ObjectName());
            FsDebugPrint("ntdll!NtQueryAttributesFile(ObjectAttributes={:#x} "
                         "({}), FileInformation={:#x})\n",
                         GuestObjectAttributes,
                         u16stringToString(Filename).c_str(),
                         GuestFileInformation);

            const bool IsBlacklisted =
                g_FsHandleTable.BlacklistDecisionHandler(Filename);
            if (!g_FsHandleTable.Known(Filename) && !IsBlacklisted) {
              FsDebugPrint("Unknown file.\n");
              return;
            }

            //
            // Is it a file that doesn't exist?
            //

            if (!g_FsHandleTable.Exists(Filename) || IsBlacklisted) {

              //
              // In that case it is easy, we just return that the object name
              // hasn't been found.
              //

              FsDebugPrint("Faking that this file does not exist.\n");
              Backend->SimulateReturnFromFunction(STATUS_OBJECT_NAME_NOT_FOUND);
              return;
            }

            //
            // Ensure that the GuestFileInformation is faulted-in memory.
            //

            if (GuestFileInformation &&
                Backend->PageFaultsMemoryIfNeeded(
                    GuestFileInformation, sizeof(FILE_BASIC_INFORMATION))) {
              return;
            }

            FsDebugPrint("Faking that this file is normal.\n");

            //
            // We do not proxy any I/O to the host kernel here because what
            // have been observed is that this function gets only called to
            // know the attributes of the file. So we simply emulate this
            // case.
            //

            FILE_BASIC_INFORMATION HostFileBasicInformation;
            Backend->VirtReadStruct(GuestFileInformation,
                                    &HostFileBasicInformation);

            HostFileBasicInformation.ChangeTime = 0;
            HostFileBasicInformation.CreationTime = 0;
            HostFileBasicInformation.LastAccessTime = 0;
            HostFileBasicInformation.LastWriteTime = 0;
            HostFileBasicInformation.FileAttributes = FILE_ATTRIBUTE_NORMAL;

            Backend->VirtWriteStructDirty(GuestFileInformation,
                                          &HostFileBasicInformation);

            Backend->SimulateReturnFromFunction(STATUS_SUCCESS);
          })) {
    return false;
  }

  if (!g_Backend->SetBreakpoint("ntdll!NtCreateFile", [](Backend_t *Backend) {
        //  __kernel_entry NTSTATUS NtCreateFile(
        //  OUT PHANDLE           FileHandle,
        //  IN ACCESS_MASK        DesiredAccess,
        //  IN POBJECT_ATTRIBUTES ObjectAttributes,
        //  OUT PIO_STATUS_BLOCK  IoStatusBlock,
        //  IN PLARGE_INTEGER     AllocationSize,
        //  IN ULONG              FileAttributes,
        //  IN ULONG              ShareAccess,
        //  IN ULONG              CreateDisposition,
        //  IN ULONG              CreateOptions,
        //  IN PVOID              EaBuffer,
        //  IN ULONG              EaLength
        //);
        const Gva_t GuestFileHandle = Backend->GetArgGva(0);
        const uint32_t DesiredAccess = uint32_t(Backend->GetArg(1));
        const Gva_t GuestObjectAttributes = Backend->GetArgGva(2);
        const Gva_t GuestIoStatusBlock = Backend->GetArgGva(3);
        const uint64_t GuestAllocationSize = Backend->GetArg(4);
        const uint32_t FileAttributes = uint32_t(Backend->GetArg(5));
        const uint32_t ShareAccess = uint32_t(Backend->GetArg(6));
        const uint32_t CreateDisposition = uint32_t(Backend->GetArg(7));
        const uint32_t CreateOptions = uint32_t(Backend->GetArg(8));
        const uint64_t EaBuffer = Backend->GetArg(9);
        const uint32_t EaLength = uint32_t(Backend->GetArg(10));

        HostObjectAttributes_t HostObjectAttributes;
        if (!HostObjectAttributes.ReadFromGuest(Backend,
                                                GuestObjectAttributes)) {
          fmt::print("ReadFromGuest failed.\n");
          ExitProcess(0);
        }

        const std::u16string Filename(HostObjectAttributes.ObjectName());
        FsDebugPrint(
            "ntdll!NtCreateFile(FileHandle={:#x}, DesiredAccess={:#x}, "
            "ObjectAttributes={:#x} ({}), IoStatusBlock={:#x}, "
            "AllocationSize={:#x}, FileAttributes={:#x}, ShareAccess={:#x} "
            "({}), CreateDisposition={:#x} ({}), CreateOptions={:#x} ({}), "
            "EaBuffer={:#x}, EaLength={:#x})\n",
            GuestFileHandle, DesiredAccess, GuestObjectAttributes,
            u16stringToString(Filename).c_str(), GuestIoStatusBlock,
            GuestAllocationSize, FileAttributes, ShareAccess,
            ShareAccessToStr(ShareAccess).c_str(), CreateDisposition,
            CreateDispositionToStr(CreateDisposition).c_str(), CreateOptions,
            OpenOptionsToStr(CreateOptions).c_str(), EaBuffer, EaLength);

        const bool IsBlacklisted =
            g_FsHandleTable.BlacklistDecisionHandler(Filename);

        if (!g_FsHandleTable.Known(Filename) && !IsBlacklisted) {
          return;
        }

        IO_STATUS_BLOCK HostIoStatusBlock;
        Backend->VirtReadStruct(GuestIoStatusBlock, &HostIoStatusBlock);

        //
        // We take care of two cases here:
        //   - If we get FILE_CREATE and that the file exists, we need to
        //   fail the request;
        //   - If we get a FILE_OPEN and that the file does not exist, we
        //   need to fail the request.
        //

        const bool Exists = g_FsHandleTable.Exists(Filename);
        const bool FailRequest = (Exists && CreateDisposition == FILE_CREATE) ||
                                 (!Exists && CreateDisposition == FILE_OPEN) ||
                                 IsBlacklisted;

        if (FailRequest) {
          FsDebugPrint("{} {} and CreateOptions={} so failing\n",
                       u16stringToString(Filename).c_str(),
                       (Exists ? "exists" : "does not exist"),
                       OpenOptionsToStr(CreateDisposition).c_str());

          //
          // We populate the IOB with a name not found and return.
          //

          const NTSTATUS NtStatus = STATUS_OBJECT_NAME_NOT_FOUND;
          HostIoStatusBlock.Status = NtStatus;
          HostIoStatusBlock.Information = 0;
          Backend->VirtWriteStructDirty(GuestIoStatusBlock, &HostIoStatusBlock);

          Backend->SimulateReturnFromFunction(NtStatus);
          return;
        }

        //
        // AllocationSize is optional so read it only if a pointer is
        // specified.
        //

        if (GuestAllocationSize) {
          fmt::print("GuestAllocationSize??\n");
          __debugbreak();
        }

        if (DesiredAccess & FILE_APPEND_DATA) {
          fmt::print("FILE_APPEND_DATA hasn't been implemented.\n");
          __debugbreak();
        }

        const HANDLE GuestHandle = g_HandleTable.AllocateGuestHandle();
        Backend->VirtWriteStructDirty(GuestFileHandle, &GuestHandle);
        FsDebugPrint("Opening {} for {}\n", fmt::ptr(GuestHandle),
                     u16stringToString(Filename).c_str());

        //
        // Don't forget to let the handle table know about the handle.
        //

        GuestFile_t *GuestFile = g_FsHandleTable.GetGuestFile(Filename);
        GuestFile->ResetCursor();
        GuestFile->Exists = true;

        if (CreateDisposition == FILE_OVERWRITE ||
            CreateDisposition == FILE_OVERWRITE_IF) {
          FsDebugPrint("FILE_OVERWRITE(IF) so setting guest file size to 0.\n");
          GuestFile->SetGuestSize(0);
        }

        g_FsHandleTable.AddHandle(GuestHandle, GuestFile);

        const NTSTATUS NtStatus = STATUS_SUCCESS;
        HostIoStatusBlock.Status = NtStatus;
        HostIoStatusBlock.Information =
            CreateDispositionToIob(CreateDisposition);

        //
        // Write the output parameters back to the guest; the handle and
        // the IOB.
        //

        Backend->VirtWriteStructDirty(GuestFileHandle, &GuestHandle);
        Backend->VirtWriteStructDirty(GuestIoStatusBlock, &HostIoStatusBlock);

        Backend->SimulateReturnFromFunction(NtStatus);
      })) {
    return false;
  }

  if (!g_Backend->SetBreakpoint("ntdll!NtOpenFile", [](Backend_t *Backend) {
        //__kernel_entry NTSTATUS NtOpenFile(
        //  OUT PHANDLE           FileHandle,
        //  IN ACCESS_MASK        DesiredAccess,
        //  IN POBJECT_ATTRIBUTES ObjectAttributes,
        //  OUT PIO_STATUS_BLOCK  IoStatusBlock,
        //  IN ULONG              ShareAccess,
        //  IN ULONG              OpenOptions
        //);
        const Gva_t GuestFileHandle = Backend->GetArgGva(0);
        const uint32_t DesiredAccess = uint32_t(Backend->GetArg(1));
        const Gva_t GuestObjectAttributes = Backend->GetArgGva(2);
        const Gva_t GuestIoStatusBlock = Backend->GetArgGva(3);
        const uint32_t ShareAccess = uint32_t(Backend->GetArg(4));
        const uint32_t OpenOptions = uint32_t(Backend->GetArg(5));

        HostObjectAttributes_t HostObjectAttributes;
        if (!HostObjectAttributes.ReadFromGuest(Backend,
                                                GuestObjectAttributes)) {
          fmt::print("ReadFromGuest failed.\n");
          ExitProcess(0);
        }

        const std::u16string Filename(HostObjectAttributes.ObjectName());
        FsDebugPrint("ntdll!NtOpenFile(FileHandle={:#x}, DesiredAccess={:#x}, "
                     "ObjectAttributes={:#x} ({}), IoStatusBlock={:#x}, "
                     "ShareAccess={:#x} ({}), OpenOptions={:#x} ({}))\n",
                     GuestFileHandle, DesiredAccess, GuestObjectAttributes,
                     u16stringToString(Filename).c_str(), GuestIoStatusBlock,
                     ShareAccess, ShareAccessToStr(ShareAccess).c_str(),
                     OpenOptions, OpenOptionsToStr(OpenOptions).c_str());

        const bool IsBlacklisted =
            g_FsHandleTable.BlacklistDecisionHandler(Filename);
        if (!g_FsHandleTable.Known(Filename) && !IsBlacklisted) {
          return;
        }

        IO_STATUS_BLOCK HostIoStatusBlock;
        Backend->VirtReadStruct(GuestIoStatusBlock, &HostIoStatusBlock);

        NTSTATUS NtStatus;
        GuestFile_t *GuestFile = g_FsHandleTable.GetGuestFile(Filename);
        if (IsBlacklisted || !GuestFile->Exists) {
          FsDebugPrint("{} does not exists\n",
                       u16stringToString(Filename).c_str());
          NtStatus = STATUS_OBJECT_NAME_NOT_FOUND;
          HostIoStatusBlock.Status = NtStatus;
          HostIoStatusBlock.Information = 0;
        } else {
          GuestFile->ResetCursor();

          //
          // This is another pretty easy case. We just need to return a
          // handle to the guest and keep track of it. That's kinda it.
          //

          const HANDLE GuestHandle = g_HandleTable.AllocateGuestHandle();
          Backend->VirtWriteStructDirty(GuestFileHandle, &GuestHandle);
          FsDebugPrint("{} exists so opening a handle: {}\n",
                       u16stringToString(Filename).c_str(),
                       fmt::ptr(GuestHandle));

          //
          // Don't forget to let the handle table know about the handle.
          //

          g_FsHandleTable.AddHandle(GuestHandle, GuestFile);

          NtStatus = STATUS_SUCCESS;
          HostIoStatusBlock.Status = NtStatus;
          HostIoStatusBlock.Information = FILE_OPENED;
        }

        Backend->VirtWriteStructDirty(GuestIoStatusBlock, &HostIoStatusBlock);

        Backend->SimulateReturnFromFunction(NtStatus);
      })) {
    return false;
  }

  if (!g_Backend->SetBreakpoint(
          "ntdll!NtQueryVolumeInformationFile", [](Backend_t *Backend) {
            //__kernel_entry NTSYSCALLAPI NTSTATUS
            // NtQueryVolumeInformationFile(
            //  HANDLE               FileHandle,
            //  PIO_STATUS_BLOCK     IoStatusBlock,
            //  PVOID                FsInformation,
            //  ULONG                Length,
            //  FS_INFORMATION_CLASS FsInformationClass
            //);
            const HANDLE FileHandle = HANDLE(Backend->GetArg(0));
            const Gva_t GuestIoStatusBlock = Backend->GetArgGva(1);
            const Gva_t GuestFsInformation = Backend->GetArgGva(2);
            const uint32_t Length = uint32_t(Backend->GetArg(3));
            const FS_INFORMATION_CLASS FsInformationClass =
                FS_INFORMATION_CLASS(Backend->GetArg(4));

            FsDebugPrint("ntdll!NtQueryVolumeInformationFile(FileHandle={}, "
                         "IoStatusBlock={:#x}, "
                         "FsInformation={:#x}, Length={:#x}, "
                         "FsInformationClass={:#x})\n",
                         fmt::ptr(FileHandle), GuestIoStatusBlock,
                         GuestFsInformation, Length, FsInformationClass);

            //
            // If we don't know anything about this handle, let the syscall
            // handle itself.
            //

            if (!g_FsHandleTable.Known(FileHandle)) {
              FsDebugPrint("Unrecognized {} handle.\n", fmt::ptr(FileHandle));
              return;
            }

            //
            // Ensure that the GuestFsInformation is faulted-in memory.
            //

            // if (GuestFsInformation &&
            //    PageFaultsMemoryIfNeeded(Cpu, GuestFsInformation, Length)) {
            //  return;
            //}

            //
            // Read the IOB.
            //

            IO_STATUS_BLOCK HostIoStatusBlock;
            Backend->VirtReadStruct(GuestIoStatusBlock, &HostIoStatusBlock);

            //
            // Get the host handle.
            //

            GuestFile_t *GuestFile = g_FsHandleTable.GetGuestFile(FileHandle);

            //
            // If we have a host handle, we proxy it to our kernel.
            //

            auto HostFsInformation = std::make_unique<uint8_t[]>(Length);

            //
            // Invoke the real syscall.
            //

            NTSTATUS NtStatus;
            const bool SyscallSuccess = GuestFile->NtQueryVolumeInformationFile(
                NtStatus, &HostIoStatusBlock, HostFsInformation.get(), Length,
                FsInformationClass);

            //
            // We want to know if it failed as usual.
            //

            if (SyscallSuccess && !NT_SUCCESS(NtStatus)) {
              __debugbreak();
            }

            //
            // Write back the output parameters, the FsInformation as well as
            // the IOB.
            //

            Backend->VirtWriteDirty(GuestFsInformation, HostFsInformation.get(),
                                    Length);
            Backend->VirtWriteStructDirty(GuestIoStatusBlock,
                                          &HostIoStatusBlock);

            if (!SyscallSuccess) {
              return;
            }

            Backend->SimulateReturnFromFunction(NtStatus);
          })) {
    return false;
  }

  if (!g_Backend->SetBreakpoint(
          "ntdll!NtQueryInformationFile", [](Backend_t *Backend) {
            //__kernel_entry NTSYSCALLAPI NTSTATUS NtQueryInformationFile(
            //  HANDLE                 FileHandle,
            //  PIO_STATUS_BLOCK       IoStatusBlock,
            //  PVOID                  FileInformation,
            //  ULONG                  Length,
            //  FILE_INFORMATION_CLASS FileInformationClass
            //);
            const HANDLE FileHandle = HANDLE(Backend->GetArg(0));
            const Gva_t GuestIoStatusBlock = Backend->GetArgGva(1);
            const Gva_t GuestFileInformation = Backend->GetArgGva(2);
            const uint32_t Length = uint32_t(Backend->GetArg(3));
            const FILE_INFORMATION_CLASS FileInformationClass =
                FILE_INFORMATION_CLASS(Backend->GetArg(4));

            FsDebugPrint("ntdll!NtQueryInformationFile(FileHandle={}, "
                         "IoStatusBlock={:#x}, "
                         "FileInformation={:#x}, Length={:#x}, "
                         "FileInformationClass={:#x})\n",
                         fmt::ptr(FileHandle), GuestIoStatusBlock,
                         GuestFileInformation, Length, FileInformationClass);

            //
            // If we don't know about the handle, let the guest figure it out.
            //

            if (!g_FsHandleTable.Known(FileHandle)) {
              FsDebugPrint("Unrecognized file handle.\n");
              return;
            }

            //
            // Grab the stream.
            //

            GuestFile_t *GuestFile = g_FsHandleTable.GetGuestFile(FileHandle);

            //
            // Ensure that the GuestFileInformation is faulted-in memory.
            //

            // if (GuestFileInformation &&
            //    PageFaultsMemoryIfNeeded(Cpu, GuestFileInformation, Length))
            //    {
            //  return;
            //}

            //
            // Read the IOB.
            //

            IO_STATUS_BLOCK HostIoStatusBlock;
            Backend->VirtReadStruct(GuestIoStatusBlock, &HostIoStatusBlock);

            //
            // Allocate memory for the FileInformation.
            //

            auto HostFileInformation = std::make_unique<uint8_t[]>(Length);

            NTSTATUS NtStatus;
            const bool SyscallSuccess = GuestFile->NtQueryInformationFile(
                NtStatus, &HostIoStatusBlock, HostFileInformation.get(), Length,
                FileInformationClass);

            //
            // If we failed we want to know.
            //

            if (SyscallSuccess && !NT_SUCCESS(NtStatus)) {
              __debugbreak();
            }

            //
            // Write back the output parameters, the FileInformation as well
            // as the IOB.
            //

            Backend->VirtWriteDirty(GuestFileInformation,
                                    HostFileInformation.get(), Length);
            Backend->VirtWriteStructDirty(GuestIoStatusBlock,
                                          &HostIoStatusBlock);

            Backend->SimulateReturnFromFunction(NtStatus);
          })) {
    return false;
  }

  if (!g_Backend->SetBreakpoint(
          "ntdll!NtSetInformationFile", [](Backend_t *Backend) {
            //__kernel_entry NTSYSCALLAPI NTSTATUS NtSetInformationFile(
            //  HANDLE                 FileHandle,
            //  PIO_STATUS_BLOCK       IoStatusBlock,
            //  PVOID                  FileInformation,
            //  ULONG                  Length,
            //  FILE_INFORMATION_CLASS FileInformationClass
            //);
            const HANDLE FileHandle = HANDLE(Backend->GetArg(0));
            const Gva_t GuestIoStatusBlock = Backend->GetArgGva(1);
            const Gva_t GuestFileInformation = Backend->GetArgGva(2);
            const uint32_t Length = uint32_t(Backend->GetArg(3));
            const FILE_INFORMATION_CLASS FileInformationClass =
                FILE_INFORMATION_CLASS(Backend->GetArg(4));

            FsDebugPrint("ntdll!NtSetInformationFile(FileHandle={}, "
                         "IoStatusBlock={:#x}, "
                         "FileInformation={:#x}, Length={:#x}, "
                         "FileInformationClass={:#x})\n",
                         FileHandle, GuestIoStatusBlock, GuestFileInformation,
                         Length, FileInformationClass);

            //
            // As usual, if we don't know about this handle, we let the guest
            // figure it out.
            //

            if (!g_FsHandleTable.Known(FileHandle)) {
              FsDebugPrint("Unrecognized file handle.\n");
              return;
            }

            //
            // Grab the stream.
            //

            GuestFile_t *GuestFile = g_FsHandleTable.GetGuestFile(FileHandle);

            //
            // Read the IOB.
            //

            IO_STATUS_BLOCK HostIoStatusBlock;
            Backend->VirtReadStruct(GuestIoStatusBlock, &HostIoStatusBlock);

            //
            // Allocate memory for the FileInformation.
            //

            auto HostFileInformation = std::make_unique<uint8_t[]>(Length);
            Backend->VirtRead(GuestFileInformation, HostFileInformation.get(),
                              Length);

            //
            // Invoke the syscall.
            //

            NTSTATUS NtStatus;
            const bool SyscallSuccess = GuestFile->NtSetInformationFile(
                NtStatus, &HostIoStatusBlock, HostFileInformation.get(), Length,
                FileInformationClass);

            // if we dont support the class, just return back to the guest?
            if (!SyscallSuccess) {
              return;
            }

            //
            // If we failed, we want to know.
            //

            if (!NT_SUCCESS(NtStatus)) {
              __debugbreak();
            }

            //
            // Write the output parameter, the IOB.
            //

            Backend->VirtWriteStructDirty(GuestIoStatusBlock,
                                          &HostIoStatusBlock);

            Backend->SimulateReturnFromFunction(NtStatus);
          })) {
    return false;
  }

  if (!g_Backend->SetBreakpoint("ntdll!NtWriteFile", [](Backend_t *Backend) {
        //__kernel_entry NTSYSCALLAPI NTSTATUS NtWriteFile(
        //  HANDLE           FileHandle,
        //  HANDLE           Event,
        //  PIO_APC_ROUTINE  ApcRoutine,
        //  PVOID            ApcContext,
        //  PIO_STATUS_BLOCK IoStatusBlock,
        //  PVOID            Buffer,
        //  ULONG            Length,
        //  PLARGE_INTEGER   ByteOffset,
        //  PULONG           Key
        //);
        const HANDLE FileHandle = HANDLE(Backend->GetArg(0));
        const uint64_t Event = Backend->GetArg(1);
        const uint64_t ApcRoutine = Backend->GetArg(2);
        const uint64_t ApcContext = Backend->GetArg(3);
        const Gva_t GuestIoStatusBlock = Backend->GetArgGva(4);
        const Gva_t GuestBuffer = Backend->GetArgGva(5);
        const uint32_t Length = uint32_t(Backend->GetArg(6));
        const uint64_t GuestByteOffset = Backend->GetArg(7);
        const uint64_t Key = Backend->GetArg(8);

        FsDebugPrint(
            "ntdll!NtWriteFile(FileHandle={}, Event={:#x}, ApcRoutine={:#x}, "
            "ApcContext={:#x}, "
            "IoStatusBlock={:#x}, Buffer={:#x}, Length={:#x}, "
            "ByteOffset={:#x}, "
            "Key={:#x})\n",
            fmt::ptr(FileHandle), Event, ApcRoutine, ApcContext,
            GuestIoStatusBlock, GuestBuffer, Length, GuestByteOffset, Key);

        //
        // If we don't know the handle, we can't do anything about it.
        //

        if (!g_FsHandleTable.Known(FileHandle)) {
          FsDebugPrint("Unrecognized file handle.\n");
          return;
        }

        //
        // Grab the host handle.
        //

        GuestFile_t *GuestFile = g_FsHandleTable.GetGuestFile(FileHandle);

        //
        // Read the IOB.
        //

        IO_STATUS_BLOCK HostIoStatusBlock;
        Backend->VirtReadStruct(GuestIoStatusBlock, &HostIoStatusBlock);

        if (GuestByteOffset) {
          fmt::print("Need to implement ByteOffset?\n");
          __debugbreak();
          ExitProcess(0);
        }

        //
        // Allocate memory for the buffer.
        //

        auto HostBuffer = std::make_unique<uint8_t[]>(Length);
        Backend->VirtRead(GuestBuffer, HostBuffer.get(), Length);

        NTSTATUS NtStatus;
        const bool SyscallSuccess = GuestFile->NtWriteFile(
            NtStatus, &HostIoStatusBlock, HostBuffer.get(), Length);

        //
        // If it failed, we want to know.
        //

        if (SyscallSuccess && !NT_SUCCESS(NtStatus)) {
          __debugbreak();
        }

        //
        // Write back the buffer as well as the IOB.
        //

        Backend->VirtWriteStructDirty(GuestIoStatusBlock, &HostIoStatusBlock);

        Backend->SimulateReturnFromFunction(NtStatus);
      })) {
    return false;
  }

  if (!g_Backend->SetBreakpoint("ntdll!NtReadFile", [](Backend_t *Backend) {
        // NTSTATUS NtReadFile(
        //  _In_     HANDLE           FileHandle,
        //  _In_opt_ HANDLE           Event,
        //  _In_opt_ PIO_APC_ROUTINE  ApcRoutine,
        //  _In_opt_ PVOID            ApcContext,
        //  _Out_    PIO_STATUS_BLOCK IoStatusBlock,
        //  _Out_    PVOID            Buffer,
        //  _In_     ULONG            Length,
        //  _In_opt_ PLARGE_INTEGER   ByteOffset,
        //  _In_opt_ PULONG           Key
        //);
        const HANDLE FileHandle = HANDLE(Backend->GetArg(0));
        const uint64_t Event = Backend->GetArg(1);
        const uint64_t ApcRoutine = Backend->GetArg(2);
        const uint64_t ApcContext = Backend->GetArg(3);
        const Gva_t GuestIoStatusBlock = Backend->GetArgGva(4);
        const Gva_t GuestBuffer = Backend->GetArgGva(5);
        const uint32_t Length = uint32_t(Backend->GetArg(6));
        const Gva_t GuestByteOffset = Backend->GetArgGva(7);
        const uint64_t Key = Backend->GetArg(8);

        FsDebugPrint(
            "ntdll!NtReadFile(FileHandle={}, Event={:#x}, ApcRoutine={:#x}, "
            "ApcContext={:#x}, IoStatusBlock={:#x}, Buffer={:#x}, "
            "Length={:#x}, ByteOffset={:#x}, Key={:#x})\n",
            fmt::ptr(FileHandle), Event, ApcRoutine, ApcContext,
            GuestIoStatusBlock, GuestBuffer, Length, GuestByteOffset, Key);

        //
        // If we don't know this handle, let's bail.
        //

        if (!g_FsHandleTable.Known(FileHandle)) {
          FsDebugPrint("Unrecognized file handle.\n");
          return;
        }

        //
        // Ensure that the GuestBuffer is faulted-in memory.
        //

        if (GuestBuffer &&
            Backend->PageFaultsMemoryIfNeeded(GuestBuffer, Length)) {
          return;
        }

        //
        // Grab the host stream.
        //

        GuestFile_t *GuestFile = g_FsHandleTable.GetGuestFile(FileHandle);

        //
        // Read the IOB.
        //

        IO_STATUS_BLOCK HostIoStatusBlock;
        Backend->VirtReadStruct(GuestIoStatusBlock, &HostIoStatusBlock);

        //
        // Read the ByteOffset parameter if specified.
        //

        uint64_t HostByteOffset;
        if (GuestByteOffset) {
          fmt::print("Need to implement ByteOffset?\n");
          __debugbreak();
          ExitProcess(0);
        }

        //
        // Allocate memory for the buffer.
        //

        auto HostBuffer = std::make_unique<uint8_t[]>(Length);

        //
        // Invoke the syscall.
        //

        NTSTATUS NtStatus;
        const bool SyscallSuccess = GuestFile->NtReadFile(
            NtStatus, &HostIoStatusBlock, HostBuffer.get(), Length);

        //
        // If it failed, we want to know.
        //

        if (SyscallSuccess && !NT_SUCCESS(NtStatus) &&
            NtStatus != STATUS_END_OF_FILE) {
          __debugbreak();
        }

        //
        // Write back the ByteOffset if it was specified.
        //

        if (GuestByteOffset) {
          Backend->VirtWriteStructDirty(GuestByteOffset, &HostByteOffset);
        }

        //
        // Write back the buffer as well as the IOB.
        //

        Backend->VirtWriteDirty(GuestBuffer, HostBuffer.get(), Length);
        Backend->VirtWriteStructDirty(GuestIoStatusBlock, &HostIoStatusBlock);

        Backend->SimulateReturnFromFunction(NtStatus);
      })) {
    return false;
  }
  return true;
}