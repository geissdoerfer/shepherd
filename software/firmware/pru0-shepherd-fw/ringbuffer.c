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

    if(++buf->end > RING_SIZE) buf->end -= RING_SIZE; // faster version of buf = (buf + 1) % SIZE

    if (buf->active < RING_SIZE)
    {
        buf->active++;
    }
    else
    {
        if(++buf->start > RING_SIZE) buf->start -= RING_SIZE; // faster version of buf = (buf + 1) % SIZE
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
    if(++buf->start > RING_SIZE) buf->start -= RING_SIZE; // faster version of buf = (buf + 1) % SIZE
    buf->active--;
    return 0;
}

int ring_empty(struct RingBuffer * buf)
{
    return buf->active;
}
