#pragma once

#include <cinttypes>
#define MAX_PACKET_SIZE 1500

enum PacketType: uint32_t {
    PACKET_TYPE_MSG_TO_SERVER = 1,
    PACKET_TYPE_MSG_FROM_SERVER = 2,
    PACKET_TYPE_PICTURE = 3,
    PACKET_TYPE_VOICE_JOIN, // Client -> Server
    PACKET_TYPE_VOICE_LEAVE, // Client -> Server
    PACKET_TYPE_VOICE_STATUS // Server -> all Clients
};

struct Packet {
    PacketType packetType;
    uint32_t length;
    union {
        char bytes[MAX_PACKET_SIZE];
        char message_to_server[MAX_PACKET_SIZE];
        struct {
            char username[50];
            char text[MAX_PACKET_SIZE - 50];
        } message_from_server;
    } data;
};

struct AuthPacket {
    char username[50];
    char password[50];
    uint32_t uLength;
    uint32_t pLength;
};
