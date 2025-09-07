// Axel '0vercl0k' Souchet - June 7 2020
#include "debugger.h"

DebuggerLess_t g_NoDbg;

#ifdef WINDOWS

std::optional<Version_t> GetFileVersion(const char *Name) {
  std::optional<Version_t> Version;
  const DWORD VersionInfoSize = GetFileVersionInfoSizeA(Name, nullptr);
  if (!VersionInfoSize) {
    fmt::print("Failed to get version info size for {} w/ gle={}", Name,
               GetLastError());
    return std::nullopt;
  }

  std::vector<uint8_t> Buffer(VersionInfoSize);
  if (!GetFileVersionInfoA(Name, 0, VersionInfoSize, Buffer.data())) {
    fmt::print("Failed to get the file version blob for {} w/ gle={}", Name,
               GetLastError());
    return std::nullopt;
  }

  void *SubBlock = nullptr;
  UINT SubBlockLen = 0;
  if (!VerQueryValueA(Buffer.data(), "\\", &SubBlock, &SubBlockLen)) {
    fmt::print("Failed to get query the '\\' sub block for {} w/ gle={}", Name,
               GetLastError());
    return std::nullopt;
  }

  const auto *FixedFileInfo = (VS_FIXEDFILEINFO *)SubBlock;
  const uint64_t FileVersion = uint64_t(FixedFileInfo->dwProductVersionMS)
                                   << 32 |
                               uint64_t(FixedFileInfo->dwProductVersionLS);

  Version.emplace(Version_t{.Major = uint16_t(FileVersion >> 48),
                            .Minor = uint16_t(FileVersion >> 32),
                            .Build = uint16_t(FileVersion >> 16),
                            .Revision = uint16_t(FileVersion)});
  return Version;
}

WindowsDebugger_t WindowsDebugger;
Debugger_t *g_Dbg = &WindowsDebugger;

WindowsDebugger_t ::~WindowsDebugger_t() {
  if (Client_) {
    Client_->EndSession(DEBUG_END_ACTIVE_DETACH);
    Client_->Release();
  }

  if (Control_) {
    Control_->Release();
  }

  if (Registers_) {
    Registers_->Release();
  }

  if (Symbols_) {
    Symbols_->Release();
  }
}

[[nodiscard]] bool WindowsDebugger_t::AddSymbol(const char *Name,
                                                const uint64_t Address) const {
  json::json Json;
  std::ifstream SymbolFileIn(SymbolFilePath_);
  if (SymbolFileIn.is_open()) {
    SymbolFileIn >> Json;
  }

  if (Json.contains(Name)) {
    Json[Name] = fmt::format("{:#x}", Address);
  } else {
    Json.emplace(Name, fmt::format("{:#x}", Address));
  }

  std::ofstream SymbolFileOut(SymbolFilePath_);
  SymbolFileOut << Json;
  return true;
}

[[nodiscard]] bool WindowsDebugger_t::Init(const fs::path &DumpPath,
                                           const fs::path &SymbolFilePath,
                                           const fs::path &ExtrasPath) {
  using Value_t = std::optional<Version_t>;

  fmt::print(
      "Initializing the debugger instance.. (this takes a bit of time)\n");

  SymbolFilePath_ = SymbolFilePath;

  char ExePathBuffer[MAX_PATH];
  if (!GetModuleFileNameA(nullptr, &ExePathBuffer[0], sizeof(ExePathBuffer))) {
    fmt::print("GetModuleFileNameA failed.\n");
    return false;
  }

  //
  // Let's grab important paths that will be used later.
  //

  const fs::path ExePath(ExePathBuffer);
  const fs::path ParentDir(ExePath.parent_path());

  auto CopyWinDbgDebugDll = [&](const std::string &DllName) -> bool {
    //
    // If there's already a file with that name; we'll just leave it there.
    //

    if (fs::exists(ParentDir / DllName)) {
      return true;
    }

    //
    // Copy the DLL `Name` into the executable directory if it exists.
    //

    const fs::path WinDbgDebugDllsLocation(
        R"(c:\program Files (x86)\windows kits\10\debuggers\x64)");

    const fs::path WinDbgDebugDll = WinDbgDebugDllsLocation / DllName;
    if (!fs::exists(WinDbgDebugDll)) {

      //
      // If it doesn't exist we have to exit.
      //

      fmt::print("Okay, the debugger class needs {} to function and I "
                 "wasn't able to find it in {}.\n",
                 DllName, WinDbgDebugDll.generic_string());
      return false;
    }

    fs::copy(WinDbgDebugDll, ParentDir);
    fmt::print("Copied {} into wtf's directory..\n",
               WinDbgDebugDll.generic_string());
    return true;
  };

  //
  // All right, the goal here is to grab the debug engine DLLs that will be
  // used to do symbol resolution. But we want to try to do what is best for
  // the user and there are several cases to handle:
  //   - The current target has no 'extras.json' so we have no idea which DLLs
  //   / versions were used to grab the dump. In that case, we do the best we
  //   can and we'll go get the debug DLLs from a locally installed WinDbg
  //   installation if we don't detect them in the CWD.
  //   - The current target has an 'extras.json'
  //     - The paths don't exist on disk, so we fallback to the previous case.
  //     - The paths do exist on disk
  //       - The file versions match up to files so we copy them in the CWD.
  //       - The file versions are higher than what is in the 'extras.json'
  //       file, so we copy them in the CWD and let the user know.
  //       - The file versions are lesser than in the 'extras.json' file, so
  //       we fallback to getting the default DLL off of a default WinDbg
  //       installation
  //

  //
  // Parse the extras file if there's one.
  //

  Extras_t Extras;
  if (fs::exists(ExtrasPath)) {
    Extras = ParseExtrasFile(ExtrasPath);
  }

  auto CopyFromExtrasIfPossible =
      [&](const std::string &DllName,
          const std::optional<Version_t> WantedMinVersion =
              std::nullopt) -> bool {
    if (const auto &ExtraDebugDll = Extras.DebugDlls.find(DllName);
        ExtraDebugDll != Extras.DebugDlls.end()) {

      //
      // If there is an entry, then let's try to copy it in if it exists,
      // otherwise, we'll bring the DLL from its default location.
      //

      const auto &ExtraDebugDllInfo = ExtraDebugDll->second;
      if (fs::exists(ExtraDebugDll->second.Path)) {

        //
        // Before copying it out, let's check the version if we need to do a
        // version check.
        //

        if (WantedMinVersion.has_value()) {
          const auto LocalExtraDllVersion =
              GetFileVersion(ExtraDebugDllInfo.Path.generic_string().c_str());

          if (!LocalExtraDllVersion) {
            fmt::print("Could not figure out the version for {}, bailing\n",
                       DllName);
            return false;
          }

          //
          // If the local file on disk is actually older than the one in the
          // `extras.json`, it doesn't feel like copying it will be beneficial.
          // And at this point I am not quite sure what is the right thing to
          // do, so just warning the user.
          //

          if (*LocalExtraDllVersion < ExtraDebugDllInfo.Version) {
            fmt::print(
                "/!\\ The local {} DLL {} is older than the one in "
                "`extras.json` {}, so we'll just let the user figure it out.",
                ExtraDebugDllInfo.Path.generic_string(), *LocalExtraDllVersion,
                ExtraDebugDllInfo.Version);
            return true;
          }
        }

        fs::copy(ExtraDebugDllInfo.Path, ParentDir,
                 fs::copy_options::overwrite_existing);
        fmt::print("Copied {} {} from `extras.json` into wtf's directory..\n",
                   DllName, ExtraDebugDllInfo.Version);
        return true;
      }

      fmt::print("{} from `extras.json` doesn't exist on disk so going to try "
                 "to find it in the default location..\n",
                 ExtraDebugDllInfo.Path.generic_string());
    }

    //
    // Okay we got there because either the DLL isn't described in the
    // `extras.json`, or because maybe there's no `extras.json`. Regardless,
    // let's fallback on trying to find the DLL from their default locations.
    //

    return CopyWinDbgDebugDll(DllName);
  };

  //
  // We'll start to figure out if there are DLLs available in the CWD already,
  // and grab their versions.
  //
  // If they don't exist, and they are referenced in the extra file, let's grab
  // it in
  //
  // Useful documentation about symsrv/dbgeng.dll (
  // https://docs.microsoft.com/en-us/windows/win32/debug/using-symsrv
  // "Installation")

  const std::vector<std::string> DllNames = [&]() {
    std::vector<std::string> DllNamesInner;
    if (Extras.DebugDlls.size() > 0) {
      for (const auto &[DllName, _] : Extras.DebugDlls) {
        DllNamesInner.push_back(DllName);
      }
    } else {
      DllNamesInner = {"dbghelp.dll", "symsrv.dll", "dbgeng.dll",
                       "dbgcore.dll"};
    }
    return DllNamesInner;
  }();

  for (const auto &DllName : DllNames) {

    //
    // Does it exist in the CWD?
    //

    const auto DllPath = ParentDir / DllName;

    if (!fs::exists(DllPath)) {

      //
      // If it doesn't exist, let's try to bring in the file from `extras.json`
      // if possible.
      //

      if (!CopyFromExtrasIfPossible(DllName)) {
        return false;
      }

      continue;
    }

    //
    // Okay the file exists in the CWD, let's get its file version.
    //

    const auto DllVersion = GetFileVersion(DllName.c_str());
    if (!DllVersion) {
      fmt::print("Could not figure out the version for {}, bailing\n", DllName);
      return false;
    }

    //
    // Does it exist in the `extras.json` file?
    //

    const auto ExtraDll = Extras.DebugDlls.find(DllName);
    if (ExtraDll != Extras.DebugDlls.end()) {

      const auto &ExtraDllInfo = ExtraDll->second;

      //
      // If it does exists, let's check if the version is greater..
      //

      if (ExtraDllInfo.Version > *DllVersion) {

        //
        // if that is the case, let's try to bring the file over.
        //

        if (!CopyFromExtrasIfPossible(DllName, ExtraDllInfo.Version)) {
          return false;
        }
      } else if (ExtraDllInfo.Version == *DllVersion) {

        //
        // We'll just avoid spamming stdout in that case.
        //

      } else {
        fmt::print("The local {} {} is more recent ({}) than the one in "
                   "`extras.json`, so keeping things as is.\n",
                   DllName, *DllVersion, ExtraDllInfo.Version);
      }
    }
  }

  HRESULT Status = DebugCreate(__uuidof(IDebugClient), (void **)&Client_);
  if (FAILED(Status)) {
    fmt::print("DebugCreate failed with hr={:#x}\n", Status);
    return false;
  }

  Status = Client_->QueryInterface(__uuidof(IDebugControl), (void **)&Control_);
  if (FAILED(Status)) {
    fmt::print("QueryInterface/IDebugControl failed with hr={:#x}\n", Status);
    return false;
  }

  Status =
      Client_->QueryInterface(__uuidof(IDebugRegisters), (void **)&Registers_);
  if (FAILED(Status)) {
    fmt::print("QueryInterface/IDebugRegisters failed with hr={:#x}\n", Status);
    return false;
  }

  Status =
      Client_->QueryInterface(__uuidof(IDebugSymbols3), (void **)&Symbols_);
  if (FAILED(Status)) {
    fmt::print("QueryInterface/IDebugSymbols failed with hr={:#x}\n", Status);
    return false;
  }

  //
  // Turn the below on to debug issues.
  //
#if 0
    const uint32_t SYMOPT_DEBUG = 0x80'00'00'00;
    Status = Symbols_->SetSymbolOptions(SYMOPT_DEBUG);
    if (FAILED(Status)) {
      fmt::print("IDebugSymbols::SetSymbolOptions failed with hr={:#x}\n ",
                 Status);
      return false;
    }
    Client_->SetOutputCallbacks(&StdioCallbacks_);
#endif

  const std::string &DumpFileString = DumpPath.string();
  const char *DumpFileA = DumpFileString.c_str();
  Status = Client_->OpenDumpFile(DumpFileA);
  if (FAILED(Status)) {
    fmt::print("OpenDumpFile({}) failed with hr={:#x}\n", DumpFileString,
               Status);
    return false;
  }

  //
  // Note The engine doesn't completely attach to the dump file until the
  // WaitForEvent method has been called. When a dump file is created from a
  // process or kernel, information about the last event is stored in the
  // dump file. After the dump file is opened, the next time execution is
  // attempted, the engine will generate this event for the event callbacks.
  // Only then does the dump file become available in the debugging session.
  // https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/dbgeng/nf-dbgeng-idebugclient-opendumpfile
  //

  Status = WaitForEvent();
  if (FAILED(Status)) {
    fmt::print("WaitForEvent for OpenDumpFile failed with hr={:#x}\n", Status);
    return false;
  }

  //
  // To debug what might be wrong with the debugger, you can execute command
  // with `Execute` like below.
  //

  // Execute("? ucrtbase");
  // Execute("? nt!SwapContext");
  return true;
}

[[nodiscard]] HRESULT WindowsDebugger_t::WaitForEvent() const {
  const HRESULT Status = Control_->WaitForEvent(DEBUG_WAIT_DEFAULT, INFINITE);
  if (FAILED(Status)) {
    fmt::print("IDebugControl::WaitForEvent failed with {:#x}\n", Status);
  }
  return Status;
}

[[nodiscard]] HRESULT WindowsDebugger_t::Execute(const char *Str) const {
  const HRESULT Status =
      Control_->Execute(DEBUG_OUTCTL_THIS_CLIENT | DEBUG_OUTCTL_NOT_LOGGED, Str,
                        DEBUG_EXECUTE_NOT_LOGGED);
  if (FAILED(Status)) {
    fmt::print("IDebugControl::Execute failed with {:#x}\n", Status);
  }

  return Status;
}

[[nodiscard]] uint64_t
WindowsDebugger_t::GetModuleBase(const char *Name) const {
  uint64_t Base = 0;
  const HRESULT Status =
      Symbols_->GetModuleByModuleName(Name, 0, nullptr, &Base);
  if (FAILED(Status)) {
    return 0;
  }

  if (!AddSymbol(Name, Base)) {
    __debugbreak();
    return 0;
  }

  return Base;
}

uint64_t WindowsDebugger_t::GetSymbol(const char *Name) const {
  uint64_t Offset = 0;
  HRESULT Status = Symbols_->GetOffsetByName(Name, &Offset);
  if (FAILED(Status)) {
    if (Status == S_FALSE) {
      // If GetOffsetByName finds multiple matches
      // for the name it can return any one of them.
      // In that case it will return S_FALSE to indicate
      // that ambiguity was arbitrarily resolved.
      __debugbreak();
    }
  }

  if (!AddSymbol(Name, Offset)) {
    __debugbreak();
  }

  return Offset;
}

const std::string &WindowsDebugger_t::GetName(const uint64_t SymbolAddress,
                                              const bool Symbolized) {
  if (SymbolCache_.contains(SymbolAddress)) {
    return SymbolCache_.at(SymbolAddress);
  }

  const size_t NameSizeMax = MAX_PATH;
  char Buffer[NameSizeMax];
  uint64_t Offset = 0;

  if (Symbolized) {
    const HRESULT Status = Symbols_->GetNameByOffset(
        SymbolAddress, Buffer, NameSizeMax, nullptr, &Offset);
    if (FAILED(Status)) {
      __debugbreak();
    }
  } else {
    ULONG Index;
    ULONG64 Base;
    HRESULT Status =
        Symbols_->GetModuleByOffset(SymbolAddress, 0, &Index, &Base);

    if (FAILED(Status)) {
      __debugbreak();
    }

    ULONG NameSize;
    Status = Symbols_->GetModuleNameString(DEBUG_MODNAME_MODULE, Index, Base,
                                           Buffer, NameSizeMax, &NameSize);

    if (FAILED(Status)) {
      __debugbreak();
    }

    Offset = SymbolAddress - Base;
  }

  SymbolCache_.emplace(SymbolAddress, fmt::format("{}+{:#x}", Buffer, Offset));
  return SymbolCache_.at(SymbolAddress);
}

#else
Debugger_t *g_Dbg = &g_NoDbg;
#endif