// Theodor Arsenij 'm4drat' - May 26 2023

#pragma once

//
// Compcov maximum comparison length. Everything above this length will be
// ignored.
//

constexpr uint64_t COMPCOV_MAX_CMP_LENGTH = 34;

//
// Setup compcov hooks on different implementations of comparison functions:
// ntdll!strcmp, ucrtbase!strcmp, etc.
//

bool CompcovSetupHooks();

//
// Generic compcov handlers for different comparison functions. They might be
// useful if you want to add support for a custom comparison function, even if
// it uses a different calling convention. Just wrap the handler into a
// BreakpointHandler_t and use SetupCustom*Hook() functions.
//

void CompcovHandleStrcmp(Backend_t *Backend, Gva_t Str1Ptr, Gva_t Str2Ptr);
void CompcovHandleStrncmp(Backend_t *Backend, Gva_t Str1Ptr, Gva_t Str2Ptr,
                          uint64_t MaxCount);
void CompcovHandleWcscmp(Backend_t *Backend, Gva_t Wstr1Ptr, Gva_t Wstr2Ptr);
void CompcovHandleWcsncmp(Backend_t *Backend, Gva_t Wstr1Ptr, Gva_t Wstr2Ptr,
                          uint64_t MaxCount);
void CompcovHandleMemcmp(Backend_t *Backend, Gva_t Buf1Ptr, Gva_t Buf2Ptr,
                         uint64_t Size);

//
// Setup compcov-strcmp hook for a custom implementation of strcmp.
//

void CompcovHookStrcmp(Backend_t *Backend);

bool CompcovSetupCustomStrcmpHook(
    const char *Symbol, const BreakpointHandler_t Handler = CompcovHookStrcmp);
bool CompcovSetupCustomStrcmpHook(
    const Gva_t Gva, const BreakpointHandler_t Handler = CompcovHookStrcmp);

//
// Setup compcov-strncmp hook for a custom implementation of strncmp.
//

void CompcovHookStrncmp(Backend_t *Backend);

bool CompcovSetupCustomStrncmpHook(
    const char *Symbol, const BreakpointHandler_t Handler = CompcovHookStrncmp);
bool CompcovSetupCustomStrncmpHook(
    const Gva_t Gva, const BreakpointHandler_t Handler = CompcovHookStrncmp);

//
// Setup compcov-wcscmp hook for a custom implementation of wcscmp.
//

void CompcovHookWcscmp(Backend_t *Backend);

bool CompcovSetupCustomWcscmpHook(
    const char *Symbol, const BreakpointHandler_t Handler = CompcovHookWcscmp);
bool CompcovSetupCustomWcscmpHook(
    const Gva_t Gva, const BreakpointHandler_t Handler = CompcovHookWcscmp);

//
// Setup compcov-wcsncmp hook for a custom implementation of wcsncmp.
//

void CompcovHookWcsncmp(Backend_t *Backend);

bool CompcovSetupCustomWcsncmpHook(
    const char *Symbol, const BreakpointHandler_t Handler = CompcovHookWcsncmp);
bool CompcovSetupCustomWcsncmpHook(
    const Gva_t Gva, const BreakpointHandler_t Handler = CompcovHookWcsncmp);

//
// Setup compcov-memcmp hook for a custom implementation of memcmp.
//

void CompcovHookMemcmp(Backend_t *Backend);

bool CompcovSetupCustomMemcmpHook(
    const char *Symbol, const BreakpointHandler_t Handler = CompcovHookMemcmp);
bool CompcovSetupCustomMemcmpHook(
    const Gva_t Gva, const BreakpointHandler_t Handler = CompcovHookMemcmp);
