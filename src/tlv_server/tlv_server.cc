// y0ny0ns0n / Axel '0vercl0k' Souchet - February 1 2022
#include <algorithm>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

enum class Command_t : uint32_t { Allocate, Edit, Delete };

struct CommonPacketHeader_t {
  Command_t CommandId;
  uint32_t ChunkId;
  uint32_t BodySize;
};

struct Chunk_t {
  uint32_t Id;
  int32_t Size;
  std::unique_ptr<uint8_t[]> Buf;
};

std::unique_ptr<Chunk_t> ChunkList[4] = {};

void ProcessPacket(const uint8_t *Packet, const uint32_t PacketSize) {
  auto Header = (CommonPacketHeader_t *)Packet;

  if (PacketSize < sizeof(*Header)) {
    printf("[!] Packet is not big enough to check the header\n");
    return;
  }

  if (Header->BodySize != (PacketSize - sizeof(*Header))) {
    printf("[!] Body size is not accurate\n");
    return;
  }

  const auto CommandId = Header->CommandId;
  const auto Body = (uint8_t *)(Header + 1);
  printf("[+] CommandId = %d\n", CommandId);

  switch (CommandId) {
  case Command_t::Allocate: {
    const auto &ChunkId = Header->ChunkId;
    const auto &FreeChunkPtr =
        std::find_if(ChunkList, std::end(ChunkList),
                     [&](const auto &Entry) { return Entry == nullptr; });

#ifdef PATCHED
    if (FreeChunkPtr == std::end(ChunkList)) {
      printf("[!] there's no available slot.\n");
      return;
    }
#endif

    auto Chunk = std::make_unique<Chunk_t>();
    Chunk->Id = ChunkId;
    Chunk->Size = Header->BodySize;
    Chunk->Buf = std::make_unique<uint8_t[]>(Chunk->Size);
    memcpy(Chunk->Buf.get(), Body, Chunk->Size);
    *FreeChunkPtr = std::move(Chunk);
    break;
  }

  case Command_t::Edit: {
    const auto &ChunkId = Header->ChunkId;
    const auto &MatchingChunkPtr = std::find_if(
        std::begin(ChunkList), std::end(ChunkList), [&](const auto &Entry) {
          return Entry != nullptr && Entry->Id == ChunkId;
        });

    if (MatchingChunkPtr == std::end(ChunkList)) {
      printf("[!] Couldn't find ChunkId 0x%" PRIx32 "\n", ChunkId);
      return;
    }

    auto MatchingChunk = MatchingChunkPtr->get();
    const uint32_t NewBufSize = Header->BodySize;

    if (NewBufSize > MatchingChunk->Size) {
      MatchingChunk->Buf = std::make_unique<uint8_t[]>(NewBufSize);
      MatchingChunk->Size = NewBufSize;
    }

    memcpy(MatchingChunk->Buf.get(), Body, NewBufSize);
    break;
  }

  case Command_t::Delete: {
    const auto &ChunkId = Header->ChunkId;
    const auto &MatchingChunkPtr = std::find_if(
        std::begin(ChunkList), std::end(ChunkList), [&](const auto &Entry) {
          return Entry != nullptr && Entry->Id == ChunkId;
        });

    if (MatchingChunkPtr == std::end(ChunkList)) {
      printf("[!] Couldn't find ChunkId 0x%" PRIx32 "\n", ChunkId);
      return;
    }

    MatchingChunkPtr->reset();
    break;
  }
  }
}

struct WSAInitializer_t {
  WSADATA Wsa = {};
  WSAInitializer_t() {
    if (WSAStartup(MAKEWORD(2, 2), &Wsa) != 0) {
      std::abort();
    }
  }

  ~WSAInitializer_t() { WSACleanup(); }
} WsaData;

static WSAInitializer_t WSAInitializer;

int main() {
  // Highly inspired from:
  // https://docs.microsoft.com/en-us/windows/win32/winsock/complete-server-code

  SOCKET ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (ListenSocket == INVALID_SOCKET) {
    printf("[!] socket failed, WSA GLE = 0x%08x\n", WSAGetLastError());
    return EXIT_FAILURE;
  }

  struct sockaddr_in LoopbackAddr = {};
  const uint16_t ListenPort = 4444;
  LoopbackAddr.sin_family = AF_INET;
  LoopbackAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
  LoopbackAddr.sin_port = htons(ListenPort);
  if (bind(ListenSocket, (sockaddr *)&LoopbackAddr, sizeof(LoopbackAddr)) ==
      SOCKET_ERROR) {
    printf("[!] bind failed, WSA GLE = 0x%08x\n", WSAGetLastError());
    closesocket(ListenSocket);
    return EXIT_FAILURE;
  }

  printf("[+] Listening on tcp:%" PRIu16 "...\n", ListenPort);
  if (listen(ListenSocket, 1) == SOCKET_ERROR) {
    printf("[!] listen failed, WSA GLE = 0x%08x\n", WSAGetLastError());
    closesocket(ListenSocket);
    return EXIT_FAILURE;
  }

  SOCKET ClientSocket = accept(ListenSocket, nullptr, nullptr);
  if (ClientSocket == INVALID_SOCKET) {
    printf("[!] accept failed, WSA GLE = 0x%08x\n", WSAGetLastError());
    closesocket(ListenSocket);
    return EXIT_FAILURE;
  }

  printf("[+] accept done!\n");

  while (1) {
    uint32_t BufferSize = 0;
    int Received =
        recv(ClientSocket, (char *)&BufferSize, sizeof(BufferSize), 0);

    if (Received != sizeof(BufferSize)) {
      printf("[!] recv failed or didn't receive enough, WSA GLE = 0x%08x\n",
             WSAGetLastError());
      break;
    }

    if (BufferSize == 0 || BufferSize >= 0x4'00) {
      printf("[!] BufSize(0x%" PRIx32 ") too big, skipping\n ", BufferSize);
      continue;
    }

    auto Buffer = std::make_unique<uint8_t[]>(BufferSize);
    printf("[+] BufferSize = %" PRIx32 "\n", BufferSize);

    Received = recv(ClientSocket, (char *)Buffer.get(), BufferSize, 0);

    if (Received != BufferSize) {
      printf("recv failed, WSA GLE = 0x%08x\n", WSAGetLastError());
      break;
    }

    ProcessPacket(Buffer.get(), BufferSize);
  }

  for (auto &Chunk : ChunkList) {
    Chunk.reset();
  }

  closesocket(ClientSocket);
  return EXIT_SUCCESS;
}