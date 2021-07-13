// Axel '0vercl0k' Souchet - January 17 2020
#pragma once

#ifdef __cplusplus

#include "CLI/CLI.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

#include <string>
#include <string_view>

#include <optional>
#include <variant>

#include <filesystem>

#include <unordered_map>
#include <unordered_set>

#include <memory>
#include <chrono>
#include <random>
#include <algorithm>

#include <fstream>

#include <fmt/format.h>

#include <tsl/robin_set.h>
#include <tsl/robin_map.h>

#include "platform.h"
#include "backend.h"
#include "utils.h"
#include "targets.h"
#include "globals.h"
#include "whv_backend.h"
#include "kvm_backend.h"
#include "bochscpu_backend.h"

#endif