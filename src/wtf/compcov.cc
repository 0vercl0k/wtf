// Theodor Arsenij 'm4drat' - May 23 2023

#include "compcov.h"
#include "backend.h"
#include "debugger.h"
#include "fmt/core.h"
#include "globals.h"
#include "nt.h"
#include "utils.h"
#include <fmt/format.h>
#include <string_view>
#include <vector>

constexpr bool CompcovLoggingOn = false;

template <typename... Args_t>
void CompcovPrint(const char *Format, const Args_t &...args) {
  if constexpr (CompcovLoggingOn) {
    fmt::print("compcov: ");
    fmt::print(fmt::runtime(Format), args...);
  }
}

//
// Get the minimum length of two strings, clamping at max_length.
//

template <class T>
uint64_t CompcovStrlen2(const T *s1, const T *s2, uint64_t max_length) {

  // from https://github.com/googleprojectzero/CompareCoverage

  size_t len = 0;
  for (; len < max_length && s1[len] != 0x0 && s2[len] != 0x0; len++) {
  }

  return len;
}

//
// Do a comparison of two buffers and update the coverage accordingly.
//

template <class T>
void CompcovTrace(const uint64_t RetLoc, const T *Buffer1, const T *Buffer2,
                  const uint64_t Length) {
  BochscpuBackend_t *BochsBackend =
      dynamic_cast<BochscpuBackend_t *>(g_Backend);
  if (!BochsBackend) {
    throw std::runtime_error("CompcovTrace: Unsupported backend, only BochsCPU "
                             "backend is supported");
  }

  uint64_t HashedLoc = SplitMix64(RetLoc);
  for (uint32_t i = 0; i < Length && Buffer1[i] == Buffer2[i]; i++) {
    if (BochsBackend->InsertCoverageEntry(Gva_t(HashedLoc + i)))
      BochsBackend->IncCompcovUniqueHits();
  }
}

//
// Generic handler for strcmp-like functions.
//

void CompcovHandleStrcmp(Backend_t *Backend, Gva_t Str1Ptr, Gva_t Str2Ptr) {
  //
  // Read the strings.
  //

  std::array<uint8_t, COMPCOV_MAX_CMP_LENGTH + 1> Str1{};
  std::array<uint8_t, COMPCOV_MAX_CMP_LENGTH + 1> Str2{};

  bool Str1ReadRes =
      Backend->VirtRead(Str1Ptr, Str1.data(), COMPCOV_MAX_CMP_LENGTH);
  bool Str2ReadRes =
      Backend->VirtRead(Str2Ptr, Str2.data(), COMPCOV_MAX_CMP_LENGTH);

  //
  // Check whether we were able to read the strings.
  //

  if (!Str1ReadRes || !Str2ReadRes) {
    CompcovPrint("{}: Failed to read strings\n", __func__);
    return;
  }

  uint64_t Length =
      CompcovStrlen2(Str1.data(), Str2.data(), COMPCOV_MAX_CMP_LENGTH);

  //
  // Skip if the comparison is too long, as we don't want to clutter the
  // coverage database.
  //

  if (Length >= COMPCOV_MAX_CMP_LENGTH) {
    CompcovPrint("{}: MaxCount >= COMPCOV_MAX_CMP_LENGTH\n", __func__);
    return;
  }

  //
  // As the breakpoint is set on the beginning of the function, we should be
  // able to extract the return address by reading the first QWORD from the
  // stack.
  //

  uint64_t RetLoc = Backend->VirtRead8(Gva_t(Backend->Rsp()));

  CompcovPrint("Strcmp(\"{}\", \"{}\", {}) -> {:#x}\n", (char *)Str1.data(),
               (char *)Str2.data(), Length, RetLoc);

  //
  // If the return location is 0, then the VirtRead8() failed.
  //

  if (RetLoc == 0) {
    CompcovPrint("{}: RetLoc == 0\n", __func__);
    return;
  }

  CompcovTrace(RetLoc, Str1.data(), Str2.data(), Length);
}

//
// Strcmp hook.
//

void CompcovHookStrcmp(Backend_t *Backend) {
  //
  // Extract the arguments.
  //

  Gva_t Str1Ptr = Gva_t(Backend->GetArg(0));
  Gva_t Str2Ptr = Gva_t(Backend->GetArg(1));

  CompcovHandleStrcmp(Backend, Str1Ptr, Str2Ptr);
}

//
// Generic handler for strncmp-like functions.
//

void CompcovHandleStrncmp(Backend_t *Backend, Gva_t Str1Ptr, Gva_t Str2Ptr,
                          uint64_t MaxCount) {

  //
  // Skip if the comparison is too long, as we don't want to clutter the
  // coverage database.
  //

  if (MaxCount >= COMPCOV_MAX_CMP_LENGTH) {
    CompcovPrint("{}: MaxCount >= COMPCOV_MAX_CMP_LENGTH\n", __func__);
    return;
  }

  //
  // Read the strings.
  //

  std::array<uint8_t, COMPCOV_MAX_CMP_LENGTH + 1> Str1{};
  std::array<uint8_t, COMPCOV_MAX_CMP_LENGTH + 1> Str2{};

  bool Str1ReadRes = Backend->VirtRead(Str1Ptr, Str1.data(), MaxCount);
  bool Str2ReadRes = Backend->VirtRead(Str2Ptr, Str2.data(), MaxCount);

  //
  // Check whether we were able to read the strings.
  //

  if (!Str1ReadRes || !Str2ReadRes) {
    CompcovPrint("{}: Failed to read strings\n", __func__);
    return;
  }

  uint64_t Length = CompcovStrlen2(Str1.data(), Str2.data(), MaxCount);

  //
  // As the breakpoint is set on the beginning of the function, we should be
  // able to extract the return address by reading the first QWORD from the
  // stack.
  //

  uint64_t RetLoc = Backend->VirtRead8(Gva_t(Backend->Rsp()));

  CompcovPrint("Strncmp(\"{}\", \"{}\", {}) -> {:#x}\n", (char *)Str1.data(),
               (char *)Str2.data(), Length, RetLoc);

  //
  // If the return location is 0, then the VirtRead8() failed.
  //

  if (RetLoc == 0) {
    CompcovPrint("{}: RetLoc == 0\n", __func__);
    return;
  }

  CompcovTrace(RetLoc, Str1.data(), Str2.data(), Length);
}

//
// Strncmp hook.
//

void CompcovHookStrncmp(Backend_t *Backend) {
  //
  // Extract the arguments.
  //

  Gva_t Str1Ptr = Gva_t(Backend->GetArg(0));
  Gva_t Str2Ptr = Gva_t(Backend->GetArg(1));
  uint64_t MaxCount = Backend->GetArg(2);

  CompcovHandleStrncmp(Backend, Str1Ptr, Str2Ptr, MaxCount);
}

//
// Generic handler for wcscmp-like functions.
//

void CompcovHandleWcscmp(Backend_t *Backend, Gva_t Wstr1Ptr, Gva_t Wstr2Ptr) {
  //
  // Read the strings (still as uint8_t, as we don't want to deal with
  // wchar_t)
  //

  std::array<uint8_t, COMPCOV_MAX_CMP_LENGTH> Wstr1{};
  std::array<uint8_t, COMPCOV_MAX_CMP_LENGTH> Wstr2{};

  bool Wstr1ReadRes =
      Backend->VirtRead(Wstr1Ptr, Wstr1.data(), COMPCOV_MAX_CMP_LENGTH);
  bool Wstr2ReadRes =
      Backend->VirtRead(Wstr2Ptr, Wstr2.data(), COMPCOV_MAX_CMP_LENGTH);

  //
  // Check whether we were able to read the strings.
  //

  if (!Wstr1ReadRes || !Wstr2ReadRes) {
    CompcovPrint("{}: Failed to read strings\n", __func__);
    return;
  }

  //
  // Calculate the length of the strings in bytes.
  //

  uint64_t Length =
      CompcovStrlen2((uint16_t *)Wstr1.data(), (uint16_t *)Wstr2.data(),
                     COMPCOV_MAX_CMP_LENGTH / 2) *
      sizeof(wchar_t);

  //
  // Skip if the comparison is too long, as we don't want to clutter the
  // coverage database.
  //

  if (Length >= COMPCOV_MAX_CMP_LENGTH) {
    CompcovPrint("{}: MaxCount >= COMPCOV_MAX_CMP_LENGTH\n", __func__);
    return;
  }

  //
  // As the breakpoint is set on the beginning of the function, we should be
  // able to extract the return address by reading the first QWORD from the
  // stack.
  //

  uint64_t RetLoc = Backend->VirtRead8(Gva_t(Backend->Rsp()));

  CompcovPrint("Wcscmp(\"{}\", \"{}\", {}) -> {:#x}\n",
               BytesToHexString(Wstr1.data(), Length),
               BytesToHexString(Wstr2.data(), Length), Length, RetLoc);

  //
  // If the return location is 0, then the VirtRead8() failed.
  //

  if (RetLoc == 0) {
    CompcovPrint("{}: RetLoc == 0\n", __func__);
    return;
  }

  CompcovTrace(RetLoc, Wstr1.data(), Wstr2.data(), Length);
}

//
// Wcscmp hook.
//

void CompcovHookWcscmp(Backend_t *Backend) {
  //
  // Extract the arguments.
  //

  Gva_t Wstr1Ptr = Gva_t(Backend->GetArg(0));
  Gva_t Wstr2Ptr = Gva_t(Backend->GetArg(1));

  CompcovHandleWcscmp(Backend, Wstr1Ptr, Wstr2Ptr);
}

//
// Generic handler for wcsncmp-like functions.
//

void CompcovHandleWcsncmp(Backend_t *Backend, Gva_t Wstr1Ptr, Gva_t Wstr2Ptr,
                          uint64_t MaxCount) {
  //
  // Skip if the comparison is too long, as we don't want to clutter the
  // coverage database.
  //

  if (MaxCount * sizeof(wchar_t) >= COMPCOV_MAX_CMP_LENGTH) {
    CompcovPrint(
        "{}: MaxCount * sizeof(wchar_t) >= COMPCOV_MAX_CMP_LENGTH / 2\n",
        __func__);
    return;
  }

  //
  // Read the strings.
  //

  std::array<uint8_t, COMPCOV_MAX_CMP_LENGTH> Wstr1{};
  std::array<uint8_t, COMPCOV_MAX_CMP_LENGTH> Wstr2{};

  bool Wstr1ReadRes = Backend->VirtRead(Wstr1Ptr, Wstr1.data(), MaxCount);
  bool Wstr2ReadRes = Backend->VirtRead(Wstr2Ptr, Wstr2.data(), MaxCount);

  //
  // Check whether we were able to read the strings.
  //

  if (!Wstr1ReadRes || !Wstr2ReadRes) {
    CompcovPrint("{}: Failed to read strings\n", __func__);
    return;
  }

  uint64_t Length =
      CompcovStrlen2((uint16_t *)Wstr1.data(), (uint16_t *)Wstr2.data(),
                     COMPCOV_MAX_CMP_LENGTH / 2) *
      sizeof(wchar_t);

  //
  // As the breakpoint is set on the beginning of the function, we should be
  // able to extract the return address by reading the first QWORD from the
  // stack.
  //

  uint64_t RetLoc = Backend->VirtRead8(Gva_t(Backend->Rsp()));

  CompcovPrint("Wcsncmp(\"{}\", \"{}\", {}) -> {:#x}\n",
               BytesToHexString(Wstr1.data(), Length),
               BytesToHexString(Wstr2.data(), Length), Length, RetLoc);

  //
  // If the return location is 0, then the VirtRead8() failed.
  //

  if (RetLoc == 0) {
    CompcovPrint("{}: RetLoc == 0\n", __func__);
    return;
  }

  CompcovTrace(RetLoc, Wstr1.data(), Wstr2.data(), Length);
}

//
// Wcsncmp hook.
//

void CompcovHookWcsncmp(Backend_t *Backend) {
  //
  // Extract the arguments.
  //

  Gva_t Wstr1Ptr = Gva_t(Backend->GetArg(0));
  Gva_t Wstr2Ptr = Gva_t(Backend->GetArg(1));
  uint64_t MaxCount = Backend->GetArg(2);

  CompcovHandleWcsncmp(Backend, Wstr1Ptr, Wstr2Ptr, MaxCount);
}

//
// Generic hook for CompareStringA. We ignore all the flags, custom locales,
// and anything else. The only thing that matters is whether the strings are
// equal or not.
//

void CompcovHookCompareStringA(Backend_t *Backend) {
  //
  // Extract the arguments.
  //

  uint32_t DwCmpFlags = Backend->GetArg(1);
  Gva_t LpString1 = Gva_t(Backend->GetArg(2));
  int32_t String1Length = Backend->GetArg(3);
  Gva_t LpString2 = Gva_t(Backend->GetArg(4));
  int32_t String2Length = Backend->GetArg(5);

  //
  // CompareStringA() might be called with a negative length, which means that
  // the string is null-terminated, and the length should be calculated
  // manually.
  //
  if (String1Length < 0) {
    String1Length = COMPCOV_MAX_CMP_LENGTH - 1;
  }

  if (String2Length < 0) {
    String2Length = COMPCOV_MAX_CMP_LENGTH - 1;
  }

  //
  // Make sure that the length is not too long.
  //
  String1Length = std::min(String1Length, (int32_t)COMPCOV_MAX_CMP_LENGTH - 1);
  String2Length = std::min(String2Length, (int32_t)COMPCOV_MAX_CMP_LENGTH - 1);

  //
  // Read the strings.
  //

  std::array<uint8_t, COMPCOV_MAX_CMP_LENGTH + 1> Str1{};
  std::array<uint8_t, COMPCOV_MAX_CMP_LENGTH + 1> Str2{};

  bool Str1ReadRes = Backend->VirtRead(LpString1, Str1.data(), String1Length);
  bool Str2ReadRes = Backend->VirtRead(LpString2, Str2.data(), String2Length);

  //
  // Check whether we were able to read the strings.
  //

  if (!Str1ReadRes || !Str2ReadRes) {
    CompcovPrint("{}: Failed to read strings\n", __func__);
    return;
  }

  uint64_t LengthBytes =
      CompcovStrlen2(Str1.data(), Str2.data(), COMPCOV_MAX_CMP_LENGTH);

  //
  // Skip if the comparison is too long, as we don't want to clutter the
  // coverage database.
  //

  if (LengthBytes >= COMPCOV_MAX_CMP_LENGTH) {
    CompcovPrint("{}: LengthBytes >= COMPCOV_MAX_CMP_LENGTH\n", __func__);
    return;
  }

  //
  // As the breakpoint is set on the beginning of the function, we should be
  // able to extract the return address by reading the first QWORD from the
  // stack.
  //
  uint64_t RetLoc = Backend->VirtRead8(Gva_t(Backend->Rsp()));

  CompcovPrint("CompareStringA(..., {:#x}, \"{}\", {}, \"{}\", {}) -> {:#x}\n",
               DwCmpFlags, Str1.data(), String1Length, Str2.data(),
               String2Length, RetLoc);

  //
  // If the return location is 0, then the VirtRead8() failed.
  //

  if (RetLoc == 0) {
    CompcovPrint("{}: RetLoc == 0\n", __func__);
    return;
  }

  CompcovTrace(RetLoc, Str1.data(), Str2.data(), LengthBytes);
}

//
// Generic hook for CompareStringW. We ignore all the flags, custom locales,
// and anything else. The only thing that matters is whether the strings are
// equal or not.
//

void CompcovHookCompareStringW(Backend_t *Backend) {
  //
  // Extract the arguments.
  //

  uint32_t DwCmpFlags = Backend->GetArg(1);
  Gva_t LpString1 = Gva_t(Backend->GetArg(2));
  int32_t String1Length = Backend->GetArg(3);
  Gva_t LpString2 = Gva_t(Backend->GetArg(4));
  int32_t String2Length = Backend->GetArg(5);

  //
  // CompareStringW() might be called with a negative length, which means that
  // the string is null-terminated, and the length should be calculated
  // manually.
  //
  const int32_t MaxLengthCh = COMPCOV_MAX_CMP_LENGTH / sizeof(wchar_t) - 1;

  if (String1Length < 0) {
    String1Length = MaxLengthCh;
  }

  if (String2Length < 0) {
    String2Length = MaxLengthCh;
  }

  //
  // Make sure that the length is within the limits.
  //
  uint32_t String1LengthBytes =
      std::min(String1Length, MaxLengthCh) * sizeof(wchar_t);
  uint32_t String2LengthBytes =
      std::min(String2Length, MaxLengthCh) * sizeof(wchar_t);

  //
  // Read the strings.
  //

  std::array<uint8_t, COMPCOV_MAX_CMP_LENGTH> Wstr1{};
  std::array<uint8_t, COMPCOV_MAX_CMP_LENGTH> Wstr2{};

  bool Wstr1ReadRes =
      Backend->VirtRead(LpString1, Wstr1.data(), String1LengthBytes);
  bool Wstr2ReadRes =
      Backend->VirtRead(LpString2, Wstr2.data(), String2LengthBytes);

  //
  // Check whether we were able to read the strings.
  //

  if (!Wstr1ReadRes || !Wstr2ReadRes) {
    CompcovPrint("{}: Failed to read strings\n", __func__);
    return;
  }

  uint64_t LengthBytes =
      CompcovStrlen2((uint16_t *)Wstr1.data(), (uint16_t *)Wstr2.data(),
                     COMPCOV_MAX_CMP_LENGTH / sizeof(wchar_t)) *
      sizeof(wchar_t);

  //
  // Skip if the comparison is too long, as we don't want to clutter the
  // coverage database.
  //

  if (LengthBytes >= COMPCOV_MAX_CMP_LENGTH) {
    CompcovPrint("{}: LengthBytes >= COMPCOV_MAX_CMP_LENGTH\n", __func__);
    return;
  }

  //
  // As the breakpoint is set on the beginning of the function, we should be
  // able to extract the return address by reading the first QWORD from the
  // stack.
  //
  uint64_t RetLoc = Backend->VirtRead8(Gva_t(Backend->Rsp()));

  CompcovPrint("CompareStringW(..., {:#x}, \"{}\", {}, \"{}\", {}) -> {:#x}\n",
               DwCmpFlags, BytesToHexString(Wstr1.data(), String1LengthBytes),
               String1Length,
               BytesToHexString(Wstr2.data(), String2LengthBytes),
               String2Length, RetLoc);

  //
  // If the return location is 0, then the VirtRead8() failed.
  //

  if (RetLoc == 0) {
    CompcovPrint("{}: RetLoc == 0\n", __func__);
    return;
  }

  CompcovTrace(RetLoc, Wstr1.data(), Wstr2.data(), LengthBytes);
}

//
// Generic handler for memcmp-like functions.
//

void CompcovHandleMemcmp(Backend_t *Backend, Gva_t Buf1Ptr, Gva_t Buf2Ptr,
                         uint64_t Size) {
  //
  // Skip if the comparison is too long, as we don't want to clutter the
  // coverage database.
  //

  if (Size >= COMPCOV_MAX_CMP_LENGTH) {
    CompcovPrint("{}: Size >= COMPCOV_MAX_CMP_LENGTH\n", __func__);
    return;
  }

  //
  // Read the buffers.
  //

  std::array<uint8_t, COMPCOV_MAX_CMP_LENGTH> Buf1{};
  std::array<uint8_t, COMPCOV_MAX_CMP_LENGTH> Buf2{};

  bool Buf1ReadRes = Backend->VirtRead(Buf1Ptr, Buf1.data(), Size);
  bool Buf2ReadRes = Backend->VirtRead(Buf2Ptr, Buf2.data(), Size);

  //
  // Check whether we were able to read the buffers.
  //

  if (!Buf1ReadRes || !Buf2ReadRes) {
    CompcovPrint("{}: Failed to read buffers\n", __func__);
    return;
  }

  //
  // As the breakpoint is set on the beginning of the function, we should be
  // able to extract the return address by reading the first QWORD from the
  // stack.
  //

  uint64_t RetLoc = Backend->VirtRead8(Gva_t(Backend->Rsp()));

  CompcovPrint("Memcmp(\"{}\", \"{}\", {}) -> {:#x}\n",
               BytesToHexString(Buf1.data(), Size),
               BytesToHexString(Buf2.data(), Size), Size, RetLoc);

  //
  // If the return location is 0, then the VirtRead8() failed.
  //

  if (RetLoc == 0) {
    CompcovPrint("{}: RetLoc == 0\n", __func__);
    return;
  }

  CompcovTrace(RetLoc, Buf1.data(), Buf2.data(), Size);
}

//
// Memcmp hook.
//

void CompcovHookMemcmp(Backend_t *Backend) {
  //
  // Extract the arguments.
  //

  Gva_t Buf1Ptr = Gva_t(Backend->GetArg(0));
  Gva_t Buf2Ptr = Gva_t(Backend->GetArg(1));
  uint64_t Size = Backend->GetArg(2);

  CompcovHandleMemcmp(Backend, Buf1Ptr, Buf2Ptr, Size);
}

struct CompcovHook_t {
  const std::vector<std::string_view> FunctionNames;
  void (*HookFunction)(Backend_t *);
};

bool CompcovSetupHooks() {
  const std::vector<std::string_view> strcmp_functions = {"ntdll!strcmp",
                                                          "ucrtbase!strcmp"};
  const std::vector<std::string_view> strncmp_functions = {"ntdll!strncmp",
                                                           "ucrtbase!strncmp"};
  const std::vector<std::string_view> wcscmp_functions = {"ntdll!wcscmp",
                                                          "ucrtbase!wcscmp"};
  const std::vector<std::string_view> wcsncmp_functions = {"ntdll!wcsncmp",
                                                           "ucrtbase!wcsncmp"};
  const std::vector<std::string_view> memcmp_functions = {
      "ntdll!memcmp", "vcruntime140!memcmp", "ucrtbase!memcmp",
      // RtlCompareMemory() behaves like memcmp(), so we can reuse the same
      // hook.
      "ntdll!RtlCompareMemory"};

  const std::vector<CompcovHook_t> hooks = {
      {strcmp_functions, CompcovHookStrcmp},
      {strncmp_functions, CompcovHookStrncmp},
      {wcscmp_functions, CompcovHookWcscmp},
      {wcsncmp_functions, CompcovHookWcsncmp},
      {{"KernelBase!CompareStringA"}, CompcovHookCompareStringA},
      {{"KernelBase!CompareStringW"}, CompcovHookCompareStringW},
      // CompareStringEx is essentially the same as CompareStringW, so we can
      // reuse the same hook.
      {{"KernelBase!CompareStringEx"}, CompcovHookCompareStringW},
      {memcmp_functions, CompcovHookMemcmp},
  };

  bool Success = true;

  for (auto &hook : hooks) {
    for (auto &function : hook.FunctionNames) {
      CompcovPrint("Hooking comparison function {}\n", function);

      // @TODO: Currently we're "ignoring" the fact that SetBreakpoint can
      // fail (e.g. a breakpoint is already set on the function). Probably,
      // the best way to handle this is to replace already set breakpoint with
      // our own, but call the original BP-handler from it. Anyways, for now
      // it's not a problem, as we're using Bochs, which uses edge/non-bp
      // coverage.
      if (!g_Backend->SetBreakpoint(function.data(), hook.HookFunction)) {
        fmt::print("Failed to SetBreakpoint on {}\n", function);
        Success = false;
      }
    }
  }

  return Success;
}

//
// Setup compcov-strcmp hook for a custom implementation of strcmp.
//

bool CompcovSetupCustomStrcmpHook(const char *Symbol,
                                  const BreakpointHandler_t Handler) {
  const Gva_t Gva = Gva_t(g_Dbg.GetSymbol(Symbol));
  if (Gva == Gva_t(0)) {
    fmt::print(
        "Could not setup compcov strcmp hook for: {}, symbol not found!\n",
        Symbol);
    return false;
  }

  return CompcovSetupCustomStrcmpHook(Gva, Handler);
}

bool CompcovSetupCustomStrcmpHook(const Gva_t Gva,
                                  const BreakpointHandler_t Handler) {
  BochscpuBackend_t *BochsBackend =
      dynamic_cast<BochscpuBackend_t *>(g_Backend);
  if (!BochsBackend) {
    fmt::print("CompcovSetupCustomHook: Unsupported backend, only BochsCPU "
               "backend is supported\n");
    return false;
  }

  return g_Backend->SetBreakpoint(Gva, Handler);
}

//
// Setup compcov-strncmp hook for a custom implementation of strncmp.
//

bool CompcovSetupCustomStrncmpHook(const char *Symbol,
                                   const BreakpointHandler_t Handler) {
  const Gva_t Gva = Gva_t(g_Dbg.GetSymbol(Symbol));
  if (Gva == Gva_t(0)) {
    fmt::print(
        "Could not setup compcov strncmp hook for: {}, symbol not found!\n",
        Symbol);
    return false;
  }

  return CompcovSetupCustomStrncmpHook(Gva, Handler);
}

bool CompcovSetupCustomStrncmpHook(const Gva_t Gva,
                                   const BreakpointHandler_t Handler) {
  BochscpuBackend_t *BochsBackend =
      dynamic_cast<BochscpuBackend_t *>(g_Backend);
  if (!BochsBackend) {
    fmt::print("CompcovSetupCustomHook: Unsupported backend, only BochsCPU "
               "backend is supported\n");
    return false;
  }

  return g_Backend->SetBreakpoint(Gva, Handler);
}

//
// Setup compcov-wcscmp hook for a custom implementation of wcscmp.
//

bool CompcovSetupCustomWcscmpHook(const char *Symbol,
                                  const BreakpointHandler_t Handler) {
  const Gva_t Gva = Gva_t(g_Dbg.GetSymbol(Symbol));
  if (Gva == Gva_t(0)) {
    fmt::print(
        "Could not setup compcov wcscmp hook for: {}, symbol not found!\n",
        Symbol);
    return false;
  }

  return CompcovSetupCustomWcscmpHook(Gva, Handler);
}

bool CompcovSetupCustomWcscmpHook(const Gva_t Gva,
                                  const BreakpointHandler_t Handler) {
  BochscpuBackend_t *BochsBackend =
      dynamic_cast<BochscpuBackend_t *>(g_Backend);
  if (!BochsBackend) {
    fmt::print("CompcovSetupCustomHook: Unsupported backend, only BochsCPU "
               "backend is supported\n");
    return false;
  }

  return g_Backend->SetBreakpoint(Gva, Handler);
}

//
// Setup compcov-wcsncmp hook for a custom implementation of wcsncmp.
//

bool CompcovSetupCustomWcsncmpHook(const char *Symbol,
                                   const BreakpointHandler_t Handler) {
  const Gva_t Gva = Gva_t(g_Dbg.GetSymbol(Symbol));
  if (Gva == Gva_t(0)) {
    fmt::print(
        "Could not setup compcov wcsncmp hook for: {}, symbol not found!\n",
        Symbol);
    return false;
  }

  return CompcovSetupCustomWcsncmpHook(Gva, Handler);
}

bool CompcovSetupCustomWcsncmpHook(const Gva_t Gva,
                                   const BreakpointHandler_t Handler) {
  BochscpuBackend_t *BochsBackend =
      dynamic_cast<BochscpuBackend_t *>(g_Backend);
  if (!BochsBackend) {
    fmt::print("CompcovSetupCustomHook: Unsupported backend, only BochsCPU "
               "backend is supported\n");
    return false;
  }

  return g_Backend->SetBreakpoint(Gva, Handler);
}

//
// Setup compcov-memcmp hook for a custom implementation of memcmp.
//

bool CompcovSetupCustomMemcmpHook(const char *Symbol,
                                  const BreakpointHandler_t Handler) {
  const Gva_t Gva = Gva_t(g_Dbg.GetSymbol(Symbol));
  if (Gva == Gva_t(0)) {
    fmt::print(
        "Could not setup compcov memcmp hook for: {}, symbol not found!\n",
        Symbol);
    return false;
  }

  return CompcovSetupCustomMemcmpHook(Gva, Handler);
}

bool CompcovSetupCustomMemcmpHook(const Gva_t Gva,
                                  const BreakpointHandler_t Handler) {
  BochscpuBackend_t *BochsBackend =
      dynamic_cast<BochscpuBackend_t *>(g_Backend);
  if (!BochsBackend) {
    fmt::print("CompcovSetupCustomHook: Unsupported backend, only BochsCPU "
               "backend is supported\n");
    return false;
  }

  return g_Backend->SetBreakpoint(Gva, Handler);
}