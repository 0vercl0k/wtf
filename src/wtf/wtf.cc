// Axel '0vercl0k' Souchet - February 25 2020
#include "CLI/CLI.hpp"
#include "backend.h"
#include "bochscpu_backend.h"
#include "kvm_backend.h"
#include "platform.h"
#include "subcommands.h"
#include "utils.h"
#include "whv_backend.h"
#include <filesystem>
#include <fmt/format.h>
#include <random>

int main(int argc, const char *argv[]) {
  //
  // Set up the arguments.
  //

  Options_t Opts;

  CLI::App Wtf("what the fuzz: a distributed, code-coverage guided, "
               "customizable,\ncross-platform snapshot-based fuzzer by Axel "
               "'0vercl0k' Souchet.\n");

  Wtf.require_subcommand(1);
  Wtf.allow_windows_style_options();
  Wtf.set_help_all_flag("--help-all", "Expand all help");

  Wtf.add_option("-v,--verbose", Opts.Verbose, "Turn on verbose mode");

  CLI::App *MasterCmd =
      Wtf.add_subcommand("master", "Master options")->callback([&Opts] {
        //
        // Populate other paths based on the based target path.. unless the user
        // has overriden them.
        //

        if (Opts.Master.InputsPath.empty()) {
          Opts.Master.InputsPath = Opts.Master.TargetPath / "inputs";
        }

        if (Opts.Master.OutputsPath.empty()) {
          Opts.Master.OutputsPath = Opts.Master.TargetPath / "outputs";
        };

        if (Opts.Master.CrashesPath.empty()) {
          Opts.Master.CrashesPath = Opts.Master.TargetPath / "crashes";
        }

        if (!std::filesystem::exists(Opts.Master.InputsPath) ||
            !std::filesystem::exists(Opts.Master.OutputsPath) ||
            !std::filesystem::exists(Opts.Master.CrashesPath)) {
          throw CLI::ParseError(
              fmt::format("Expected to find inputs/outputs/crashes directories "
                          "in '{}'.",
                          Opts.Master.TargetPath.string()),
              EXIT_FAILURE);
        }

        if (Opts.Master.Seed == 0) {
          std::random_device R;
          Opts.Master.Seed = (uint64_t(R()) << 32) | R();
        }
      });

  MasterCmd
      ->add_option("--address", Opts.Master.Address,
                   "Which address to listen in")
      ->default_val("tcp://localhost:31337");

  MasterCmd->add_option("--runs", Opts.Master.Runs, "Runs")
      ->description("Number of mutations done.")
      ->required();

  MasterCmd
      ->add_option("--max_len", Opts.Master.TestcaseBufferMaxSize,
                   "Testcase size")
      ->description("Maximum size of a generated testcase.")
      ->required();

  MasterCmd->add_option("--name", Opts.TargetName, "Target name")
      ->description("Name of the target fuzzer.")
      ->required();

  MasterCmd->add_option("--target", Opts.Master.TargetPath, "Target path")
      ->description("Target directory")
      ->required();

  MasterCmd->add_option("--inputs", Opts.Master.InputsPath, "Inputs")
      ->description("Input corpus");

  MasterCmd->add_option("--outputs", Opts.Master.OutputsPath, "Outputs")
      ->description("Outputs path");

  MasterCmd->add_option("--crashes", Opts.Master.CrashesPath, "Crashes")
      ->description("Crashes path");

  MasterCmd
      ->add_option("--seed", Opts.Master.Seed, "Specify a seed for the RNG")
      ->description("Override the seed used to initialize RNG.");

  CLI::App *RunCmd =
      Wtf.add_subcommand("run", "Run and trace options")->callback([&Opts] {
        //
        // Populate other paths based on the based state path.
        //

        Opts.DumpPath = Opts.StatePath / "mem.dmp";
        Opts.CpuStatePath = Opts.StatePath / "regs.json";
        Opts.SymbolFilePath = Opts.StatePath / "symbol-store.json";

        if (Opts.GuestFilesPath.empty()) {
          Opts.GuestFilesPath = Opts.StatePath.parent_path() / "guest-files";
        }

        if (Opts.CoveragePath.empty()) {
          Opts.CoveragePath = Opts.StatePath.parent_path() / "coverage";
        }

        //
        // If a trace type was specified but no path, then defaults it
        // to the cwd.
        //

        if (Opts.Run.TraceType != TraceType_t::NoTrace &&
            Opts.Run.BaseTracePath.empty()) {
          Opts.Run.BaseTracePath = fs::current_path();
        }

        //
        // Ensure that they exist just as a quick check.
        //

        if (!std::filesystem::exists(Opts.DumpPath) ||
            !std::filesystem::exists(Opts.CpuStatePath)) {
          throw CLI::ParseError(fmt::format("Expected to find state/mem.dmp, "
                                            "state/regs.json files in '{}'.",
                                            Opts.StatePath.string()),
                                EXIT_FAILURE);
        }

        //
        // Ensure that if the 'edge' mode is turned on, bxcpu is used as the
        // backend.
        //

        if (Opts.Edges && Opts.Backend != BackendType_t::Bochscpu) {
          throw CLI::ParseError(
              "Edge coverage is only available with the bxcpu backend.",
              EXIT_FAILURE);
        }

#ifdef LINUX
        if (!std::filesystem::exists(Opts.SymbolFilePath)) {
          throw CLI::ParseError(
              fmt::format("Expected to find a state/symbol-store.json file in "
                          "'{}'. You need to generate it from Windows.",
                          Opts.Fuzz.TargetPath.string()),
              EXIT_FAILURE);
        }

        if (Opts.Run.TraceType == TraceType_t::Rip &&
            Opts.Backend != BackendType_t::Bochscpu) {
          throw CLI::ParseError("Only the bochscpu backend can be used to "
                                "generate rip traces on Linux.",
                                EXIT_FAILURE);
        }
#endif
      });

  CLI::Option_group *TraceOpt = RunCmd->add_option_group(
      "trace", "Describe the type of trace and where to store it");

  TraceOpt
      ->add_option("--trace-path", Opts.Run.BaseTracePath,
                   "Base folder where to output traces")
      ->check(CLI::ExistingDirectory);

  const std::unordered_map<std::string, TraceType_t> TraceTypeMap = {
      {"rip", TraceType_t::Rip},
      {"cov", TraceType_t::UniqueRip},
      {"tenet", TraceType_t::Tenet}};

  TraceOpt->add_option("--trace-type", Opts.Run.TraceType, "Trace type")
      ->transform(CLI::CheckedTransformer(TraceTypeMap, CLI::ignore_case))
      ->description("Type of trace to generate.");

  TraceOpt->require_option(0, 2);

  const std::unordered_map<std::string, BackendType_t> BackendTypeMap = {
      {"bochscpu", BackendType_t::Bochscpu},
      {"bxcpu", BackendType_t::Bochscpu},
#ifdef WINDOWS
      //
      // We disable whv on Linux for obvious reasons.
      //

      {"whv", BackendType_t::Whv}
#endif
#ifdef LINUX
      //
      // KVM supports is only available on Linux.
      //

      {"kvm", BackendType_t::Kvm}
#endif
  };

  RunCmd->add_option("--name", Opts.TargetName, "Target name")
      ->description("Name of the target fuzzer.")
      ->required();

  RunCmd->add_option("--backend", Opts.Backend, "Execution backend")
      ->transform(CLI::CheckedTransformer(BackendTypeMap, CLI::ignore_case))
      ->description("Execution backend.")
      ->required();

  RunCmd->add_option("--state", Opts.StatePath, "State directory")
      ->check(CLI::ExistingDirectory)
      ->description("State directory which contains memory and cpu state.")
      ->required();

  RunCmd
      ->add_option("--guest-files", Opts.GuestFilesPath,
                   "Guest files directory")
      ->check(CLI::ExistingDirectory)
      ->description("Directory where all the guest files are stored in.");

  RunCmd->add_option("--input", Opts.Run.InputPath, "Input file / folder")
      ->check(CLI::ExistingFile | CLI::ExistingDirectory)
      ->description("Input file or input folders to run.")
      ->required();

  RunCmd->add_option("--limit", Opts.Limit, "Limit")
      ->description("Limit per testcase (instruction count for bochscpu, time "
                    "in second for whv).");

  RunCmd->add_option("--coverage", Opts.CoveragePath, "Coverage files")
      ->check(CLI::ExistingDirectory)
      ->description("Directory where all the coverage files are stored in.");

  RunCmd->add_flag("--edges", Opts.Edges, "Edge coverage")
      ->default_val(false)
      ->description("Turn on edge coverage (bxcpu only).");

  RunCmd->add_option("--runs", Opts.Run.Runs, "Runs")
      ->description("Number of mutations done.")
      ->default_val(1);

  CLI::App *FuzzCmd =
      Wtf.add_subcommand("fuzz", "Fuzzing options")->callback([&Opts] {
        //
        // Populate other paths based on the based target path.. unless the user
        // has overriden them.
        // One use-case for this for example, is to be able to launch two
        // instances fuzzing the same target but using two different dumps;
        // let's say one with PageHeap and one without. One can override every
        // option to customize which paths to use.
        //

        if (Opts.GuestFilesPath.empty()) {
          Opts.GuestFilesPath = Opts.Fuzz.TargetPath / "guest-files";
        }

        if (Opts.StatePath.empty()) {
          Opts.StatePath = Opts.Fuzz.TargetPath / "state";
        }

        if (Opts.CoveragePath.empty()) {
          Opts.CoveragePath = Opts.Fuzz.TargetPath / "coverage";
        }

        Opts.DumpPath = Opts.StatePath / "mem.dmp";
        Opts.CpuStatePath = Opts.StatePath / "regs.json";
        Opts.SymbolFilePath = Opts.StatePath / "symbol-store.json";

        //
        // Ensure that they exist just as a quick check.
        //

        if (!std::filesystem::exists(Opts.DumpPath) ||
            !std::filesystem::exists(Opts.CpuStatePath)) {
          throw CLI::ParseError(
              fmt::format("Expected to find a state/mem.dmp file, a "
                          "state/regs.json file, an inputs directory, an "
                          "outputs directory, a crashes directory in '{}'.",
                          Opts.Fuzz.TargetPath.string()),
              EXIT_FAILURE);
        }

        //
        // Ensure that if the 'edge' mode is turned on, bxcpu is used as the
        // backend.
        //

        if (Opts.Edges && Opts.Backend != BackendType_t::Bochscpu) {
          throw CLI::ParseError(
              "Edge coverage is only available with the bxcpu backend.",
              EXIT_FAILURE);
        }

        if (Opts.Fuzz.Seed == 0) {
          std::random_device R;
          Opts.Fuzz.Seed = (uint64_t(R()) << 32) | R();
        }

#ifdef LINUX
        if (!std::filesystem::exists(Opts.SymbolFilePath)) {
          throw CLI::ParseError(
              fmt::format("Expected to find a state/symbol-store.json file in "
                          "'{}'; you need to generate it from Windows.",
                          Opts.Fuzz.TargetPath.string()),
              EXIT_FAILURE);
        }
#endif
      });

  FuzzCmd->add_option("--backend", Opts.Backend, "Execution backend")
      ->transform(CLI::CheckedTransformer(BackendTypeMap, CLI::ignore_case))
      ->description("Execution backend.")
      ->required();

  FuzzCmd->add_flag("--edges", Opts.Edges, "Edge coverage")
      ->default_val(false)
      ->description("Turn on edge coverage (bxcpu only).");

  FuzzCmd->add_option("--name", Opts.TargetName, "Target name")
      ->description("Name of the target fuzzer.")
      ->required();

  FuzzCmd->add_option("--target", Opts.Fuzz.TargetPath, "Target directory")
      ->description(
          "Target directory which contains state/ inputs/ outputs/ folders.");

  FuzzCmd->add_option("--limit", Opts.Limit, "Limit")
      ->description("Limit per testcase (instruction count for bochscpu, time "
                    "in second for whv).");

  FuzzCmd->add_option("--state", Opts.StatePath, "State directory")
      ->check(CLI::ExistingDirectory)
      ->description("State directory which contains memory and cpu state.");

  FuzzCmd
      ->add_option("--guest-files", Opts.GuestFilesPath,
                   "Guest files directory")
      ->check(CLI::ExistingDirectory)
      ->description("Directory where all the guest files are stored in.");

  FuzzCmd->add_option("--seed", Opts.Fuzz.Seed, "Specify a seed for the RNGs")
      ->description("Override the seed used to initialize RNGs.");

  FuzzCmd
      ->add_option("--address", Opts.Fuzz.Address,
                   "Specify what address to connect to the master node")
      ->default_val("tcp://localhost:31337/")
      ->description("Connect to the master node.");

  CLI11_PARSE(Wtf, argc, argv);

  //
  // Check if the user has the right target before doing any heavy lifting.
  //

  Targets_t &Targets = Targets_t::Instance();
  const Target_t *Target = Targets.Get(Opts.TargetName);
  if (Target == nullptr) {
    Targets.DisplayRegisteredTargets();
    return EXIT_FAILURE;
  }

  //
  // If we are in master mode, no need to initialize the heavy machinery.
  //

  if (Wtf.got_subcommand("master")) {
    return MasterSubcommand(Opts, *Target);
  }

  //
  // Populate the state from the file.
  //

  CpuState_t CpuState;
  if (!LoadCpuStateFromJSON(CpuState, Opts.CpuStatePath)) {
    fmt::print("LoadCpuStateFromJSON failed, no take off today.\n");
    return EXIT_FAILURE;
  }

#ifdef WINDOWS
  if (Opts.Backend == BackendType_t::Whv) {
    g_Backend = new WhvBackend_t();
  }
#endif
#ifdef LINUX
  if (Opts.Backend == BackendType_t::Kvm) {
    g_Backend = new KvmBackend_t();
  }
#endif
  if (Opts.Backend == BackendType_t::Bochscpu) {
    g_Backend = new BochscpuBackend_t();
  }

  //
  // Initialize the debugger instance.
  //

  if (!g_Dbg.Init(Opts.DumpPath, Opts.SymbolFilePath)) {
    return EXIT_FAILURE;
  }

  //
  // Set an instruction limit to avoid infinite loops, etc.
  //

  if (Opts.Limit != 0) {
    g_Backend->SetLimit(Opts.Limit);
  }

  //
  // Initialize the backend with a state. This ensures the backend is ready to
  // service memory / register access, etc.
  //
  // Because SanitizeCpuState needs to read virtual memory, the backend has to
  // start from somewhere. We first flush the state as is and this should be
  // enough to have SanitizeCpuState do its job.
  //

  if (!g_Backend->Initialize(Opts, CpuState)) {
    fmt::print("Backend failed initialization.\n");
    return EXIT_FAILURE;
  }

  //
  // Sanitize the state before running.
  //

  if (!SanitizeCpuState(CpuState)) {
    fmt::print("SanitizeCpuState failed, no take off today.\n");
    return EXIT_FAILURE;
  }

  //
  // We now have the real starting state we want to start with, so we make sure
  // it gets set in the backend and to do that we call the Restore function.
  // This ensures we start from a clean state.
  //

  if (!g_Backend->Restore(CpuState)) {
    fmt::print("Backend failed to restore.\n");
    return EXIT_FAILURE;
  }

  //
  // Now invoke the fuzz command if this is what we want.
  //

  if (Wtf.got_subcommand("fuzz")) {
    return FuzzSubcommand(Opts, *Target, CpuState);
  }

  //
  // Or the run command.
  //

  if (Wtf.got_subcommand("run")) {
    return RunSubcommand(Opts, *Target, CpuState);
  }

  return EXIT_FAILURE;
}
