#pragma once

#include <cinttypes>
#define MAX_PACKET_SIZE 1500

enum PacketType {
    PACKET_TYPE_STRING = 1,
    PACKET_TYPE_TEXT = 2,
    PACKET_TYPE_PICTURE = 3
};

struct Packet {
    uint32_t packetType;
    uint32_t length;
    char data[MAX_PACKET_SIZE];
};