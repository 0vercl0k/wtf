// Axel '0vercl0k' Souchet - November 27 2022
#pragma once
#include "pch.h"

namespace chrono = std::chrono;

struct PercentageHuman_t {
  uint32_t Value;
};

struct BytesHuman_t {
  double Value;
  const char *Unit;
};

struct NumberHuman_t {
  double Value;
  const char *Unit;
};

struct SecondsHuman_t {
  double Value;
  const char *Unit;
};

template <>
struct fmt::formatter<PercentageHuman_t> : fmt::formatter<std::string> {
  template <typename FormatContext>
  auto format(const PercentageHuman_t &Percentage, FormatContext &Ctx) const
      -> decltype(Ctx.out()) {
    return fmt::format_to(Ctx.out(), "{}%", Percentage.Value);
  }
};

template <> struct fmt::formatter<BytesHuman_t> : fmt::formatter<std::string> {
  template <typename FormatContext>
  auto format(const BytesHuman_t &Bytes, FormatContext &Ctx) const
      -> decltype(Ctx.out()) {
    return fmt::format_to(Ctx.out(), "{:.1f}{}", Bytes.Value, Bytes.Unit);
  }
};

template <> struct fmt::formatter<NumberHuman_t> : fmt::formatter<std::string> {
  template <typename FormatContext>
  auto format(const NumberHuman_t &Number, FormatContext &Ctx) const
      -> decltype(Ctx.out()) {
    return fmt::format_to(Ctx.out(), "{:.1f}{}", Number.Value, Number.Unit);
  }
};

template <>
struct fmt::formatter<SecondsHuman_t> : fmt::formatter<std::string> {
  template <typename FormatContext>
  auto format(const SecondsHuman_t &Micro, FormatContext &Ctx) const
      -> decltype(Ctx.out()) {
    return fmt::format_to(Ctx.out(), "{:.1f}{}", Micro.Value, Micro.Unit);
  }
};

//
// Utility that calculates the number of seconds sine a time point.
//

[[nodiscard]] chrono::seconds
SecondsSince(const chrono::system_clock::time_point &Since);

//
// Utility that is used to print seconds for human.
//

[[nodiscard]] SecondsHuman_t SecondsToHuman(const chrono::seconds &Seconds);

//
// Utility that is used to print bytes for human.
//

[[nodiscard]] BytesHuman_t BytesToHuman(const uint64_t Bytes_);

//
// Utility that is used to print numbers for human.
//

[[nodiscard]] NumberHuman_t NumberToHuman(const uint64_t N_);
