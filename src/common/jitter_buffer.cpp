#include <string>
#include <iostream>
#include <common/jitter_buffer.h>

JitterBuffer::JitterBuffer() {
    // buf->data = malloc(SLOT_SIZE * SLOT_COUNT);

    for (int i = 0; i < SLOT_COUNT; i++) {
        this->data_sizes[i] = 0;
        this->frame_number[i] = 0;
    }

    this->initial_index = 0;
    this->ready = 0;
    this->empty_counter = 0;

    g_mutex_init(&this->lock);
}

JitterBuffer::~JitterBuffer() {
    g_mutex_clear(&this->lock);
}

inline uint8_t JitterBuffer::get_min_index() {
    uint8_t min_index = 0;

    for (int i = 1; i < SLOT_COUNT; i++) {
        if (this->frame_number[i] < this->frame_number[min_index]) {
            min_index = i;
        }
    }

    return min_index;
}

// Returns -1 if no non-zero index is found
inline int8_t JitterBuffer::get_min_nonzero_index() {
    int8_t min_index = -1;

    uint32_t min_frame = -1; // -1 but uint = largest uint

    for (int i = 0; i < SLOT_COUNT; i++) {
        if (this->frame_number[i] == 0) {
            continue;
        }

        if (this->frame_number[i] < min_frame) {
            min_index = i;
            min_frame = this->frame_number[i];
        }
    }

    return min_index;
}

void JitterBuffer::insert(VoicePacketToServer* newpkt, uint16_t size) {
    g_mutex_lock(&this->lock);

    if (!this->ready) {
        this->frame_number[this->initial_index] = newpkt->packet_number;
        memcpy(this->data[this->initial_index], newpkt->encoded_data, size);
        this->data_sizes[this->initial_index] = size;

        this->initial_index++;

        if (this->initial_index >= SLOT_COUNT) {
            this->ready = 1;
            std::cout << "THE BUFFER IS READY!" << std::endl;
        }

        g_mutex_unlock(&this->lock);
        return;
    }

    uint8_t min_index = this->get_min_index();
    this->frame_number[min_index] = newpkt->packet_number;
    memcpy(this->data[min_index], newpkt->encoded_data, size);
    this->data_sizes[min_index] = size;

    g_mutex_unlock(&this->lock);
}

bool JitterBuffer::get(VoicePacketToServer* pkt, uint16_t* size) {
    g_mutex_lock(&this->lock);

    if (!this->ready) {
        // The buffer is not ready yet.
        g_mutex_unlock(&this->lock);
        return false;
    }

    int8_t index = this->get_min_nonzero_index();

    if (index < 0) {
        // Jitter buffer is empty
        // std::cout << "Jitter buffer is empty!" << std::endl;
        this->empty_counter++;

        // After 1s of silence or no data, reset the entire jitter buffer
        if (this->empty_counter > 100) {
            this->initial_index = 0;
            this->ready = 0;
            this->empty_counter = 0;
        }

        
        g_mutex_unlock(&this->lock);
        return false;
    }

    memcpy(pkt->encoded_data, this->data[index], this->data_sizes[index]);
    pkt->packet_number = this->frame_number[index];
    *size = this->data_sizes[index];

    // Mark packet
    this->frame_number[index] = 0;

    g_mutex_unlock(&this->lock);
    return true;
}
