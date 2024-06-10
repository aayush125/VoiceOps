#pragma once

#include <cinttypes>
#define MAX_PACKET_SIZE 1500

enum PacketType: uint32_t {
    PACKET_TYPE_STRING = 1,
    PACKET_TYPE_TEXT = 2,
    PACKET_TYPE_PICTURE = 3,
    PACKET_TYPE_VOICE_JOIN, // Client -> Server
    PACKET_TYPE_VOICE_LEAVE, // Client -> Server
    PACKET_TYPE_VOICE_STATUS // Server -> all Clients
};

struct Packet {
    PacketType packetType;
    uint32_t length;
    char data[MAX_PACKET_SIZE];
};

struct AuthPacket {
    char username[50];
    char password[50];
    uint32_t uLength;
    uint32_t pLength;
};
