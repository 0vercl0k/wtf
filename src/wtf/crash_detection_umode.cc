// Axel '0vercl0k' Souchet - August 01 2020
#include "crash_detection_umode.h"
#include "backend.h"
#include "debugger.h"
#include "globals.h"
#include "nt.h"
#include "utils.h"
#include <fmt/format.h>

constexpr bool UCrashDetectionLoggingOn = true;

template <typename... Args_t>
void CrashDetectionPrint(const char *Format, const Args_t &...args) {
  if constexpr (UCrashDetectionLoggingOn) {
    fmt::print("ucrash: ");
    fmt::print(Format, args...);
  }
}

bool SetupUsermodeCrashDetectionHooks() {

  //
  // This is to catch the PMI interrupt if performance counters are used to
  // bound execution.
  //

  if (!g_Backend->SetBreakpoint("hal!HalpPerfInterrupt",
                                [](Backend_t *Backend) {
                                  CrashDetectionPrint("Perf interrupt\n");
                                  Backend->Stop(Timedout_t());
                                })) {
    fmt::print(
        "Failed to set breakpoint on HalpPerfInterrupt, but ignoring..\n");
  }

  //
  // Avoid the fuzzer to spin out of control if we mess-up real bad.
  //

  if (!g_Backend->SetCrashBreakpoint("nt!KeBugCheck2")) {
    fmt::print("Failed to SetBreakpoint on KeBugCheck2\n");
    return false;
  }

  if (!g_Backend->SetBreakpoint("nt!SwapContext", [](Backend_t *Backend) {
        CrashDetectionPrint("nt!SwapContext\n");
        Backend->Stop(Cr3Change_t());
      })) {
    fmt::print("Failed to SetBreakpoint on SwapContext\n");
    return false;
  }

  if (!g_Backend->SetBreakpoint(
          "ntdll!RtlDispatchException", [](Backend_t *Backend) {
            // BOOLEAN
            // NTAPI
            // RtlDispatchException(
            //    _In_ PEXCEPTION_RECORD ExceptionRecord,
            //    _In_ PCONTEXT Context)
            const Gva_t ExceptionRecordPtr = Backend->GetArgGva(0);
            wtf::EXCEPTION_RECORD ExceptionRecord;
            if (!Backend->VirtReadStruct(ExceptionRecordPtr,
                                         &ExceptionRecord)) {
              std::abort();
            }

            //
            // Let's ignore the less interesting stuff; DbgPrint, C++
            // exceptions, etc.
            //

            if (ExceptionRecord.ExceptionCode == 0xE06D7363 ||
                ExceptionRecord.ExceptionCode == DBG_PRINTEXCEPTION_C ||
                ExceptionRecord.ExceptionCode == DBG_PRINTEXCEPTION_WIDE_C) {

              //
              // https://support.microsoft.com/fr-fr/help/185294/prb-exception-code-0xe06d7363-when-calling-win32-seh-apis
              // https://blogs.msdn.microsoft.com/oldnewthing/20100730-00/?p=13273
              //

              return;
            }

            const Gva_t ExceptionAddress =
                Gva_t(ExceptionRecord.ExceptionAddress);
            const uint64_t ExceptionInformation =
                ExceptionRecord.ExceptionInformation[0];
            uint32_t ExceptionCode = ExceptionRecord.ExceptionCode;
            if (ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {

              //
              // The first element of the array contains a read write flag that
              // indicates the type of operation that caused the access
              // violation. If this value is zero, the thread attempted to read
              // the inaccessible data. If this value is 1, the thread attempted
              // to write to an inaccessible address. If this value is 8, the
              // thread causes a user - mode data execution prevention(DEP)
              // violation.
              //

              switch (ExceptionInformation) {
              case 0: {
                ExceptionCode = EXCEPTION_ACCESS_VIOLATION_READ;
                break;
              }
              case 1: {
                ExceptionCode = EXCEPTION_ACCESS_VIOLATION_WRITE;
                break;
              }
              case 8: {
                ExceptionCode = EXCEPTION_ACCESS_VIOLATION_EXECUTE;
                break;
              }
              default: {
                break;
              }
              }
            }

            CrashDetectionPrint("RtlDispatchException triggered {} @ {:#x}\n",
                                ExceptionCodeToStr(ExceptionCode),
                                ExceptionAddress);
            Backend->SaveCrash(ExceptionAddress, ExceptionCode);
          })) {
    fmt::print("Failed to SetBreakpoint on RtlDispatchException\n");
    return false;
  }

  //
  // As we can't set-up the exception bitmap so that we receive a vmexit on
  // failfast exceptions, we instead set a breakpoint to the function handling
  // the interruption.
  //
  // kd> !idt 0x29
  // Dumping IDT: fffff8053f15b000
  // 29:	fffff8053b9ccb80 nt!KiRaiseSecurityCheckFailure
  //

  if (!g_Backend->SetBreakpoint(
          "nt!KiRaiseSecurityCheckFailure", [](Backend_t *Backend) {
            const Gva_t Rsp = Gva_t(Backend->Rsp());
            const Gva_t ExceptionAddress = Backend->VirtReadGva(Rsp);
            CrashDetectionPrint(
                "KiRaiseSecurityCheckFailure triggered @ {:#x}\n",
                ExceptionAddress);
            Backend->SaveCrash(ExceptionAddress, STATUS_STACK_BUFFER_OVERRUN);
          })) {
    fmt::print("Failed to SetBreakpoint on KiRaiseSecurityCheckFailure\n");
    return false;
  }

  if (g_Dbg.GetModuleBase("verifier") > 0) {
    if (!g_Backend->SetBreakpoint(
            "verifier!VerifierStopMessage", [](Backend_t *Backend) {
              const uint64_t Unique = Backend->Rsp();
              CrashDetectionPrint("VerifierStopMessage @ {:#x}!\n", Unique);
              Backend->SaveCrash(Gva_t(Unique), STATUS_HEAP_CORRUPTION);
            })) {
      fmt::print("Failed to SetBreakpoint on VerifierStopMessage\n");
      return false;
    }
  }

  return true;
}