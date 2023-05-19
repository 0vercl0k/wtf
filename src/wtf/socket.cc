// Axel '0vercl0k' Souchet - November 7 2020
#include "socket.h"
#include <string.h>
#include <string_view>
#include <utility>
#include <variant>

enum class Protocol_t {
  Tcp = IPPROTO_TCP,
  Unix = 0,
};

enum class SocketType_t {
  Stream = SOCK_STREAM,
};

struct SocketAddress_t {
  Protocol_t Protocol;
  int Family = -1;
  SocketType_t Type = SocketType_t::Stream;
  std::variant<sockaddr_in, sockaddr_un> Addr;

  SocketAddress_t(const Protocol_t Protocol_) : Protocol(Protocol_) {
    if (Protocol == Protocol_t::Tcp) {
      sockaddr_in In = {};
      Family = AF_INET;
      In.sin_family = Family;
      Addr = In;
      return;
    } else if (Protocol == Protocol_t::Unix) {
      sockaddr_un Un = {};
      Family = AF_UNIX;
      Un.sun_family = Family;
      Addr = Un;
    }
  }

  sockaddr_un &Sockun() { return std::get<sockaddr_un>(Addr); }
  const sockaddr_un &Sockun() const { return std::get<sockaddr_un>(Addr); }

  sockaddr_in &Sockin() { return std::get<sockaddr_in>(Addr); }
  const sockaddr_in &Sockin() const { return std::get<sockaddr_in>(Addr); }

  std::pair<const sockaddr *, size_t> Sockaddr() const {
    if (Protocol == Protocol_t::Tcp) {
      const auto &In = Sockin();
      return {(const sockaddr *)&In, sizeof(In)};
    }

    const auto &Un = Sockun();
    return {(const sockaddr *)&Un, sizeof(Un)};
  }
};

std::optional<Protocol_t>
ProtocolFromString(const std::string_view ProtoString) {
  if (ProtoString == "tcp") {
    return Protocol_t::Tcp;
  }

  if (ProtoString == "unix") {
    return Protocol_t::Unix;
  }

  return {};
}

SocketType_t SocketTypeFromProtocol(const Protocol_t Protocol) {
  switch (Protocol) {
  case Protocol_t::Unix:
  case Protocol_t::Tcp: {
    return SocketType_t::Stream;
  }
  }

  std::abort();
  return SocketType_t::Stream;
}

std::optional<SocketAddress_t> SockAddrFromString(const std::string &Address) {
  std::string_view AddressSv(Address);

  //
  // Get the protocol.
  //

  const auto &ProtoEndIdx = AddressSv.find("://");
  if (ProtoEndIdx == AddressSv.npos) {
    fmt::print("The address {} is malformed.\n", AddressSv);
    return {};
  }

  const auto &ProtoSv = AddressSv.substr(0, ProtoEndIdx);
  const auto &Proto = ProtocolFromString(ProtoSv);
  if (!Proto) {
    fmt::print("Protocol {} is not supported.\n", ProtoSv);
    return {};
  }

  //
  // Strip the protocol part.
  //

  AddressSv.remove_prefix(ProtoEndIdx);

  //
  // Strip the :// part.
  //

  AddressSv.remove_prefix(3);

  //
  // If the address ends w/ a '/', strips it.
  //

  if (AddressSv.back() == '/') {
    AddressSv.remove_suffix(1);
  }

  //
  // Handle TCP.
  //

  if (Proto == Protocol_t::Tcp) {

    //
    // Locate the end of the Ip section.
    //

    const auto IpEndOffset = AddressSv.find_last_of(':');
    if (IpEndOffset == AddressSv.npos) {
      fmt::print("The address must contains a port\n");
      return {};
    }

    //
    // If the ':' delimiter is the last character, then we don't have a port
    // specified.
    //

    if (IpEndOffset == AddressSv.npos) {
      fmt::print("A port must be specified after the ':'\n");
      return {};
    }

    //
    // The port is anything that comes after the delimiter.
    //

    const auto PortString = AddressSv.substr(IpEndOffset + 1);

    //
    // Convert the port to an integer.
    //

    const char *PortStringEnd = PortString.data() + PortString.length();
    char *EndPtr = (char *)PortStringEnd;
    const uint64_t Port = strtoull(PortString.data(), &EndPtr, 10);

    if (EndPtr != PortStringEnd) {
      fmt::print("Port failed conversion\n");
      return {};
    }

    //
    // Ensure the port doesn't overflow the capacity of a uint16_t.
    //

    if (Port > std::numeric_limits<uint16_t>::max()) {
      fmt::print("A port should be a 16 bit value\n");
      return {};
    }

    //
    // Ensure that we have an hostname.
    //

    if (IpEndOffset == 0) {
      fmt::print("Expected an hostname.\n");
      return {};
    }

    //
    // Copy the ip in a string.
    //

    const std::string Ip(AddressSv.substr(0, IpEndOffset));

    //
    // Populate the structure now.
    //

    struct addrinfo Hints;
    memset(&Hints, 0, sizeof(Hints));
    Hints.ai_family = AF_INET;
    Hints.ai_socktype = int(SocketTypeFromProtocol(*Proto));
    Hints.ai_protocol = int(*Proto);

    struct addrinfo *Results = nullptr;
    if (getaddrinfo(Ip.data(), nullptr, &Hints, &Results) != 0) {
      fmt::print("{} could not be converted by inet_pton / getaddrinfo\n", Ip);
      return {};
    }

    SocketAddress_t SocketAddress(Protocol_t(Results->ai_protocol));
    if (Results->ai_addrlen > sizeof(SocketAddress.Sockin())) {
      fmt::print("getaddrinfo() returned a sockaddr larger than expected\n");
      return {};
    }

    memcpy(&SocketAddress.Sockin(), Results->ai_addr, Results->ai_addrlen);
    SocketAddress.Sockin().sin_port = htons(Port);
    return SocketAddress;
  }

  //
  // Handle UNIX.
  //

  const std::string SocketName(AddressSv);
  SocketAddress_t SocketAddress(Protocol_t::Unix);
  if (SocketName.length() > 64) {
    fmt::print("'{}' is too big as a name, bailing.\n", SocketName);
    return {};
  }

  strcpy(SocketAddress.Sockun().sun_path, SocketName.c_str());
  return SocketAddress;
}

std::optional<SocketFd_t> Listen(const std::string &Address) {
  const auto SockAddr = SockAddrFromString(Address);
  if (!SockAddr) {
    fmt::print("SockAddrFromString failed\n");
    return {};
  }

  if (SockAddr->Protocol == Protocol_t::Unix) {
    const std::string SocketPath = SockAddr->Sockun().sun_path;
    fmt::print("Deleting {}..\n", SocketPath);
    fs::remove(SocketPath);
  }

  SocketFd_t Fd =
      socket(SockAddr->Family, int(SockAddr->Type), int(SockAddr->Protocol));
  if (Fd == INVALID_SOCKET) {
    fmt::print("socket failed\n");
    return {};
  }

  const auto &[Addr, AddrLen] = SockAddr->Sockaddr();
  if (bind(Fd, Addr, AddrLen) == -1) {
    fmt::print("bind failed\n");
    return {};
  }

  if (listen(Fd, 1) == -1) {
    fmt::print("listen failed\n");
    return {};
  }

  return Fd;
}

std::optional<SocketFd_t> Dial(const std::string &Address) {
  const auto SockAddr = SockAddrFromString(Address);
  if (!SockAddr) {
    fmt::print("SockAddrFromString failed\n");
    return {};
  }

  SocketFd_t Fd =
      socket(SockAddr->Family, int(SockAddr->Type), int(SockAddr->Protocol));
  if (Fd == INVALID_SOCKET) {
    fmt::print("socket failed\n");
    return {};
  }

  const auto &[Addr, AddrLen] = SockAddr->Sockaddr();
  if (connect(Fd, Addr, AddrLen) == -1) {
    fmt::print("connect failed\n");
    return {};
  }

  return Fd;
}

bool Send(const SocketFd_t Fd, const uint8_t *Buffer, const size_t Size) {
  const uint32_t SendSize = uint32_t(Size);
  if (send(Fd, (char *)&SendSize, sizeof(SendSize), 0) == -1) {
    fmt::print("send size failed\n");
    return false;
  }

  if (send(Fd, (char *)Buffer, SendSize, 0) == -1) {
    fmt::print("send buffer failed\n");
    return false;
  }

  return true;
}

std::optional<uint32_t> Receive(const SocketFd_t Fd,
                                const uint8_t *ScratchBuffer,
                                const size_t ScratchBufferSize) {
  char *CurrentBuffer = (char *)ScratchBuffer;
  if (const int R = recv(Fd, (char *)CurrentBuffer, sizeof(uint32_t), 0);
      R == -1 || R != sizeof(uint32_t)) {
    fmt::print("Could not receive size ({})\n", R);
    return {};
  }

  const uint32_t Expected = *(uint32_t *)ScratchBuffer;
  if (Expected > ScratchBufferSize) {
    fmt::print("Received a message that would not fit in the scratch buffer "
               "({} VS {})\n",
               Expected, ScratchBufferSize);
    return false;
  }

  CurrentBuffer = (char *)ScratchBuffer;
  uint32_t ReceivedSize = 0;
  uint32_t MaxSize = ScratchBufferSize;
  while (ReceivedSize != Expected) {
    const int ReceivedChunkSize = recv(Fd, CurrentBuffer, MaxSize, 0);
    if (ReceivedChunkSize == -1 || ReceivedChunkSize < 0) {
      return {};
    }

    ReceivedSize += ReceivedChunkSize;
    CurrentBuffer += ReceivedChunkSize;
    MaxSize -= ReceivedChunkSize;
  }

  return Expected;
}

std::optional<uint32_t> Receive(const SocketFd_t Fd,
                                const std::span<uint8_t> ScratchBuffer) {
  return Receive(Fd, ScratchBuffer.data(), ScratchBuffer.size_bytes());
}