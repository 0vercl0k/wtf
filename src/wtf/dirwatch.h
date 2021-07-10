// Axel '0vercl0k' Souchet - April 5 2020
#pragma once
#include "globals.h"
#include "tsl/robin_set.h"
#include "utils.h"
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

struct DirWatcher_t {
  const fs::path Dir_;
  tsl::robin_pg_set<std::string> Files_;

  DirWatcher_t(const fs::path &Dir) : Dir_(Dir) {}

  const std::vector<fs::path> Run() {
    std::vector<fs::path> NewFiles;
    const std::filesystem::directory_iterator DirIt(Dir_);
    for (const auto &DirEntry : DirIt) {
      const fs::path &NewFilePath = DirEntry.path();
      const auto &Res = Files_.emplace(NewFilePath.string());
      if (!Res.second) {
        continue;
      }

      NewFiles.emplace_back(NewFilePath);
    }

    //
    // Sort per file size.
    //

    std::sort(NewFiles.begin(), NewFiles.end(), CompareTwoFileBySize);
    return NewFiles;
  }
};