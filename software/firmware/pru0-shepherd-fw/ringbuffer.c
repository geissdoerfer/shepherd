#include "inttypes.h"
#include "ringbuffer.h"

void init_ring(struct RingBuffer *const buf)
{
	buf->start=0;
	buf->end=0;
	buf->active=0;
}

void ring_put(struct RingBuffer *const buf, const uint8_t element)
{
    buf->ring[buf->end] = element;

    // special faster version of buf = (buf + 1) % SIZE
    if(++buf->end == RING_SIZE) buf->end = 0U;

    if (buf->active < RING_SIZE)
    {
        buf->active++;
    }
    else
    {
        if(++buf->start == RING_SIZE) buf->start = 0U; // faster version
    }
}

bool_ft ring_get(struct RingBuffer *const buf, uint8_t *const element)
{
    if(buf->active == 0) return 0;

    *element = buf->ring[buf->start];
    if(++buf->start == RING_SIZE) buf->start = 0U; // faster version
    buf->active--;
    return 1;
}

bool_ft ring_empty(const struct RingBuffer *const buf)
{
    return (buf->active > 0) ? 1 : 0;
}
