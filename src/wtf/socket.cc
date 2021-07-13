// Axel '0vercl0k' Souchet - November 7 2020
#include "socket.h"
#include <string_view>

enum class Protocol_t {
  Tcp = IPPROTO_TCP,
};

enum class SocketType_t {
  Stream = SOCK_STREAM,
};

struct SocketAddress_t {
  Protocol_t Protocol;
  SocketType_t Type;
  sockaddr_in Addr;

  SocketAddress_t() { memset(this, 0, sizeof(*this)); }
  const sockaddr *Sockaddr() const { return (sockaddr *)&Addr; }
};

std::optional<Protocol_t>
ProtocolFromString(const std::string_view ProtoString) {
  if (ProtoString == "tcp") {
    return Protocol_t::Tcp;
  }

  return std::nullopt;
}

SocketType_t SocketTypeFromProtocol(const Protocol_t Protocol) {
  switch (Protocol) {
  case Protocol_t::Tcp: {
    return SocketType_t::Stream;
  }
  }

  return SocketType_t::Stream;
}

std::optional<SocketAddress_t> SockAddrFromString(const std::string &Address) {
  std::string_view AddressSv(Address);
  if (AddressSv.length() < 3) {
    fmt::print("The address needs to start with a protocol.\n");
    return std::nullopt;
  }

  //
  // Get the protocol.
  //

  const auto &ProtoSv = AddressSv.substr(0, 3);
  const auto &Proto = ProtocolFromString(ProtoSv);
  if (!Proto) {
    fmt::print("Protocol {} is not supported.\n", ProtoSv);
    return std::nullopt;
  }

  //
  // Strip the protocol part.
  //

  AddressSv.remove_prefix(3);
  if (!AddressSv.starts_with("://")) {
    fmt::print("Protocol must be followed by ://.\n");
    return std::nullopt;
  }

  //
  // Strip the :// part.
  //

  AddressSv.remove_prefix(3);

  //
  // If the address has a '/', strips it.
  //

  const auto LastSlash = AddressSv.find_last_of('/');
  if (LastSlash != AddressSv.npos) {
    AddressSv.remove_suffix(AddressSv.length() - LastSlash);
  }

  //
  // Locate the end of the Ip section.
  //

  const auto IpEndOffset = AddressSv.find_last_of(':');
  if (IpEndOffset == AddressSv.npos) {
    fmt::print("The address must contains a port\n");
    return std::nullopt;
  }

  //
  // If the ':' delimiter is the last character, then we don't have a port
  // specified.
  //

  if (IpEndOffset == AddressSv.length()) {
    fmt::print("A port must be specified after the ':'\n");
    return std::nullopt;
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
    return std::nullopt;
  }

  //
  // Ensure the port doesn't overflow the capacity of a uint16_t.
  //

  if (Port > std::numeric_limits<uint16_t>::max()) {
    fmt::print("A port should be a 16 bit value\n");
    return std::nullopt;
  }

  //
  // Ensure that we have an hostname.
  //

  if (IpEndOffset == 0) {
    fmt::print("Expected an hostname.\n");
    return std::nullopt;
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
    return std::nullopt;
  }

  SocketAddress_t SocketAddress;
  SocketAddress.Protocol = Protocol_t(Results->ai_protocol);
  SocketAddress.Type = SocketType_t(Results->ai_socktype);
  memcpy(&SocketAddress.Addr, Results->ai_addr, sizeof(SocketAddress.Addr));
  SocketAddress.Addr.sin_port = htons(Port);

  freeaddrinfo(Results);
  return SocketAddress;
}

std::optional<SocketFd_t> Listen(const std::string &Address) {
  const auto SockAddr = SockAddrFromString(Address);
  if (!SockAddr) {
    fmt::print("SockAddrFromString failed\n");
    return std::nullopt;
  }

  SocketFd_t Fd = socket(SockAddr->Addr.sin_family, int(SockAddr->Type),
                         int(SockAddr->Protocol));
  if (Fd == INVALID_SOCKET) {
    fmt::print("socket failed\n");
    return std::nullopt;
  }

  if (bind(Fd, SockAddr->Sockaddr(), sizeof(SockAddr->Addr)) == -1) {
    fmt::print("bind failed\n");
    return std::nullopt;
  }

  if (listen(Fd, 1) == -1) {
    fmt::print("listen failed\n");
    return std::nullopt;
  }

  return Fd;
}

std::optional<SocketFd_t> Dial(const std::string &Address) {
  const auto SockAddr = SockAddrFromString(Address);
  if (!SockAddr) {
    fmt::print("SockAddrFromString failed\n");
    return std::nullopt;
  }

  SocketFd_t Fd = socket(SockAddr->Addr.sin_family, int(SockAddr->Type),
                         int(SockAddr->Protocol));
  if (Fd == INVALID_SOCKET) {
    fmt::print("socket failed\n");
    return std::nullopt;
  }

  if (connect(Fd, SockAddr->Sockaddr(), sizeof(SockAddr->Addr)) == -1) {
    fmt::print("connect failed\n");
    return std::nullopt;
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
    return std::nullopt;
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
      return std::nullopt;
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