#ifndef RINGBUFFER_H_
#define RINGBUFFER_H_

#define RING_SIZE 64

#define BUF_EMPTY -1

struct RingBuffer
{
    uint8_t ring[RING_SIZE];
	int32_t start;
	int32_t end;
	int32_t active;
};

void init_ring(struct RingBuffer * buf);
void ring_put(struct RingBuffer * buf, uint8_t element);
int32_t ring_get(struct RingBuffer * buf, uint8_t * element);
int32_t ring_empty(struct RingBuffer * buf);

#endif
