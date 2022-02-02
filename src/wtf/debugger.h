// Axel '0vercl0k' Souchet - March 14 2020
#pragma once
#include "globals.h"
#include "nlohmann/json.hpp"
#include "platform.h"
#include "tsl/robin_map.h"
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
namespace json = nlohmann;

#ifdef WINDOWS
#include "globals.h"
#include <DbgEng.h>

// Highly inspired from:
// C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\sdk\samples\dumpstk
class StdioOutputCallbacks : public IDebugOutputCallbacks {
public:
  // IUnknown
  STDMETHODIMP
  QueryInterface(REFIID InterfaceId, PVOID *Interface) {
    *Interface = NULL;

    if (IsEqualIID(InterfaceId, __uuidof(IUnknown)) ||
        IsEqualIID(InterfaceId, __uuidof(IDebugOutputCallbacks))) {
      *Interface = (IDebugOutputCallbacks *)this;
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  STDMETHODIMP_(ULONG) AddRef() {
    // This class is designed to be static so
    // there's no true refcount.
    return 1;
  }

  STDMETHODIMP_(ULONG) Release() {
    // This class is designed to be static so
    // there's no true refcount.
    return 0;
  }

  STDMETHODIMP Output(ULONG, PCSTR Text) {
    fmt::print("{}", Text);
    return S_OK;
  }
};

class Debugger_t {
  IDebugClient *Client_ = nullptr;
  IDebugControl *Control_ = nullptr;
  IDebugRegisters *Registers_ = nullptr;
  IDebugSymbols3 *Symbols_ = nullptr;

  fs::path SymbolFilePath_;
  StdioOutputCallbacks StdioCallbacks_;
  tsl::robin_map<uint64_t, std::string> SymbolCache_;

public:
  explicit Debugger_t() = default;

  ~Debugger_t() {
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

  Debugger_t(const Debugger_t &) = delete;
  Debugger_t &operator=(const Debugger_t &) = delete;

  [[nodiscard]] bool AddSymbol(const char *Name, const uint64_t Address) const {
    json::json Json;
    std::ifstream SymbolFileIn(SymbolFilePath_);
    if (SymbolFileIn.is_open()) {
      SymbolFileIn >> Json;
    }

    if (Json.contains(Name)) {
      return true;
    }

    Json.emplace(Name, fmt::format("{:#x}", Address));

    std::ofstream SymbolFileOut(SymbolFilePath_);
    SymbolFileOut << Json;
    return true;
  }

  [[nodiscard]] bool Init(const fs::path &DumpPath,
                          const fs::path &SymbolFilePath) {
    fmt::print(
        "Initializing the debugger instance.. (this takes a bit of time)\n");

    SymbolFilePath_ = SymbolFilePath;

    //
    // Ensure that we both have dbghelp.dll and symsrv.dll in the current
    // directory otherwise things don't work. cf
    // https://docs.microsoft.com/en-us/windows/win32/debug/using-symsrv
    // "Installation"
    //

    char ExePathBuffer[MAX_PATH];
    if (!GetModuleFileNameA(nullptr, &ExePathBuffer[0],
                            sizeof(ExePathBuffer))) {
      fmt::print("GetModuleFileNameA failed.\n");
      return false;
    }

    //
    // Let's check if the dlls exist in the same path as the application.
    //

    const fs::path ExePath(ExePathBuffer);
    const fs::path ParentDir(ExePath.parent_path());
    const std::vector<std::string_view> Dlls = {"dbghelp.dll", "symsrv.dll",
                                                "dbgeng.dll", "dbgcore.dll"};
    const fs::path DefaultDbgDllLocation(
        R"(c:\program Files (x86)\windows kits\10\debuggers\x64)");

    for (const auto &Dll : Dlls) {
      if (fs::exists(ParentDir / Dll)) {
        continue;
      }

      //
      // Apparently it doesn't. Be nice and try to find them by ourselves.
      //

      const fs::path DbgDllLocation(DefaultDbgDllLocation / Dll);
      if (!fs::exists(DbgDllLocation)) {

        //
        // If it doesn't exist we have to exit.
        //

        fmt::print("The debugger class expects debug dlls in the "
                   "directory "
                   "where the application is running from.\n");
        return false;
      }

      //
      // Sounds like we are able to fix the problem ourselves. Copy the files
      // in the directory where the application is running from and move on!
      //

      fs::copy(DbgDllLocation, ParentDir);
      fmt::print("Copied {} into the "
                 "executable directory..\n",
                 DbgDllLocation.generic_string());
    }

    HRESULT Status = DebugCreate(__uuidof(IDebugClient), (void **)&Client_);
    if (FAILED(Status)) {
      fmt::print("DebugCreate failed with hr={:#x}\n", Status);
      return false;
    }

    Status =
        Client_->QueryInterface(__uuidof(IDebugControl), (void **)&Control_);
    if (FAILED(Status)) {
      fmt::print("QueryInterface/IDebugControl failed with hr={:#x}\n", Status);
      return false;
    }

    Status = Client_->QueryInterface(__uuidof(IDebugRegisters),
                                     (void **)&Registers_);
    if (FAILED(Status)) {
      fmt::print("QueryInterface/IDebugRegisters failed with hr={:#x}\n",
                 Status);
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

    //#define SYMOPT_DEBUG 0x80000000
    //    Status = Symbols_->SetSymbolOptions(SYMOPT_DEBUG);
    //    if (FAILED(Status)) {
    //      fmt::print("IDebugSymbols::SetSymbolOptions failed with
    //      hr={:#x}\n", Status); return false;
    //    }
    // Client_->SetOutputCallbacks(&StdioCallbacks_);

    const std::string &DumpFileString = DumpPath.string();
    const char *DumpFileA = DumpFileString.c_str();
    Status = Client_->OpenDumpFile(DumpFileA);
    if (FAILED(Status)) {
      fmt::print("OpenDumpFile({}) failed with hr={:#x}\n", DumpFileString,
                 Status);
      return false;
    }

    // Note The engine doesn't completely attach to the dump file until the
    // WaitForEvent method has been called. When a dump file is created from a
    // process or kernel, information about the last event is stored in the
    // dump file. After the dump file is opened, the next time execution is
    // attempted, the engine will generate this event for the event callbacks.
    // Only then does the dump file become available in the debugging session.
    // https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/dbgeng/nf-dbgeng-idebugclient-opendumpfile

    Status = WaitForEvent();
    if (FAILED(Status)) {
      fmt::print("WaitForEvent for OpenDumpFile failed with hr={:#x}\n",
                 Status);
      return false;
    }

    // XXX: Figure out whats wrong.
    // Execute("? ucrtbase");
    // Execute("? ucrtbase");
    return true;
  }

  [[nodiscard]] HRESULT WaitForEvent() const {
    const HRESULT Status = Control_->WaitForEvent(DEBUG_WAIT_DEFAULT, INFINITE);
    if (FAILED(Status)) {
      fmt::print("Execute::WaitForEvent failed with {:#x}\n", Status);
    }
    return Status;
  }

  [[nodiscard]] HRESULT Execute(const char *Str) const {
    const HRESULT Status =
        Control_->Execute(DEBUG_OUTCTL_THIS_CLIENT | DEBUG_OUTCTL_NOT_LOGGED,
                          Str, DEBUG_EXECUTE_NOT_LOGGED);
    if (FAILED(Status)) {
      fmt::print("IDebugControl::Execute failed with {:#x}\n", Status);
      return Status;
    }

    return WaitForEvent();
  }

  [[nodiscard]] uint64_t GetModuleBase(const char *Name) const {
    uint64_t Base = 0;
    const HRESULT Status =
        Symbols_->GetModuleByModuleName(Name, 0, nullptr, &Base);
    if (FAILED(Status)) {
      __debugbreak();
      Base = 0;
    }

    if (!AddSymbol(Name, Base)) {
      __debugbreak();
      Base = 0;
    }

    return Base;
  }

  uint64_t GetSymbol(const char *Name) const {
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

  const std::string &GetName(const uint64_t SymbolAddress,
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
};
#else
#include <cstdlib>

class Debugger_t {
  std::unordered_map<std::string, uint64_t> Symbols_;

public:
  explicit Debugger_t() = default;
  bool Init(const fs::path &DumpPath, const fs::path &SymbolFilePath) {
    json::json Json;
    std::ifstream SymbolFile(SymbolFilePath);
    SymbolFile >> Json;
    for (const auto &[Key, Value] : Json.items()) {
      const uint64_t Address =
          std::strtoull(Value.get<std::string>().c_str(), nullptr, 0);
      Symbols_.emplace(Key, Address);
    }

    fmt::print("The debugger instance is loaded with {} items\n",
               Symbols_.size());
    return true;
  }

  uint64_t GetModuleBase(const char *Name) const { return GetSymbol(Name); }

  uint64_t GetSymbol(const char *Name) const {
    if (!Symbols_.contains(Name)) {
      fmt::print("{} could not be found in the symbol store\n", Name);
      exit(0);
      return 0;
    }

    return Symbols_.at(Name);
  }

  const std::string &GetName(const uint64_t SymbolAddress,
                             const bool Symbolized) {
    static const std::string foo("hello");
    fmt::print("GetName does not work on Linux\n");
    exit(0);
    return foo;
  }
};
#endif

extern Debugger_t g_Dbg;