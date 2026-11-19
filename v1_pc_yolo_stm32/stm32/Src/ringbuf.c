#include "ringbuf.h"

void ringbuf_init(ringbuf_t *rb)
{
    rb->head = 0u;
    rb->tail = 0u;
}

int ringbuf_put(ringbuf_t *rb, uint8_t byte)
{
    uint16_t next = (uint16_t)((rb->head + 1u) & (RINGBUF_SIZE - 1u));
    if (next == rb->tail) {
        return 0;   /* full */
    }
    rb->buf[rb->head] = byte;
    rb->head = next;
    return 1;
}

int ringbuf_get(ringbuf_t *rb, uint8_t *byte)
{
    if (rb->tail == rb->head) {
        return 0;   /* empty */
    }
    *byte = rb->buf[rb->tail];
    rb->tail = (uint16_t)((rb->tail + 1u) & (RINGBUF_SIZE - 1u));
    return 1;
}

uint16_t ringbuf_count(const ringbuf_t *rb)
{
    return (uint16_t)((rb->head - rb->tail) & (RINGBUF_SIZE - 1u));
}
