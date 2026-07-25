#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <mstcpip.h>
#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif
typedef SOCKET PcSocket;
typedef int PcSockLen;
#define PC_INVALID_SOCKET INVALID_SOCKET
#define PcCloseSocket closesocket
#else
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
typedef int PcSocket;
typedef socklen_t PcSockLen;
#define PC_INVALID_SOCKET (-1)
#define PcCloseSocket close
#endif

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pc_link_protocol.h"

#define SERVER_MAX_WAITING 1024
#define SERVER_MAX_MATCHES 512
#define SERVER_WAIT_TIMEOUT_MS 30000
#define SERVER_MATCH_TIMEOUT_MS 30000

struct WaitingClient
{
    int active;
    uint8_t code[PC_LINK_CODE_LENGTH];
    uint8_t codeLength;
    uint64_t nonce;
    uint64_t clientId;
    struct sockaddr_in address;
    uint64_t lastSeen;
};

struct Match
{
    int active;
    uint64_t session;
    uint64_t nonce[2];
    uint64_t clientId[2];
    struct sockaddr_in address[2];
    struct sockaddr_in advertisedAddress[2];
    uint64_t lastSeen;
};

static struct WaitingClient sWaiting[SERVER_MAX_WAITING];
static struct Match sMatches[SERVER_MAX_MATCHES];
static volatile sig_atomic_t sRunning = 1;

static uint64_t HostToNetwork64(uint64_t value)
{
    uint32_t high = htonl((uint32_t)(value >> 32));
    uint32_t low = htonl((uint32_t)value);

    return ((uint64_t)low << 32) | high;
}

static uint64_t NetworkToHost64(uint64_t value)
{
    return HostToNetwork64(value);
}

static uint64_t GetTimeMs(void)
{
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
#endif
}

static uint32_t GetProcessIdValue(void)
{
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return getpid();
#endif
}

static uint64_t MakeSession(uint64_t first, uint64_t second)
{
    uint64_t value = first ^ (second << 1) ^ GetTimeMs() ^ (uint64_t)GetProcessIdValue() << 32;

    value ^= value << 13;
    value ^= value >> 7;
    value ^= value << 17;
    return value ? value : 1;
}

static void InitializePacket(struct PcLinkPacket *packet, uint8_t type)
{
    memset(packet, 0, sizeof(*packet));
    packet->magic = htonl(PC_LINK_MAGIC);
    packet->version = PC_LINK_VERSION;
    packet->type = type;
}

static int IsLoopbackAddress(uint32_t address)
{
    return (ntohl(address) >> 24) == 127;
}

static uint32_t GetLocalAddressFor(const struct sockaddr_in *destination)
{
    struct sockaddr_in localAddress;
    PcSockLen localAddressLength = sizeof(localAddress);
    PcSocket routeSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    uint32_t address = 0;

    if (routeSocket == PC_INVALID_SOCKET)
        return 0;
    if (connect(routeSocket,
                (const struct sockaddr *)destination,
                sizeof(*destination)) == 0
     && getsockname(routeSocket,
                    (struct sockaddr *)&localAddress,
                    &localAddressLength) == 0)
        address = localAddress.sin_addr.s_addr;
    PcCloseSocket(routeSocket);
    return address;
}

static void UpdateAdvertisedAddresses(struct Match *match)
{
    int player;

    memcpy(match->advertisedAddress, match->address, sizeof(match->address));
    for (player = 0; player < 2; player++)
    {
        int other = player ^ 1;

        if (IsLoopbackAddress(match->address[player].sin_addr.s_addr)
         && !IsLoopbackAddress(match->address[other].sin_addr.s_addr))
        {
            uint32_t address = GetLocalAddressFor(&match->address[other]);

            if (address != 0 && !IsLoopbackAddress(address))
                match->advertisedAddress[player].sin_addr.s_addr = address;
        }
    }
}

static void SendMatch(PcSocket socket, const struct Match *match, int player)
{
    struct PcLinkPacket packet;

    InitializePacket(&packet, PC_LINK_PACKET_MATCH);
    packet.playerId = player;
    packet.nonce = HostToNetwork64(match->nonce[player]);
    packet.session = HostToNetwork64(match->session);
    packet.peerAddress = match->advertisedAddress[player ^ 1].sin_addr.s_addr;
    packet.peerPort = match->advertisedAddress[player ^ 1].sin_port;
    sendto(socket,
           (const char *)&packet,
           sizeof(packet),
           0,
           (const struct sockaddr *)&match->address[player],
           sizeof(match->address[player]));
}

static struct Match *FindMatch(uint64_t nonce, int *player)
{
    size_t i;

    for (i = 0; i < SERVER_MAX_MATCHES; i++)
    {
        if (!sMatches[i].active)
            continue;
        if (sMatches[i].nonce[0] == nonce)
        {
            *player = 0;
            return &sMatches[i];
        }
        if (sMatches[i].nonce[1] == nonce)
        {
            *player = 1;
            return &sMatches[i];
        }
    }
    return NULL;
}

static struct Match *FindMatchByClientId(uint64_t clientId, int *player)
{
    size_t i;

    for (i = 0; i < SERVER_MAX_MATCHES; i++)
    {
        if (!sMatches[i].active)
            continue;
        if (sMatches[i].clientId[0] == clientId)
        {
            *player = 0;
            return &sMatches[i];
        }
        if (sMatches[i].clientId[1] == clientId)
        {
            *player = 1;
            return &sMatches[i];
        }
    }
    return NULL;
}

static struct WaitingClient *FindWaiting(uint64_t nonce)
{
    size_t i;

    for (i = 0; i < SERVER_MAX_WAITING; i++)
    {
        if (sWaiting[i].active && sWaiting[i].nonce == nonce)
            return &sWaiting[i];
    }
    return NULL;
}

static struct WaitingClient *FindWaitingByClientId(uint64_t clientId)
{
    size_t i;

    for (i = 0; i < SERVER_MAX_WAITING; i++)
    {
        if (sWaiting[i].active && sWaiting[i].clientId == clientId)
            return &sWaiting[i];
    }
    return NULL;
}

static struct WaitingClient *FindPartner(const struct PcLinkPacket *packet, uint64_t nonce)
{
    uint64_t clientId = NetworkToHost64(packet->clientId);
    size_t i;

    for (i = 0; i < SERVER_MAX_WAITING; i++)
    {
        if (sWaiting[i].active
         && sWaiting[i].nonce != nonce
         && sWaiting[i].clientId != clientId
         && sWaiting[i].codeLength == packet->codeLength
         && memcmp(sWaiting[i].code, packet->code, packet->codeLength) == 0)
            return &sWaiting[i];
    }
    return NULL;
}

static struct WaitingClient *AllocateWaiting(void)
{
    size_t i;

    for (i = 0; i < SERVER_MAX_WAITING; i++)
    {
        if (!sWaiting[i].active)
            return &sWaiting[i];
    }
    return NULL;
}

static struct Match *AllocateMatch(void)
{
    size_t i;

    for (i = 0; i < SERVER_MAX_MATCHES; i++)
    {
        if (!sMatches[i].active)
            return &sMatches[i];
    }
    return NULL;
}

static void HandleHello(PcSocket socket,
                        const struct PcLinkPacket *packet,
                        const struct sockaddr_in *source,
                        uint64_t now)
{
    uint64_t nonce = NetworkToHost64(packet->nonce);
    uint64_t clientId = NetworkToHost64(packet->clientId);
    struct WaitingClient *client;
    struct WaitingClient *partner;
    struct Match *match;
    int player;

    if (nonce == 0
     || clientId == 0
     || packet->codeLength == 0
     || packet->codeLength > PC_LINK_CODE_LENGTH)
        return;

    match = FindMatch(nonce, &player);
    if (match != NULL)
    {
        match->address[player] = *source;
        UpdateAdvertisedAddresses(match);
        match->lastSeen = now;
        SendMatch(socket, match, 0);
        SendMatch(socket, match, 1);
        return;
    }

    match = FindMatchByClientId(clientId, &player);
    if (match != NULL)
    {
        match->nonce[player] = nonce;
        match->address[player] = *source;
        match->session = MakeSession(match->nonce[0], match->nonce[1]);
        match->lastSeen = now;
        UpdateAdvertisedAddresses(match);
        SendMatch(socket, match, 0);
        SendMatch(socket, match, 1);
        fprintf(stderr,
                "player %d restarted link attempt; refreshed session\n",
                player + 1);
        return;
    }

    client = FindWaiting(nonce);
    if (client != NULL)
    {
        if (client->clientId != clientId)
            return;
        client->address = *source;
        client->lastSeen = now;
        return;
    }

    client = FindWaitingByClientId(clientId);
    if (client != NULL)
    {
        client->nonce = nonce;
        client->address = *source;
        client->codeLength = packet->codeLength;
        memcpy(client->code, packet->code, packet->codeLength);
        client->lastSeen = now;
        return;
    }

    partner = FindPartner(packet, nonce);
    if (partner == NULL)
    {
        client = AllocateWaiting();
        if (client == NULL)
            return;
        memset(client, 0, sizeof(*client));
        client->active = 1;
        client->nonce = nonce;
        client->clientId = clientId;
        client->address = *source;
        client->codeLength = packet->codeLength;
        memcpy(client->code, packet->code, packet->codeLength);
        client->lastSeen = now;
        return;
    }

    match = AllocateMatch();
    if (match == NULL)
        return;
    memset(match, 0, sizeof(*match));
    match->active = 1;
    if (partner->clientId < clientId
     || (partner->clientId == clientId && partner->nonce < nonce))
    {
        match->nonce[0] = partner->nonce;
        match->nonce[1] = nonce;
        match->clientId[0] = partner->clientId;
        match->clientId[1] = clientId;
        match->address[0] = partner->address;
        match->address[1] = *source;
    }
    else
    {
        match->nonce[0] = nonce;
        match->nonce[1] = partner->nonce;
        match->clientId[0] = clientId;
        match->clientId[1] = partner->clientId;
        match->address[0] = *source;
        match->address[1] = partner->address;
    }
    match->session = MakeSession(match->nonce[0], match->nonce[1]);
    match->lastSeen = now;
    partner->active = 0;
    UpdateAdvertisedAddresses(match);
    SendMatch(socket, match, 0);
    SendMatch(socket, match, 1);

    {
        char sourceAddress[2][INET_ADDRSTRLEN];
        char advertisedAddress[2][INET_ADDRSTRLEN];

        inet_ntop(AF_INET,
                  &match->address[0].sin_addr,
                  sourceAddress[0],
                  sizeof(sourceAddress[0]));
        inet_ntop(AF_INET,
                  &match->address[1].sin_addr,
                  sourceAddress[1],
                  sizeof(sourceAddress[1]));
        inet_ntop(AF_INET,
                  &match->advertisedAddress[0].sin_addr,
                  advertisedAddress[0],
                  sizeof(advertisedAddress[0]));
        inet_ntop(AF_INET,
                  &match->advertisedAddress[1].sin_addr,
                  advertisedAddress[1],
                  sizeof(advertisedAddress[1]));
        fprintf(stderr,
                "matched %08llx at %s:%u and %08llx at %s:%u\n",
                (unsigned long long)match->nonce[0],
                sourceAddress[0],
                ntohs(match->address[0].sin_port),
                (unsigned long long)match->nonce[1],
                sourceAddress[1],
                ntohs(match->address[1].sin_port));
        if (match->advertisedAddress[0].sin_addr.s_addr
              != match->address[0].sin_addr.s_addr
         || match->advertisedAddress[1].sin_addr.s_addr
              != match->address[1].sin_addr.s_addr)
        {
            fprintf(stderr,
                    "advertised direct endpoints %s:%u and %s:%u\n",
                    advertisedAddress[0],
                    ntohs(match->advertisedAddress[0].sin_port),
                    advertisedAddress[1],
                    ntohs(match->advertisedAddress[1].sin_port));
        }
    }
}

static void HandleRelayPacket(PcSocket socket,
                              const struct PcLinkPacket *packet,
                              const struct sockaddr_in *source,
                              uint64_t now)
{
    uint64_t session = NetworkToHost64(packet->session);
    uint64_t nonce = NetworkToHost64(packet->nonce);
    uint64_t clientId = NetworkToHost64(packet->clientId);
    size_t i;
    int player;

    if (session == 0 || packet->playerId > 1)
        return;
    player = packet->playerId;
    for (i = 0; i < SERVER_MAX_MATCHES; i++)
    {
        struct Match *match = &sMatches[i];

        if (!match->active
         || match->session != session
         || match->nonce[player] != nonce
         || match->clientId[player] != clientId)
            continue;

        match->address[player] = *source;
        match->lastSeen = now;
        sendto(socket,
               (const char *)packet,
               sizeof(*packet),
               0,
               (const struct sockaddr *)&match->address[player ^ 1],
               sizeof(match->address[player ^ 1]));
        return;
    }
}

static void ExpireEntries(uint64_t now)
{
    size_t i;

    for (i = 0; i < SERVER_MAX_WAITING; i++)
    {
        if (sWaiting[i].active && now - sWaiting[i].lastSeen >= SERVER_WAIT_TIMEOUT_MS)
            sWaiting[i].active = 0;
    }
    for (i = 0; i < SERVER_MAX_MATCHES; i++)
    {
        if (sMatches[i].active && now - sMatches[i].lastSeen >= SERVER_MATCH_TIMEOUT_MS)
            sMatches[i].active = 0;
    }
}

static void HandleSignal(int signalNumber)
{
    (void)signalNumber;
    sRunning = 0;
}

static int ParsePort(const char *text, uint16_t *port)
{
    char *end;
    unsigned long value = strtoul(text, &end, 10);

    if (*text == '\0' || *end != '\0' || value == 0 || value > 65535)
        return 0;
    *port = (uint16_t)value;
    return 1;
}

int main(int argc, char **argv)
{
    const char *bindAddress = "0.0.0.0";
    uint16_t port = PC_LINK_DEFAULT_PORT;
    struct sockaddr_in address;
    PcSocket socketFd = PC_INVALID_SOCKET;
    int arg;

#ifdef _WIN32
    {
        WSADATA data;

        if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
        {
            fprintf(stderr, "could not initialize Winsock\n");
            return 1;
        }
    }
#endif

    for (arg = 1; arg < argc; arg++)
    {
        if (strcmp(argv[arg], "--bind") == 0 && arg + 1 < argc)
            bindAddress = argv[++arg];
        else if (strcmp(argv[arg], "--port") == 0 && arg + 1 < argc)
        {
            if (!ParsePort(argv[++arg], &port))
            {
                fprintf(stderr, "invalid port\n");
                return 2;
            }
        }
        else
        {
            fprintf(stderr, "usage: %s [--bind ADDRESS] [--port PORT]\n", argv[0]);
            return 2;
        }
    }

    socketFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socketFd == PC_INVALID_SOCKET)
    {
#ifdef _WIN32
        fprintf(stderr, "socket failed: Windows error %d\n", WSAGetLastError());
#else
        perror("socket");
#endif
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
#ifdef _WIN32
    {
        BOOL enableConnectionReset = FALSE;
        DWORD bytesReturned;

        // An ICMP port-unreachable response is specific to one stale UDP
        // endpoint and must not terminate the shared rendezvous server.
        WSAIoctl(socketFd,
                 SIO_UDP_CONNRESET,
                 &enableConnectionReset,
                 sizeof(enableConnectionReset),
                 NULL,
                 0,
                 &bytesReturned,
                 NULL,
                 NULL);
    }
#endif
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (inet_pton(AF_INET, bindAddress, &address.sin_addr) != 1)
    {
        fprintf(stderr, "invalid bind address: %s\n", bindAddress);
        PcCloseSocket(socketFd);
#ifdef _WIN32
        WSACleanup();
#endif
        return 2;
    }
    if (bind(socketFd, (struct sockaddr *)&address, sizeof(address)) != 0)
    {
#ifdef _WIN32
        fprintf(stderr, "bind failed: Windows error %d\n", WSAGetLastError());
#else
        perror("bind");
#endif
        PcCloseSocket(socketFd);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    signal(SIGINT, HandleSignal);
    signal(SIGTERM, HandleSignal);
    fprintf(stderr, "pokeemerald link server listening on %s:%u UDP\n", bindAddress, port);
    while (sRunning)
    {
        struct PcLinkPacket packet;
        struct sockaddr_in source;
        PcSockLen sourceLength = sizeof(source);
        int size = recvfrom(socketFd,
                                (char *)&packet,
                                sizeof(packet),
                                0,
                                (struct sockaddr *)&source,
                                &sourceLength);
        uint64_t now = GetTimeMs();

        if (size < 0)
        {
#ifdef _WIN32
            int error = WSAGetLastError();

            if (error == WSAEINTR || error == WSAECONNRESET)
                continue;
            fprintf(stderr, "recvfrom failed: Windows error %d\n", error);
#else
            if (errno == EINTR)
                continue;
            perror("recvfrom");
#endif
            break;
        }
        if (size == sizeof(packet)
         && ntohl(packet.magic) == PC_LINK_MAGIC
         && packet.version == PC_LINK_VERSION)
        {
            if (packet.type == PC_LINK_PACKET_HELLO)
                HandleHello(socketFd, &packet, &source, now);
            else if (packet.type == PC_LINK_PACKET_RELAY_PUNCH
                  || packet.type == PC_LINK_PACKET_RELAY_FRAME)
                HandleRelayPacket(socketFd, &packet, &source, now);
        }
        ExpireEntries(now);
    }

    PcCloseSocket(socketFd);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
