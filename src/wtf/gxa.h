// Axel '0vercl0k' Souchet - January 16 2020
#pragma once
#include <cstdint>
#include <fmt/format.h>

//
// Strong type for GPA/GVA.
//

class Gpa_t {
  uint64_t Gpa_ = 0;

public:
  Gpa_t() = default;
  explicit Gpa_t(const uint64_t Gpa) : Gpa_(Gpa) {}
  uint64_t U64() const { return Gpa_; }
  Gpa_t Offset() const { return Gpa_t(Gpa_ & 0xfff); }
  Gpa_t Align() const { return Gpa_t(Gpa_ & ~0xfff); }

  bool operator!=(const Gpa_t &Gpa) const { return Gpa_ != Gpa.U64(); }
  bool operator==(const Gpa_t &Gpa) const { return Gpa_ == Gpa.U64(); }
  bool operator<(const Gpa_t &Gpa) const { return Gpa_ < Gpa.U64(); }
  uint64_t *operator&() { return &Gpa_; }
  Gpa_t operator*(const Gpa_t &Gpa) const { return Gpa_t(Gpa_ * Gpa.U64()); }
  Gpa_t operator+(const Gpa_t &Gpa) const { return Gpa_t(Gpa_ + Gpa.U64()); }
  Gpa_t &operator+=(const Gpa_t &Gpa) {
    Gpa_ += Gpa.U64();
    return *this;
  }
};

class Gva_t {
  uint64_t Gva_ = 0;

public:
  Gva_t() = default;
  explicit Gva_t(const uint64_t Gva) : Gva_(Gva) {}
  uint64_t U64() const { return Gva_; }
  Gva_t Offset() const { return Gva_t(Gva_ & 0xfff); }
  Gva_t Align() const { return Gva_t(Gva_ & ~0xfff); }

  bool operator!=(const Gva_t &Gva) const { return Gva_ != Gva.U64(); }
  bool operator==(const Gva_t &Gva) const { return Gva_ == Gva.U64(); }
  bool operator<(const Gva_t &Gva) const { return Gva_ < Gva.U64(); }
  explicit operator bool() const { return Gva_ != 0; }
  uint64_t *operator&() { return &Gva_; }
  Gva_t operator+(const Gva_t &Gva) const { return Gva_t(Gva_ + Gva.U64()); }
  Gva_t operator-(const Gva_t &Gva) const { return Gva_t(Gva_ - Gva.U64()); }
  Gva_t &operator+=(const Gva_t &Gva) {
    Gva_ += Gva.U64();
    return *this;
  }
};

//
// fmt specialization for Gpa_t/Gva_t.
//

template <> struct fmt::formatter<Gpa_t> : formatter<uint64_t> {
  template <typename FormatContext>
  auto format(const Gpa_t Gpa, FormatContext &ctx) {
    return formatter<uint64_t>::format(Gpa.U64(), ctx);
  }
};

template <> struct fmt::formatter<Gva_t> : formatter<uint64_t> {
  template <typename FormatContext>
  auto format(const Gva_t Gva, FormatContext &ctx) {
    return formatter<uint64_t>::format(Gva.U64(), ctx);
  }
};

//
// Hash specialization for Gpa_t/Gva_t.
//

namespace std {
template <> struct hash<Gpa_t> {
  std::size_t operator()(const Gpa_t &Gpa) const noexcept {
    return std::hash<uint64_t>()(Gpa.U64());
  }
};
template <> struct hash<Gva_t> {
  std::size_t operator()(const Gva_t &Gva) const noexcept {
    return std::hash<uint64_t>()(Gva.U64());
  }
};
} // namespace std
