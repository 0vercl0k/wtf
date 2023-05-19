// Axel '0vercl0k' Souchet - November 6 2020
#pragma once
#include "backend.h"
#include "globals.h"
#include "platform.h"
#include "tsl/robin_set.h"
#include "yas/serialize.hpp"
#include "yas/std_types.hpp"
#include <fmt/format.h>
#include <optional>
#include <span>
#include <string>

#ifdef WINDOWS
#include <winsock2.h>
#include <ws2tcpip.h>

#include <afunix.h>

#pragma comment(lib, "ws2_32.lib")

struct WSAInitializer_t {
  WSADATA Wsa;
  explicit WSAInitializer_t() {
    if (WSAStartup(MAKEWORD(2, 2), &Wsa) != 0) {
      std::abort();
    }
  }

  ~WSAInitializer_t() { WSACleanup(); }
};

static WSAInitializer_t WSAInitializer;
using SocketFd_t = SOCKET;

static void CloseSocket(const SocketFd_t Fd) { closesocket(Fd); }
static int SocketError() { return WSAGetLastError(); }
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

using SocketFd_t = int;
constexpr int INVALID_SOCKET = -1;

static void CloseSocket(const SocketFd_t Fd) { close(Fd); }
static int SocketError() { return errno; }
#endif

//
// Set up a listening socket bound to Address.
//

[[nodiscard]] std::optional<SocketFd_t> Listen(const std::string &Address);

//
// Set up a connecting socket.
//

[[nodiscard]] std::optional<SocketFd_t> Dial(const std::string &Address);

//
// Send a buffer.
//

[[nodiscard]] bool Send(const SocketFd_t Fd, const uint8_t *Buffer,
                        const size_t Size);

//
// Receive a buffer.
//

[[nodiscard]] std::optional<uint32_t>
Receive(const SocketFd_t Fd, const std::span<uint8_t> ScratchBuffer);

[[nodiscard]] std::optional<uint32_t> Receive(const SocketFd_t Fd,
                                              const uint8_t *ScratchBuffer,
                                              const size_t ScratchBufferSize);

template <typename Ar> void serialize(Ar &ar, Ok_t &) {}
template <typename Ar> void serialize(Ar &ar, Timedout_t &) {}
template <typename Ar> void serialize(Ar &ar, Cr3Change_t &) {}
template <typename Ar> void serialize(Ar &ar, Crash_t &Crash) {
  ar &Crash.CrashName;
}

namespace yas::detail {
template <std::size_t F>
struct serializer<type_prop::not_a_fundamental,
                  ser_case::use_internal_serializer, F, Gva_t> {
  template <typename Archive>
  static Archive &save(Archive &ar, const Gva_t &gva) {
    ar &gva.U64();
    return ar;
  }

  template <typename Archive> static Archive &load(Archive &ar, Gva_t &gva) {
    uint64_t G;
    ar &G;
    gva = Gva_t(G);
    return ar;
  }
};

template <std::size_t F>
struct serializer<type_prop::not_a_fundamental,
                  ser_case::use_internal_serializer, F, tsl::robin_set<Gva_t>> {
  template <typename Archive>
  static Archive &save(Archive &ar, const tsl::robin_set<Gva_t> &set) {
    return concepts::set::save<F>(ar, set);
  }

  template <typename Archive>
  static Archive &load(Archive &ar, tsl::robin_set<Gva_t> &set) {
    return concepts::set::load<F>(ar, set);
  }
};
} // namespace yas::detail

constexpr size_t YasFlags = yas::mem | yas::binary | yas::no_header;
