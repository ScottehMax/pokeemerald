#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif
typedef SOCKET PcSocket;
#define PC_INVALID_SOCKET INVALID_SOCKET
#define PcCloseSocket closesocket
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
typedef int PcSocket;
#define PC_INVALID_SOCKET (-1)
#define PcCloseSocket close
#endif

#include "global.h"
#include "link.h"
#include "overworld.h"
#include "pc_link.h"
#include "pc_link_protocol.h"
#include "pc_platform.h"
#include "constants/characters.h"

_Static_assert(CMD_LENGTH == PC_LINK_COMMAND_WORDS, "PC link command size mismatch");

#define PC_LINK_RENDEZVOUS_INTERVAL_MS 500
#define PC_LINK_PUNCH_INTERVAL_MS 100
#define PC_LINK_DIRECT_TIMEOUT_MS 1200
#define PC_LINK_DIRECT_RETRY_MS 1000
#define PC_LINK_RETRY_INTERVAL_MS 50
#define PC_LINK_CONNECT_TIMEOUT_MS 15000
#define PC_LINK_PEER_TIMEOUT_MS 5000
#define PC_LINK_FRAME_CAPACITY 1024
#define PC_LINK_MAX_IN_FLIGHT 32
#define PC_LINK_COMMAND_QUEUE_CAPACITY 128

enum PcLinkState
{
    PC_LINK_STATE_IDLE,
    PC_LINK_STATE_RENDEZVOUS,
    PC_LINK_STATE_PUNCHING,
    PC_LINK_STATE_CONNECTED,
    PC_LINK_STATE_ERROR,
};

struct PcLinkFrame
{
    u32 sequence;
    u16 commands[CMD_LENGTH];
    bool8 occupied;
    bool8 delivered;
};

struct PcLinkClient
{
    PcSocket socket;
#ifdef _WIN32
    bool32 winsockInitialized;
#endif
    struct sockaddr_in server;
    struct sockaddr_in peer;
    enum PcLinkState state;
    u8 code[PC_LINK_CODE_LENGTH];
    u8 codeLength;
    u8 playerId;
    u64 nonce;
    u64 clientId;
    u64 session;
    u32 nextSendSequence;
    u32 nextReceiveSequence;
    u32 nextDeliverySequence;
    u32 acknowledgedSequence;
    struct PcLinkFrame localFrames[PC_LINK_FRAME_CAPACITY];
    struct PcLinkFrame remoteFrames[PC_LINK_FRAME_CAPACITY];
    u16 commandQueue[PC_LINK_COMMAND_QUEUE_CAPACITY][CMD_LENGTH];
    u16 commandQueuePosition;
    u16 commandQueueCount;
    u16 latestKeyCommands[CMD_LENGTH];
    bool8 haveLatestKeyCommands;
    u64 stateStartedAt;
    u64 lastServerSend;
    u64 lastPunchSend;
    u64 lastDirectProbeSend;
    u64 lastRetrySend;
    u64 lastPeerReceive;
    bool8 sendErrorReported;
    bool8 usingRelay;
    bool8 directEnabled;
};

static struct PcLinkClient sLink = {.socket = PC_INVALID_SOCKET};

static u64 HostToNetwork64(u64 value)
{
    u32 high = htonl((u32)(value >> 32));
    u32 low = htonl((u32)value);

    return ((u64)low << 32) | high;
}

static u64 NetworkToHost64(u64 value)
{
    return HostToNetwork64(value);
}

static u64 GetTimeMs(void)
{
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);
    return (u64)now.tv_sec * 1000 + now.tv_nsec / 1000000;
#endif
}

static u64 MakeNonce(void)
{
    u64 value = GetTimeMs() ^ (u64)(uintptr_t)&sLink;

#ifdef _WIN32
    value ^= (u64)GetCurrentProcessId() << 32;
#else
    value ^= (u64)getpid() << 32;
#endif
    value ^= value << 13;
    value ^= value >> 7;
    value ^= value << 17;
    return value ? value : 1;
}

static bool32 DirectLinkEnabled(void)
{
    const char *setting = getenv("POKEEMERALD_LINK_P2P");

    return setting != NULL
        && (strcmp(setting, "1") == 0
         || strcmp(setting, "true") == 0
         || strcmp(setting, "yes") == 0);
}

static bool32 SetNonblocking(PcSocket socket)
{
#ifdef _WIN32
    u_long enabled = 1;

    return ioctlsocket(socket, FIONBIO, &enabled) == 0;
#else
    int flags = fcntl(socket, F_GETFL, 0);

    return flags >= 0 && fcntl(socket, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

static int GetSocketError(void)
{
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

static bool32 WouldBlock(int error)
{
#ifdef _WIN32
    return error == WSAEWOULDBLOCK;
#else
    return error == EAGAIN || error == EWOULDBLOCK;
#endif
}

static bool32 IsTransientUdpError(int error)
{
#ifdef _WIN32
    return error == WSAECONNRESET || error == WSAECONNREFUSED || error == WSAEINTR;
#else
    return error == ECONNRESET || error == ECONNREFUSED || error == EINTR;
#endif
}

static bool32 ParseServerAddress(struct sockaddr_in *address)
{
    const char *setting = PcPlatformGetLinkServer();
    const char *host = "127.0.0.1";
    const char *port = "8765";
    char hostBuffer[256];
    struct addrinfo hints;
    struct addrinfo *result;
    const char *colon;
    int status;

    if (setting != NULL && *setting != '\0')
    {
        colon = strrchr(setting, ':');
        if (colon != NULL)
        {
            size_t hostLength = colon - setting;

            if (hostLength == 0 || hostLength >= sizeof(hostBuffer) || colon[1] == '\0')
                return FALSE;
            memcpy(hostBuffer, setting, hostLength);
            hostBuffer[hostLength] = '\0';
            host = hostBuffer;
            port = colon + 1;
        }
        else
        {
            host = setting;
        }
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    status = getaddrinfo(host, port, &hints, &result);
    if (status != 0)
    {
        fprintf(stderr, "could not resolve link server %s:%s: %s\n", host, port, gai_strerror(status));
        return FALSE;
    }
    memcpy(address, result->ai_addr, sizeof(*address));
    freeaddrinfo(result);
    return TRUE;
}

static void InitializePacket(struct PcLinkPacket *packet, u8 type)
{
    memset(packet, 0, sizeof(*packet));
    packet->magic = htonl(PC_LINK_MAGIC);
    packet->version = PC_LINK_VERSION;
    packet->type = type;
    packet->playerId = sLink.playerId;
    packet->nonce = HostToNetwork64(sLink.nonce);
    packet->clientId = HostToNetwork64(sLink.clientId);
    packet->session = HostToNetwork64(sLink.session);
}

static bool32 SendPacketTo(const struct PcLinkPacket *packet, const struct sockaddr_in *address)
{
    int sent = sendto(sLink.socket,
                      (const char *)packet,
                      sizeof(*packet),
                      0,
                      (const struct sockaddr *)address,
                      sizeof(*address));

    if (sent != sizeof(*packet))
    {
        if (!sLink.sendErrorReported)
        {
            char addressText[INET_ADDRSTRLEN];
            int error = GetSocketError();

            inet_ntop(AF_INET, &address->sin_addr, addressText, sizeof(addressText));
#ifdef _WIN32
            fprintf(stderr,
                    "PC link sendto %s:%u failed: Windows error %d\n",
                    addressText,
                    ntohs(address->sin_port),
                    error);
#else
            fprintf(stderr,
                    "PC link sendto %s:%u failed: %s\n",
                    addressText,
                    ntohs(address->sin_port),
                    strerror(error));
#endif
            sLink.sendErrorReported = TRUE;
        }
        return FALSE;
    }
    return TRUE;
}

static void SendHello(u64 now)
{
    struct PcLinkPacket packet;

    InitializePacket(&packet, PC_LINK_PACKET_HELLO);
    packet.codeLength = sLink.codeLength;
    memcpy(packet.code, sLink.code, sLink.codeLength);
    SendPacketTo(&packet, &sLink.server);
    sLink.lastServerSend = now;
}

static void SendDirectPunch(u64 now)
{
    struct PcLinkPacket packet;

    InitializePacket(&packet, PC_LINK_PACKET_PUNCH);
    SendPacketTo(&packet, &sLink.peer);
    sLink.lastDirectProbeSend = now;
}

static void SendPunch(u64 now)
{
    struct PcLinkPacket packet;

    if (sLink.usingRelay)
    {
        InitializePacket(&packet, PC_LINK_PACKET_RELAY_PUNCH);
        SendPacketTo(&packet, &sLink.server);
    }
    else
    {
        SendDirectPunch(now);
    }
    sLink.lastPunchSend = now;
}

static void EncodeCommands(struct PcLinkPacket *packet, const u16 *commands)
{
    u32 i;

    for (i = 0; i < CMD_LENGTH; i++)
        packet->commands[i] = htons(commands[i]);
}

static void DecodeCommands(u16 *commands, const struct PcLinkPacket *packet)
{
    u32 i;

    for (i = 0; i < CMD_LENGTH; i++)
        commands[i] = ntohs(packet->commands[i]);
}

static void SendFrame(const struct PcLinkFrame *frame)
{
    struct PcLinkPacket packet;

    InitializePacket(&packet,
                     sLink.usingRelay
                         ? PC_LINK_PACKET_RELAY_FRAME
                         : PC_LINK_PACKET_FRAME);
    packet.sequence = htonl(frame->sequence);
    packet.acknowledgeSequence = htonl(sLink.nextReceiveSequence - 1);
    EncodeCommands(&packet, frame->commands);
    SendPacketTo(&packet, sLink.usingRelay ? &sLink.server : &sLink.peer);
}

static void AcknowledgeLocalFrames(u32 sequence)
{
    u32 current;

    if (sequence <= sLink.acknowledgedSequence || sequence >= sLink.nextSendSequence)
        return;

    for (current = sLink.acknowledgedSequence + 1; current <= sequence; current++)
    {
        struct PcLinkFrame *frame = &sLink.localFrames[current % PC_LINK_FRAME_CAPACITY];

        if (frame->occupied && frame->sequence == current && frame->delivered)
            frame->occupied = FALSE;
    }
    sLink.acknowledgedSequence = sequence;
}

static bool32 AddressesEqual(const struct sockaddr_in *a, const struct sockaddr_in *b)
{
    return a->sin_family == b->sin_family
        && a->sin_addr.s_addr == b->sin_addr.s_addr
        && a->sin_port == b->sin_port;
}

static bool32 ValidatePacket(const struct PcLinkPacket *packet, int size)
{
    return size == sizeof(*packet)
        && ntohl(packet->magic) == PC_LINK_MAGIC
        && packet->version == PC_LINK_VERSION;
}

static bool32 CommandsAreEmpty(const u16 *commands)
{
    u32 i;

    for (i = 0; i < CMD_LENGTH; i++)
    {
        if (commands[i] != 0)
            return FALSE;
    }
    return TRUE;
}

static bool32 KeyCommandsCanBeCoalesced(const u16 *commands)
{
    if (commands[0] == LINKCMD_BLENDER_SEND_KEYS)
        return TRUE;
    if (commands[0] != LINKCMD_SEND_HELD_KEYS)
        return FALSE;

    switch (commands[1])
    {
    case LINK_KEY_CODE_NULL:
    case LINK_KEY_CODE_EMPTY:
    case LINK_KEY_CODE_DPAD_DOWN:
    case LINK_KEY_CODE_DPAD_UP:
    case LINK_KEY_CODE_DPAD_LEFT:
    case LINK_KEY_CODE_DPAD_RIGHT:
        return TRUE;
    default:
        return FALSE;
    }
}

static void QueueGameCommands(const u16 *commands)
{
    u32 position;

    if (CommandsAreEmpty(commands))
        return;
    // Continuous input snapshots may supersede one another. Control key codes such as
    // READY and EXIT_ROOM are transitions and must remain ordered and reliable.
    if (KeyCommandsCanBeCoalesced(commands))
    {
        memcpy(sLink.latestKeyCommands, commands, sizeof(sLink.latestKeyCommands));
        sLink.haveLatestKeyCommands = TRUE;
        return;
    }
    // Do not emit an older movement snapshot after a later reliable transition.
    sLink.haveLatestKeyCommands = FALSE;
    if (sLink.commandQueueCount == PC_LINK_COMMAND_QUEUE_CAPACITY)
    {
        fprintf(stderr, "PC link command queue overflow\n");
        sLink.state = PC_LINK_STATE_ERROR;
        return;
    }
    position = (sLink.commandQueuePosition + sLink.commandQueueCount)
             % PC_LINK_COMMAND_QUEUE_CAPACITY;
    memcpy(sLink.commandQueue[position], commands, sizeof(sLink.commandQueue[position]));
    sLink.commandQueueCount++;
}

static void PopGameCommands(u16 *commands)
{
    if (sLink.commandQueueCount != 0)
    {
        memcpy(commands,
               sLink.commandQueue[sLink.commandQueuePosition],
               sizeof(sLink.commandQueue[sLink.commandQueuePosition]));
        sLink.commandQueuePosition = (sLink.commandQueuePosition + 1)
                                   % PC_LINK_COMMAND_QUEUE_CAPACITY;
        sLink.commandQueueCount--;
    }
    else if (sLink.haveLatestKeyCommands)
    {
        memcpy(commands, sLink.latestKeyCommands, sizeof(sLink.latestKeyCommands));
        sLink.haveLatestKeyCommands = FALSE;
    }
    else
    {
        memset(commands, 0, sizeof(u16) * CMD_LENGTH);
    }
}

static bool32 QueueLocalFrame(const u16 *commands)
{
    struct PcLinkFrame *frame = &sLink.localFrames[sLink.nextSendSequence % PC_LINK_FRAME_CAPACITY];

    if (frame->occupied)
    {
        fprintf(stderr, "PC link frame history overflow\n");
        sLink.state = PC_LINK_STATE_ERROR;
        return FALSE;
    }
    frame->sequence = sLink.nextSendSequence++;
    memcpy(frame->commands, commands, sizeof(frame->commands));
    frame->occupied = TRUE;
    frame->delivered = FALSE;
    SendFrame(frame);
    return TRUE;
}

static void StoreRemoteFrame(const struct PcLinkPacket *packet)
{
    u32 sequence = ntohl(packet->sequence);
    struct PcLinkFrame *frame;

    AcknowledgeLocalFrames(ntohl(packet->acknowledgeSequence));
    if (sequence < sLink.nextDeliverySequence
     || sequence >= sLink.nextDeliverySequence + PC_LINK_FRAME_CAPACITY)
        return;

    frame = &sLink.remoteFrames[sequence % PC_LINK_FRAME_CAPACITY];
    if (!frame->occupied)
    {
        frame->sequence = sequence;
        DecodeCommands(frame->commands, packet);
        frame->occupied = TRUE;
    }

    while (sLink.nextReceiveSequence < sLink.nextDeliverySequence + PC_LINK_FRAME_CAPACITY)
    {
        frame = &sLink.remoteFrames[sLink.nextReceiveSequence % PC_LINK_FRAME_CAPACITY];
        if (!frame->occupied || frame->sequence != sLink.nextReceiveSequence)
            break;
        sLink.nextReceiveSequence++;
    }
}

static bool32 DeliverFrame(u16 (*recvCmds)[CMD_LENGTH])
{
    u32 sequence = sLink.nextDeliverySequence;
    struct PcLinkFrame *local = &sLink.localFrames[sequence % PC_LINK_FRAME_CAPACITY];
    struct PcLinkFrame *remote = &sLink.remoteFrames[sequence % PC_LINK_FRAME_CAPACITY];

    if (!local->occupied
     || local->sequence != sequence
     || !remote->occupied
     || remote->sequence != sequence)
        return FALSE;

    memcpy(recvCmds[sLink.playerId], local->commands, sizeof(local->commands));
    memcpy(recvCmds[sLink.playerId ^ 1], remote->commands, sizeof(remote->commands));
    local->delivered = TRUE;
    if (sequence <= sLink.acknowledgedSequence)
        local->occupied = FALSE;
    remote->occupied = FALSE;
    sLink.nextDeliverySequence++;
    return TRUE;
}

static void RetryOldestUnacknowledgedFrame(u64 now)
{
    u32 sequence = sLink.acknowledgedSequence + 1;
    struct PcLinkFrame *frame;

    if (sequence >= sLink.nextSendSequence
     || now - sLink.lastRetrySend < PC_LINK_RETRY_INTERVAL_MS)
        return;
    frame = &sLink.localFrames[sequence % PC_LINK_FRAME_CAPACITY];
    if (frame->occupied && frame->sequence == sequence)
        SendFrame(frame);
    sLink.lastRetrySend = now;
}

static void ReceivePackets(u64 now)
{
    for (;;)
    {
        struct PcLinkPacket packet;
        struct sockaddr_in source;
        socklen_t sourceLength = sizeof(source);
        int size = recvfrom(sLink.socket,
                            (char *)&packet,
                            sizeof(packet),
                            0,
                            (struct sockaddr *)&source,
                            &sourceLength);

        if (size < 0)
        {
            int error = GetSocketError();

            if (WouldBlock(error))
                return;
            // Closing and reopening the peer's UDP socket can produce an ICMP port
            // unreachable for an in-flight packet. It says nothing about the new
            // rendezvous attempt and must not abort the link.
            if (IsTransientUdpError(error))
                continue;
#ifdef _WIN32
            fprintf(stderr, "PC link recvfrom failed: Windows error %d\n", error);
#else
            fprintf(stderr, "PC link recvfrom failed: %s\n", strerror(error));
#endif
            sLink.state = PC_LINK_STATE_ERROR;
            return;
        }
        if (!ValidatePacket(&packet, size))
            continue;

        if ((sLink.state == PC_LINK_STATE_RENDEZVOUS
          || sLink.state == PC_LINK_STATE_PUNCHING)
         && packet.type == PC_LINK_PACKET_MATCH
         && NetworkToHost64(packet.nonce) == sLink.nonce)
        {
            bool32 firstMatch = sLink.state == PC_LINK_STATE_RENDEZVOUS;
            bool32 endpointChanged;
            bool32 sessionChanged;
            u64 session = NetworkToHost64(packet.session);

            // A UDP reply may come from a different local interface than the
            // configured or DNS-resolved rendezvous address. The random
            // per-attempt nonce authenticates the reply; retain its actual
            // source so subsequent HELLO and relay packets follow it.
            sLink.server = source;
            sessionChanged = sLink.session != 0 && sLink.session != session;
            sLink.session = session;
            sLink.playerId = packet.playerId;
            endpointChanged = sLink.peer.sin_addr.s_addr != packet.peerAddress
                           || sLink.peer.sin_port != packet.peerPort;
            if (firstMatch || endpointChanged || sessionChanged)
            {
                char peerAddress[INET_ADDRSTRLEN];

                memset(&sLink.peer, 0, sizeof(sLink.peer));
                sLink.peer.sin_family = AF_INET;
                sLink.peer.sin_addr.s_addr = packet.peerAddress;
                sLink.peer.sin_port = packet.peerPort;
                inet_ntop(AF_INET,
                          &sLink.peer.sin_addr,
                          peerAddress,
                          sizeof(peerAddress));
                if (sLink.directEnabled)
                {
                    fprintf(stderr,
                            "PC link %s as player %u; punching %s:%u directly\n",
                            sessionChanged ? "rematched" : "matched",
                            sLink.playerId + 1,
                            peerAddress,
                            ntohs(sLink.peer.sin_port));
                }
                else
                {
                    fprintf(stderr,
                            "PC link %s as player %u; using server relay\n",
                            sessionChanged ? "rematched" : "matched",
                            sLink.playerId + 1);
                }
                sLink.usingRelay = !sLink.directEnabled;
            }
            if (firstMatch || sessionChanged)
                sLink.stateStartedAt = now;
            sLink.state = PC_LINK_STATE_PUNCHING;
            sLink.lastPunchSend = 0;
            continue;
        }

        if (sLink.directEnabled
         && packet.type == PC_LINK_PACKET_PUNCH
         && sLink.session != 0
         && NetworkToHost64(packet.session) == sLink.session
         && packet.playerId != sLink.playerId)
        {
            bool32 shouldReply = sLink.state != PC_LINK_STATE_CONNECTED || sLink.usingRelay;

            sLink.peer = source;
            sLink.lastPeerReceive = now;
            if (sLink.usingRelay)
                fprintf(stderr, "PC link upgraded from relay to direct P2P\n");
            sLink.usingRelay = FALSE;
            if (shouldReply)
                SendPunch(now);
            if (sLink.state != PC_LINK_STATE_CONNECTED)
            {
                sLink.state = PC_LINK_STATE_CONNECTED;
                fprintf(stderr,
                        "PC link peer connected directly as player %u\n",
                        sLink.playerId + 1);
            }
            continue;
        }

        if (packet.type == PC_LINK_PACKET_RELAY_PUNCH
         && sLink.session != 0
         && NetworkToHost64(packet.session) == sLink.session
         && packet.playerId != sLink.playerId
         && AddressesEqual(&source, &sLink.server))
        {
            bool32 shouldReply = sLink.state != PC_LINK_STATE_CONNECTED || !sLink.usingRelay;

            sLink.lastPeerReceive = now;
            sLink.usingRelay = TRUE;
            if (shouldReply)
                SendPunch(now);
            if (sLink.state != PC_LINK_STATE_CONNECTED)
            {
                sLink.state = PC_LINK_STATE_CONNECTED;
                fprintf(stderr,
                        "PC link peer connected through relay as player %u\n",
                        sLink.playerId + 1);
            }
            continue;
        }

        if (sLink.state != PC_LINK_STATE_CONNECTED
         || NetworkToHost64(packet.session) != sLink.session
         || packet.playerId == sLink.playerId)
            continue;

        if (packet.type == PC_LINK_PACKET_FRAME
         && !sLink.usingRelay
         && AddressesEqual(&source, &sLink.peer))
        {
            sLink.lastPeerReceive = now;
            StoreRemoteFrame(&packet);
        }
        else if (packet.type == PC_LINK_PACKET_RELAY_FRAME
              && sLink.usingRelay
              && AddressesEqual(&source, &sLink.server))
        {
            sLink.lastPeerReceive = now;
            StoreRemoteFrame(&packet);
        }
    }
}

void PcLinkSetCode(const u8 *code)
{
    u32 length = 0;

    memset(sLink.code, 0, sizeof(sLink.code));
    if (code != NULL)
    {
        while (length < sizeof(sLink.code) && code[length] != EOS)
        {
            sLink.code[length] = code[length];
            if (sLink.code[length] >= CHAR_a && sLink.code[length] <= CHAR_z)
                sLink.code[length] += CHAR_A - CHAR_a;
            length++;
        }
    }
    sLink.codeLength = length;
}

bool32 PcLinkOpen(void)
{
    struct sockaddr_in localAddress;
    u64 now;

    PcLinkClose();
    if (sLink.codeLength == 0)
    {
        fprintf(stderr, "a connect code is required for PC link\n");
        sLink.state = PC_LINK_STATE_ERROR;
        return FALSE;
    }
#ifdef _WIN32
    {
        WSADATA data;

        if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
        {
            fprintf(stderr, "could not initialize Winsock for PC link\n");
            sLink.state = PC_LINK_STATE_ERROR;
            return FALSE;
        }
        sLink.winsockInitialized = TRUE;
    }
#endif
    if (!ParseServerAddress(&sLink.server))
    {
        fprintf(stderr, "a valid POKEEMERALD_LINK_SERVER is required for PC link\n");
        PcLinkClose();
        sLink.state = PC_LINK_STATE_ERROR;
        return FALSE;
    }
    sLink.socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sLink.socket == PC_INVALID_SOCKET)
    {
        fprintf(stderr, "could not create PC link UDP socket\n");
        PcLinkClose();
        sLink.state = PC_LINK_STATE_ERROR;
        return FALSE;
    }
    memset(&localAddress, 0, sizeof(localAddress));
    localAddress.sin_family = AF_INET;
    localAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    localAddress.sin_port = 0;
    if (bind(sLink.socket,
             (const struct sockaddr *)&localAddress,
             sizeof(localAddress)) != 0
     || !SetNonblocking(sLink.socket))
    {
        fprintf(stderr, "could not bind PC link UDP socket\n");
        PcLinkClose();
        sLink.state = PC_LINK_STATE_ERROR;
        return FALSE;
    }
#ifdef _WIN32
    {
        BOOL enableConnectionReset = FALSE;
        DWORD bytesReturned;

        // Windows otherwise turns an ICMP response to an unconnected UDP send into
        // WSAECONNRESET on a later recvfrom call.
        WSAIoctl(sLink.socket,
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

    now = GetTimeMs();
    if (sLink.clientId == 0)
        sLink.clientId = MakeNonce();
    sLink.nonce = MakeNonce();
    sLink.session = 0;
    sLink.playerId = 0;
    sLink.nextSendSequence = 1;
    sLink.nextReceiveSequence = 1;
    sLink.nextDeliverySequence = 1;
    sLink.acknowledgedSequence = 0;
    memset(sLink.localFrames, 0, sizeof(sLink.localFrames));
    memset(sLink.remoteFrames, 0, sizeof(sLink.remoteFrames));
    sLink.commandQueuePosition = 0;
    sLink.commandQueueCount = 0;
    sLink.haveLatestKeyCommands = FALSE;
    sLink.stateStartedAt = now;
    sLink.lastServerSend = 0;
    sLink.lastPunchSend = 0;
    sLink.lastDirectProbeSend = 0;
    sLink.lastRetrySend = now;
    sLink.lastPeerReceive = now;
    sLink.sendErrorReported = FALSE;
    sLink.directEnabled = DirectLinkEnabled();
    sLink.usingRelay = !sLink.directEnabled;
    sLink.state = PC_LINK_STATE_RENDEZVOUS;
    SendHello(now);
    return TRUE;
}

void PcLinkClose(void)
{
    if (sLink.socket != PC_INVALID_SOCKET)
    {
        PcCloseSocket(sLink.socket);
        sLink.socket = PC_INVALID_SOCKET;
    }
#ifdef _WIN32
    if (sLink.winsockInitialized)
    {
        WSACleanup();
        sLink.winsockInitialized = FALSE;
    }
#endif
    sLink.state = PC_LINK_STATE_IDLE;
    sLink.session = 0;
}

u32 PcLinkMain(u16 *sendCmd, u16 (*recvCmds)[CMD_LENGTH])
{
    u64 now = GetTimeMs();
    u32 status;
    u16 frameCommands[CMD_LENGTH];
    bool32 receivedFrame = FALSE;

    // Local minigames use gRecvCmds to stage simulated player input.
    if (sLink.state == PC_LINK_STATE_IDLE)
        return 0;
    memset(recvCmds, 0, sizeof(u16) * MAX_RFU_PLAYERS * CMD_LENGTH);
    if (sLink.state != PC_LINK_STATE_ERROR)
        ReceivePackets(now);

    if ((sLink.state == PC_LINK_STATE_RENDEZVOUS || sLink.state == PC_LINK_STATE_PUNCHING)
     && now - sLink.lastServerSend >= PC_LINK_RENDEZVOUS_INTERVAL_MS)
        SendHello(now);
    if (sLink.state == PC_LINK_STATE_PUNCHING
     && sLink.directEnabled
     && !sLink.usingRelay
     && now - sLink.stateStartedAt >= PC_LINK_DIRECT_TIMEOUT_MS)
    {
        sLink.usingRelay = TRUE;
        sLink.lastPunchSend = 0;
        fprintf(stderr, "PC link direct path unavailable; trying relay\n");
    }
    if (sLink.state == PC_LINK_STATE_PUNCHING
     && now - sLink.lastPunchSend >= PC_LINK_PUNCH_INTERVAL_MS)
        SendPunch(now);

    if (sLink.state == PC_LINK_STATE_PUNCHING
     && now - sLink.stateStartedAt >= PC_LINK_CONNECT_TIMEOUT_MS)
    {
        fprintf(stderr, "PC link connection timed out\n");
        sLink.state = PC_LINK_STATE_ERROR;
    }
    if (sLink.state == PC_LINK_STATE_CONNECTED
     && now - sLink.lastPeerReceive >= PC_LINK_PEER_TIMEOUT_MS)
    {
        fprintf(stderr, "PC link peer timed out\n");
        sLink.state = PC_LINK_STATE_ERROR;
    }

    if (sLink.state == PC_LINK_STATE_CONNECTED)
    {
        if (sLink.directEnabled
         && sLink.usingRelay
         && now - sLink.lastDirectProbeSend >= PC_LINK_DIRECT_RETRY_MS)
            SendDirectPunch(now);
        QueueGameCommands(sendCmd);
        if (sLink.state == PC_LINK_STATE_CONNECTED
         && sLink.nextSendSequence - sLink.nextDeliverySequence < PC_LINK_MAX_IN_FLIGHT)
        {
            PopGameCommands(frameCommands);
            QueueLocalFrame(frameCommands);
        }
        receivedFrame = DeliverFrame(recvCmds);
        RetryOldestUnacknowledgedFrame(now);
    }
    memset(sendCmd, 0, sizeof(u16) * CMD_LENGTH);

    status = sLink.playerId;
    if (sLink.state == PC_LINK_STATE_CONNECTED)
    {
        status |= 2 << LINK_STAT_PLAYER_COUNT_SHIFT;
        status |= LINK_STAT_CONN_ESTABLISHED;
        if (sLink.playerId == 0)
            status |= LINK_STAT_MASTER;
        if (!receivedFrame)
        {
            status |= LINK_STAT_RECEIVED_NOTHING;
        }
    }
    else
    {
        status |= 1 << LINK_STAT_PLAYER_COUNT_SHIFT;
    }
    if (sLink.state == PC_LINK_STATE_ERROR)
        status |= LINK_STAT_ERROR_HARDWARE;
    return status;
}

u8 PcLinkGetId(void)
{
    return sLink.playerId;
}

bool32 PcLinkHasError(void)
{
    return sLink.state == PC_LINK_STATE_ERROR;
}
