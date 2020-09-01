#ifndef RINGBUFFER_H_
#define RINGBUFFER_H_

#include "stdint_fast.h"

#define RING_SIZE   64U

struct RingBuffer
{
    uint8_t ring[RING_SIZE];
	uint32_t start; // TODO: these can be smaller, at least in documentation
	uint32_t end;
	uint32_t active;
};

void init_ring(struct RingBuffer * buf);
void ring_put(struct RingBuffer * buf, uint8_t element);
bool_ft ring_get(struct RingBuffer * buf, uint8_t * element);
bool_ft ring_empty(const struct RingBuffer * buf);

#endif
