// Axel '0vercl0k' Souchet - June 7 2020
#include "debugger.h"

DebuggerLess_t g_NoDbg;

#ifdef WINDOWS
WindowsDebugger_t WindowsDebugger;
Debugger_t *g_Dbg = &WindowsDebugger;
#else
Debugger_t *g_Dbg = &g_NoDbg;
#endif