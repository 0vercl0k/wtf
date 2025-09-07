// Axel '0vercl0k' Souchet - March 14 2020
#pragma once
#include "globals.h"
#include "nlohmann/json.hpp"
#include "platform.h"
#include "tsl/robin_map.h"
#include "utils.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
namespace json = nlohmann;

struct Debugger_t {
  virtual bool Init(const fs::path &DumpPath, const fs::path &SymbolFilePath,
                    const fs::path &ExtrasFilePath) = 0;

  virtual uint64_t GetModuleBase(const char *Name) const = 0;

  virtual uint64_t GetSymbol(const char *Name) const = 0;

  virtual const std::string &GetName(const uint64_t SymbolAddress,
                                     const bool Symbolized) = 0;
};

class DebuggerLess_t : public Debugger_t {
  std::unordered_map<std::string, uint64_t> Symbols_;

public:
  explicit DebuggerLess_t() = default;
  bool Init(const fs::path &DumpPath, const fs::path &SymbolFilePath,
            const fs::path &) {
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

class WindowsDebugger_t : public Debugger_t {
  IDebugClient *Client_ = nullptr;
  IDebugControl *Control_ = nullptr;
  IDebugRegisters *Registers_ = nullptr;
  IDebugSymbols3 *Symbols_ = nullptr;

  fs::path SymbolFilePath_;
  StdioOutputCallbacks StdioCallbacks_;
  tsl::robin_map<uint64_t, std::string> SymbolCache_;

public:
  explicit WindowsDebugger_t() = default;

  ~WindowsDebugger_t();

  WindowsDebugger_t(const WindowsDebugger_t &) = delete;
  WindowsDebugger_t &operator=(const WindowsDebugger_t &) = delete;

  [[nodiscard]] bool AddSymbol(const char *Name, const uint64_t Address) const;

  [[nodiscard]] bool Init(const fs::path &DumpPath,
                          const fs::path &SymbolFilePath,
                          const fs::path &ExtrasPath);

  [[nodiscard]] HRESULT WaitForEvent() const;

  [[nodiscard]] HRESULT Execute(const char *Str) const;

  [[nodiscard]] uint64_t GetModuleBase(const char *Name) const;

  uint64_t GetSymbol(const char *Name) const;

  const std::string &GetName(const uint64_t SymbolAddress,
                             const bool Symbolized);
};
#else

#endif

extern Debugger_t *g_Dbg;
extern DebuggerLess_t g_NoDbg;