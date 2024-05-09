#pragma once
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <miniaudio.h>
#include <stdbool.h>

#define SLOT_SIZE 400 // each slot in jitter buffer will have 400 bytes
#define SLOT_COUNT 5

struct packet {
  uint32_t packet_number;
  uint8_t encoded_data[SLOT_SIZE];
};

typedef struct jitterbuffer {
  uint8_t data[SLOT_COUNT][SLOT_SIZE];
  uint16_t data_sizes[SLOT_COUNT];
  uint32_t frame_number[SLOT_COUNT];

  uint8_t ready;
  uint8_t initial_index;

  ma_spinlock lock;
} jitterbuffer;

// Allocates the actual buffer data
void jb_initialize(jitterbuffer *buf) {
  // buf->data = malloc(SLOT_SIZE * SLOT_COUNT);

  for (int i = 0; i < SLOT_COUNT; i++) {
    buf->data_sizes[i] = 0;
    buf->frame_number[i] = 0;
  }

  buf->initial_index = 0;
  buf->ready = 0;
}

inline uint8_t jb_get_min_index(jitterbuffer *buf) {
  uint8_t min_index = 0;

  for (int i = 1; i < SLOT_COUNT; i++) {
    if (buf->frame_number[i] < buf->frame_number[min_index]) {
      min_index = i;
    }
  }
  
  return min_index;
}

// Returns -1 if no non-zero index is found
inline int8_t jb_get_min_nonzero_index(jitterbuffer *buf) {
  int8_t min_index = -1;

  uint32_t min_frame = -1; // -1 but uint = largest uint

  for (int i = 0; i < SLOT_COUNT; i++) {
    if (buf->frame_number[i] == 0) {
      continue;
    }

    if (buf->frame_number[i] < min_frame) {
      min_index = i;
      min_frame = buf->frame_number[i];
    }
  }
  
  return min_index;
}

// uint8_t jb_get_max_index(jitterbuffer *buf) {
//   // stub
//   return 0;
// }

void jb_insert(jitterbuffer *buf, struct packet *newpkt, uint16_t size) {
  ma_spinlock_lock(buf->lock);

  if (!buf->ready) {
    buf->frame_number[buf->initial_index] = newpkt->packet_number;
    memcpy(buf->data[buf->initial_index], newpkt->encoded_data, size);
    buf->data_sizes[buf->initial_index] = size;

    buf->initial_index++;

    if (buf->initial_index >= SLOT_COUNT) {
      buf->ready = 1;
      printf("THE BUFFER IS READY!\n");
    }

    ma_spinlock_unlock(buf->lock);
    return;
  }

  uint8_t min_index = jb_get_min_index(buf);
  buf->frame_number[min_index] = newpkt->packet_number;
  memcpy(buf->data[min_index], newpkt->encoded_data, size);
  buf->data_sizes[min_index] = size;

  ma_spinlock_unlock(buf->lock);
}

bool jb_get(jitterbuffer *buf, struct packet *pkt, uint16_t *size) {
  ma_spinlock_lock(buf->lock);

  if (!buf->ready) {
    // The buffer is not ready yet.
    ma_spinlock_unlock(buf->lock);
    return false;
  }

  int8_t index = jb_get_min_nonzero_index(buf);
  
  if (index < 0) {
    printf("Jitter buffer is empty!\n");
    
    ma_spinlock_unlock(buf->lock);
    return false;
  }
  
  memcpy(pkt->encoded_data, buf->data[index], buf->data_sizes[index]);
  pkt->packet_number = buf->frame_number[index];
  *size = buf->data_sizes[index];

  // Mark packet
  buf->frame_number[index] = 0;

  ma_spinlock_unlock(buf->lock);
  return true;
}

void jb_terminate(jitterbuffer *buf) {
  free(buf->data);
}
