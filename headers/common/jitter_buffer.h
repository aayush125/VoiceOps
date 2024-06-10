#pragma once

#include <cinttypes>
#include <glib.h>

#define SLOT_SIZE 400 // each slot in jitter buffer will have 400 bytes
#define SLOT_COUNT 5

struct VoicePacketToServer {
    uint32_t packet_number;
    uint8_t encoded_data[SLOT_SIZE];
};

struct VoicePacketFromServer {
    uint32_t packet_number;
    uint32_t userID;
    uint8_t encoded_data[SLOT_SIZE];
};

class JitterBuffer {
private:
    uint8_t data[SLOT_COUNT][SLOT_SIZE];
    uint16_t data_sizes[SLOT_COUNT];
    uint32_t frame_number[SLOT_COUNT];

    uint8_t ready;
    uint8_t initial_index;

    GMutex lock;

    inline uint8_t get_min_index();
    inline int8_t get_min_nonzero_index();

public:
    JitterBuffer();
    ~JitterBuffer();

    void insert(VoicePacketToServer* newpkt, uint16_t size);
    bool get(VoicePacketToServer* pkt, uint16_t* size);
};
