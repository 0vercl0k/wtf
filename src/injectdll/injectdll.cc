// Axel '0vercl0k' Souchet - March 14 2020
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <tlhelp32.h>
#include <vector>

bool InjectDll(const uint32_t ProcessId, const std::filesystem::path &Path) {
  const uint32_t ProcessRights =
      PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
      PROCESS_VM_WRITE | PROCESS_VM_READ;
  const HANDLE Process = OpenProcess(ProcessRights, false, ProcessId);

  if (Process == nullptr) {
    return EXIT_FAILURE;
  }

  const PVOID RemoteDllPath = VirtualAllocEx(
      Process, nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

  if (RemoteDllPath == nullptr) {
    printf("VirtualAllocEx failed.\n");
    return EXIT_FAILURE;
  }

  const std::string DllPath = Path.string();
  const size_t DllPathLen = DllPath.size() + 1;
  SIZE_T BytesWritten;
  if (!WriteProcessMemory(Process, RemoteDllPath, DllPath.c_str(), DllPathLen,
                          &BytesWritten)) {
    printf("WriteProcessMemory failed.\n");
    return EXIT_FAILURE;
  }

  const HMODULE Kernelbase = GetModuleHandleA("kernelbase");
  if (Kernelbase == nullptr) {
    printf("GetModuleHandleA failed.\n");
    return EXIT_FAILURE;
  }

  const PVOID LoadLibraryA = PVOID(GetProcAddress(Kernelbase, "LoadLibraryA"));
  if (LoadLibraryA == nullptr) {
    printf("GetProcAddress failed.\n");
    return EXIT_FAILURE;
  }

  DWORD Tid;
  const HANDLE Thread = CreateRemoteThread(Process, nullptr, 0,
                                           LPTHREAD_START_ROUTINE(LoadLibraryA),
                                           RemoteDllPath, 0, &Tid);

  if (Thread == NULL) {
    printf("CreateRemoteThread failed.\n");
    return EXIT_FAILURE;
  }

  WaitForSingleObject(Thread, INFINITE);

  DWORD ExitCode = 0;
  GetExitCodeThread(Thread, &ExitCode);

  if (ExitCode == 0) {
    printf("/!\\ The thread failed to load the dll.\n");
  }

  CloseHandle(Thread);
  VirtualFreeEx(Process, RemoteDllPath, 0, MEM_RELEASE);
  CloseHandle(Process);
  return ExitCode != 0;
}

bool Pid2Name(const char *ProcessName, uint32_t &Pid) {
  PROCESSENTRY32 Pe32;
  HANDLE Snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (Snap == INVALID_HANDLE_VALUE) {
    return false;
  }

  Pe32.dwSize = sizeof(PROCESSENTRY32);
  if (!Process32First(Snap, &Pe32)) {
    CloseHandle(Snap);
    return false;
  }

  bool FoundPid = false;
  do {
    const bool Match = _stricmp(Pe32.szExeFile, ProcessName) == 0;
    if (Match) {
      if (FoundPid) {
        printf("There are several instances of %s, pid %d will be used.\n",
               Pe32.szExeFile, Pid);
      } else {
        FoundPid = true;
        Pid = Pe32.th32ProcessID;
      }
    }
  } while (Process32Next(Snap, &Pe32));

  CloseHandle(Snap);
  return FoundPid;
}

int main(int Argc, const char *Argv[]) {
  if (Argc != 3) {
    printf("./injectdll <pid | process name> <dll path | dll dir path>\n");
    return EXIT_FAILURE;
  }

  uint32_t ProcessId = strtol(Argv[1], nullptr, 0);
  if (ProcessId == 0) {
    const bool Success = Pid2Name(Argv[1], ProcessId);
    if (!Success) {
      printf("Pid2Name failed, exiting.\n");
      return EXIT_FAILURE;
    }
  }

  std::vector<std::filesystem::path> Dlls;
  if (std::filesystem::is_directory(Argv[2])) {
    const std::filesystem::directory_iterator DirIt(Argv[2]);
    for (const auto &DirEntry : DirIt) {
      if (DirEntry.path().extension().string() == ".dll") {
        Dlls.emplace_back(DirEntry);
      }
    }
  } else {
    Dlls.emplace_back(Argv[2]);
  }

  for (const std::filesystem::path &Dll : Dlls) {
    const std::filesystem::path DllAbsolute = std::filesystem::absolute(Dll);
    const bool Succeed = InjectDll(ProcessId, DllAbsolute);
    if (!Succeed) {
      printf("Error while injecting %ls in %d\n", DllAbsolute.c_str(),
             ProcessId);
      return EXIT_FAILURE;
    }

    printf("Successfully injected %ls in %d\n", DllAbsolute.c_str(), ProcessId);
  }

  printf("Done!\n");
  return EXIT_SUCCESS;
}