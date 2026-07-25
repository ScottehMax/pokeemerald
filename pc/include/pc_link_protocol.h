#ifndef GUARD_PC_LINK_PROTOCOL_H
#define GUARD_PC_LINK_PROTOCOL_H

#include <stdint.h>

#define PC_LINK_MAGIC 0x50454C4Bu
#define PC_LINK_VERSION 4
#define PC_LINK_DEFAULT_PORT 8765
#define PC_LINK_CODE_LENGTH 8
#define PC_LINK_COMMAND_WORDS 8

enum PcLinkPacketType
{
    PC_LINK_PACKET_HELLO = 1,
    PC_LINK_PACKET_MATCH,
    PC_LINK_PACKET_PUNCH,
    PC_LINK_PACKET_FRAME,
    PC_LINK_PACKET_RELAY_PUNCH,
    PC_LINK_PACKET_RELAY_FRAME,
};

#pragma pack(push, 1)
struct PcLinkPacket
{
    uint32_t magic;
    uint8_t version;
    uint8_t type;
    uint8_t playerId;
    uint8_t codeLength;
    uint64_t nonce;
    uint64_t clientId;
    uint64_t session;
    uint32_t sequence;
    uint32_t acknowledgeSequence;
    uint32_t peerAddress;
    uint16_t peerPort;
    uint16_t commands[PC_LINK_COMMAND_WORDS];
    uint8_t code[PC_LINK_CODE_LENGTH];
};
#pragma pack(pop)

#endif
