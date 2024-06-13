#pragma once

#include <cinttypes>
#define MAX_PACKET_SIZE 1400

#define min(a,b) (a < b ? a : b)

enum PacketType: uint32_t {
    PACKET_TYPE_MSG_TO_SERVER = 1,
    PACKET_TYPE_MSG_FROM_SERVER = 2,
    PACKET_TYPE_IMAGE = 3,
    PACKET_TYPE_IMAGE_FROM_SERVER_FIRST_PACKET,
    PACKET_TYPE_AUTH_REQUEST,
    PACKET_TYPE_AUTH_RESPONSE,
    PACKET_TYPE_VOICE_JOIN, // Client -> Server
    PACKET_TYPE_VOICE_LEAVE, // Client -> Server
    PACKET_TYPE_VOICE_STATUS // Server -> all Clients
};

struct Packet {
    PacketType packetType;
    uint32_t length;
    union {
        char image[MAX_PACKET_SIZE];
        char image_sender[50];
        char message_to_server[MAX_PACKET_SIZE];
        struct {
            char username[50];
            char text[MAX_PACKET_SIZE - 50];
        } message_from_server;
        struct {
            uint32_t uLength;
            uint32_t pLength;
            char username[50];
            char password[50];
        } auth_request;
        uint8_t auth_response; // Only 1 or 0
    } data;
};

enum ReceiveResult: uint8_t {
    RECEIVE_RESULT_SUCCESS,
    RECEIVE_RESULT_CONN_CLOSED,
    RECEIVE_RESULT_ERROR
};

ReceiveResult recv_pkt(uint64_t socket, Packet& pkt);
