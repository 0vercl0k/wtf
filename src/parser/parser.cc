// Axel '0vercl0k' Souchet - February 15 2019
#include "kdmp-parser.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string_view>
#include <vector>

//
// Delimiter.
//

#define DELIMITER                                                              \
  "----------------------------------------------------------------------"     \
  "----------"

//
// The options available for the parser.
//

struct Options_t {

  //
  // This is enabled if -h is used.
  //

  bool ShowHelp = false;

  //
  // This is enabled if -c is used.
  //

  bool ShowContextRecord = false;

  //
  // This is enabled if -a is used.
  //

  bool ShowAllStructures = false;

  //
  // This is enabled if -e is used.
  //

  bool ShowExceptionRecord = false;

  //
  // This is enable if -p is used.
  //

  bool ShowPhysicalMem = false;

  //
  // This is on if the user specified a physical address.
  //

  bool HasPhysicalAddress = false;

  //
  // If an optional physical address has been passed to -p then this is the
  // physical address.

  uint64_t PhysicalAddress = 0;

  //
  // The path to the dump file.
  //

  std::string_view DumpPath;
};

//
// Display usage
//

void Help() {
  printf("parser.exe [-p [<physical address>]] [-c] [-e] [-h] <kdump path>\n");
  printf("\n");
  printf("Examples:\n");
  printf("  Show every structures of the dump:\n");
  printf("    parser.exe -a full.dmp\n");
  printf("\n");
  printf("  Show the context record:\n");
  printf("    parser.exe -c full.dmp\n");
  printf("\n");
  printf("  Show the exception record:\n");
  printf("    parser.exe -e full.dmp\n");
  printf("\n");
  printf("  Show all the physical memory (first 16 bytes of every "
         "pages):\n");
  printf("    parser.exe -p full.dmp\n");
  printf("\n");
  printf("  Show the context record as well as the page at physical "
         "address 0x1000:\n");
  printf("    parser.exe -c -p 0x1000 full.dmp\n");
}

//
// Copied from https://github.com/pvachon/tsl/blob/master/tsl/hexdump.c.
// Phil is the man.
//

void Hexdump(const uint64_t Address, const void *Buffer, size_t Len) {
  const uint8_t *ptr = (uint8_t *)Buffer;

  for (size_t i = 0; i < Len; i += 16) {
    printf("%08" PRIx64 ": ", Address + i);
    for (int j = 0; j < 16; j++) {
      if (i + j < Len) {
        printf("%02x ", ptr[i + j]);
      } else {
        printf("   ");
      }
    }
    printf(" |");
    for (int j = 0; j < 16; j++) {
      if (i + j < Len) {
        printf("%c", isprint(ptr[i + j]) ? (char)ptr[i + j] : '.');
      } else {
        printf(" ");
      }
    }
    printf("|\n");
  }
}

//
// Let's do some work!
//

int main(int argc, const char *argv[]) {

  //
  // This holds the options passed to the program.
  //

  Options_t Opts;

  //
  // Parse the arguments passed to the program.
  //

  for (int ArgIdx = 1; ArgIdx < argc; ArgIdx++) {
    const std::string_view Arg(argv[ArgIdx]);
    const int IsLastArg = (ArgIdx + 1) >= argc;

    if (Arg == "-c") {

      //
      // Show the context record.
      //

      Opts.ShowContextRecord = 1;
    } else if (Arg == "-p") {

      //
      // Show the physical memory.
      //

      Opts.ShowPhysicalMem = 1;

      //
      // If the next argument is not the last one, we assume that it is followed
      // by a physical address.
      //

      const int NextArgIdx = ArgIdx + 1;
      const bool IsNextArgLast = (NextArgIdx + 1) >= argc;

      if (!IsNextArgLast) {

        //
        // In which case we convert it to an actual integer.
        //

        Opts.HasPhysicalAddress = true;
        Opts.PhysicalAddress = strtoull(argv[NextArgIdx], nullptr, 0);

        //
        // Skip the next argument.
        //

        ArgIdx++;
      }
    } else if (Arg == "-e") {

      //
      // Show the exception record.
      //

      Opts.ShowExceptionRecord = 1;
    } else if (Arg == "-a") {

      //
      // Show all the structures.
      //

      Opts.ShowAllStructures = true;
    } else if (Arg == "-h") {

      //
      // Show the help.
      //

      Opts.ShowHelp = true;
    } else if (IsLastArg) {

      //
      // If this is the last argument then this must be the dump path.
      //

      Opts.DumpPath = Arg;
    } else {

      //
      // Otherwise it seems that the user passed something wrong?
      //

      printf("The argument %s is not recognized.\n\n", Arg.data());
      Help();
      return EXIT_FAILURE;
    }
  }

  //
  // Show the help.
  //

  if (Opts.ShowHelp) {
    Help();
    return EXIT_SUCCESS;
  }

  //
  // The only thing we actually need is a file path. So let's make sure we
  // have one.
  //

  if (Opts.DumpPath.empty()) {
    printf("You didn't provide the path to the dump file.\n\n");
    Help();
    return EXIT_FAILURE;
  }

  //
  // If we only have a path, at least force to dump the context
  // structure.
  //

  if (!Opts.ShowContextRecord && !Opts.ShowPhysicalMem &&
      !Opts.ShowAllStructures && !Opts.ShowExceptionRecord) {
    printf("Forcing to show the context record as no option as been "
           "passed.\n\n");
    Opts.ShowContextRecord = 1;
  }

  //
  // Create the parser instance.
  //

  kdmpparser::KernelDumpParser Dmp;

  //
  // Parse the dump file.
  //

  if (!Dmp.Parse(Opts.DumpPath.data())) {
    printf("Parsing of the dump failed, exiting.\n");
    return EXIT_FAILURE;
  }

  //
  // If the user wants all the structures, then show them.
  //

  if (Opts.ShowAllStructures) {
    printf(DELIMITER "\nDump structures:\n");
    Dmp.ShowAllStructures(2);
  }

  //
  // If the user wants the context, then show it.
  //

  if (Opts.ShowContextRecord) {
    printf(DELIMITER "\nContext Record:\n");
    Dmp.ShowContextRecord(2);
  }

  //
  // If the user wants the exception record, then show it.
  //

  if (Opts.ShowExceptionRecord) {
    printf(DELIMITER "\nException Record:\n");
    Dmp.ShowExceptionRecord(2);
  }

  //
  // If the user wants some physical memory, then show it.
  //

  if (Opts.ShowPhysicalMem) {
    printf(DELIMITER "\nPhysical memory:\n");

    //
    // If the user specified a physical address this is the one we
    // will dump.
    //

    if (Opts.PhysicalAddress) {

      //
      // Retrieve the page for the specified PhysicalAddress.
      // If it doesn't exist then display a message, else dump it on stdout.
      //

      const uint8_t *Page = Dmp.GetPhysicalPage(Opts.PhysicalAddress);
      if (Page == nullptr) {
        printf("0x%" PRIx64 " is not a valid physical address.\n",
               Opts.PhysicalAddress);
      } else {
        Hexdump(Opts.PhysicalAddress, Page, 0x1000);
      }
    } else {

      //
      // If the user didn't specify a physical address then dump the first
      // 16 bytes of every physical pages.
      //
      // Note that as the physmem is unordered, so we order the addresses here
      // so that it is nicer for the user as they probably don't expect unorder.
      //

      const auto &Physmem = Dmp.GetPhysmem();
      std::vector<kdmpparser::Physmem_t::key_type> OrderedPhysicalAddresses;
      OrderedPhysicalAddresses.reserve(Physmem.size());

      //
      // Stuff the physical addresses in a vector.
      //

      for (const auto &[PhysicalAddress, _] : Dmp.GetPhysmem()) {
        OrderedPhysicalAddresses.emplace_back(PhysicalAddress);
      }

      //
      // Sort them.
      //

      std::sort(OrderedPhysicalAddresses.begin(),
                OrderedPhysicalAddresses.end());

      //
      // And now we can iterate through them and get the page content.
      //

      for (const auto PhysicalAddress : OrderedPhysicalAddresses) {
        const uint8_t *Page = Dmp.GetPhysicalPage(PhysicalAddress);
        Hexdump(PhysicalAddress, Page, 16);
      }
    }
  }

  return EXIT_SUCCESS;
}