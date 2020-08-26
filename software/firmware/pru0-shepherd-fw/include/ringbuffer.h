#ifndef RINGBUFFER_H_
#define RINGBUFFER_H_

#define RING_SIZE 64

#define BUF_EMPTY -1

struct RingBuffer
{
	char ring[RING_SIZE];
	int start;
	int end;
	int active;
};

void init_ring(struct RingBuffer * buf);
int ring_put(struct RingBuffer * buf, char element);
int ring_get(struct RingBuffer * buf, char * element);
int ring_empty(struct RingBuffer * buf);

#endif
