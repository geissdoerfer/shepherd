#include "inttypes.h"
#include "ringbuffer.h"

void init_ring(struct RingBuffer * buf)
{
	buf->start=0;
	buf->end=0;
	buf->active=0;
}

int ring_put(struct RingBuffer * buf, uint8_t element)
{
    buf->ring[buf->end] = element;

    buf->end = (buf->end + 1) % RING_SIZE;

    if (buf->active < RING_SIZE)
    {
        buf->active++;
    }
    else
    {
        buf->start = (buf->start + 1) % RING_SIZE;
    }
    return 0;
}

int ring_get(struct RingBuffer * buf, uint8_t * element)
{
    if(!buf->active)
    {
    	return BUF_EMPTY;
    }
    *element = buf->ring[buf->start];
    buf->start = (buf->start + 1) % RING_SIZE;
    buf->active--;
    return 0;
}

int ring_empty(struct RingBuffer * buf)
{
    return buf->active;
}
