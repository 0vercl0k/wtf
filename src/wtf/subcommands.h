// Axel '0vercl0k' Souchet - April 5 2020
#pragma once
#include "globals.h"
#include "targets.h"

//
// Handles the 'master' subcommand.
//

int MasterSubcommand(const Options_t &Opts, const Target_t &Target);

//
// Handles the 'run' subcommand.
//

int RunSubcommand(const Options_t &Opts, const Target_t &Target,
                  const CpuState_t &CpuState);

//
// Handles the 'fuzz' subcommand.
//

int FuzzSubcommand(const Options_t &Opts, const Target_t &Target,
                   const CpuState_t &CpuState);
