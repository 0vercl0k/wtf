// Copyright (c) 2017-2020, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <CLI/CLI.hpp>
#include <CLI/Timer.hpp>
#include <iostream>
#include <memory>
#include <string>

int main(int argc, char **argv) {
    CLI::AutoTimer("This is a timer");

    CLI::App app("K3Pi goofit fitter");

    CLI::App_p impOpt = std::make_shared<CLI::App>("Important");
    std::string file;
    CLI::Option *opt = impOpt->add_option("-f,--file,file", file, "File name")->required();

    int count{0};
    CLI::Option *copt = impOpt->add_flag("-c,--count", count, "Counter")->required();

    CLI::App_p otherOpt = std::make_shared<CLI::App>("Other");
    double value{0.0};  // = 3.14;
    otherOpt->add_option("-d,--double", value, "Some Value");

    // add the subapps to the main one
    app.add_subcommand(impOpt);
    app.add_subcommand(otherOpt);

    try {
        app.parse(argc, argv);
    } catch(const CLI::ParseError &e) {
        return app.exit(e);
    }

    std::cout << "Working on file: " << file << ", direct count: " << impOpt->count("--file")
              << ", opt count: " << opt->count() << std::endl;
    std::cout << "Working on count: " << count << ", direct count: " << impOpt->count("--count")
              << ", opt count: " << copt->count() << std::endl;
    std::cout << "Some value: " << value << std::endl;

    return 0;
}
