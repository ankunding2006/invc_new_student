#include "srv_ring_buffer.h"
#include <stddef.h>

static bool ring_buffer_valid(const ring_buffer_t *rb)
{
    return rb != NULL && rb->p_buffer != NULL && rb->size >= 2;
}

void srv_ring_buffer_init(ring_buffer_t *rb, uint8_t *storage, uint16_t size)
{
    if (rb == NULL)
    {
        return;
    }
    /* Invalid initialization leaves a disabled, safely queryable queue. */
    rb->p_buffer = (storage != NULL && size >= 2) ? storage : NULL;
    rb->size = (rb->p_buffer != NULL) ? size : 0;
    rb->head = 0;
    rb->tail = 0;
    rb->overflow_count = 0;
}

bool srv_ring_buffer_push(ring_buffer_t *rb, uint8_t byte)
{
    if (!ring_buffer_valid(rb))
    {
        return false;
    }
    uint16_t head = rb->head;
    uint16_t next_head = (uint16_t)((head + 1U) % rb->size);
    if (next_head == rb->tail)
    {
        rb->overflow_count++;
        return false;
    }
    rb->p_buffer[head] = byte;
    rb->head = next_head;
    return true;
}

bool srv_ring_buffer_pop(ring_buffer_t *rb, uint8_t *p_byte)
{
    if (!ring_buffer_valid(rb) || p_byte == NULL)
    {
        return false;
    }
    uint16_t tail = rb->tail;
    if (rb->head == tail)
    {
        return false;
    }
    *p_byte = rb->p_buffer[tail];
    rb->tail = (uint16_t)((tail + 1U) % rb->size);
    return true;
}

bool srv_ring_buffer_peek(const ring_buffer_t *rb, uint16_t offset, uint8_t *p_byte)
{
    if (!ring_buffer_valid(rb) || p_byte == NULL || offset >= srv_ring_buffer_get_count(rb))
    {
        return false;
    }
    *p_byte = rb->p_buffer[(rb->tail + offset) % rb->size];
    return true;
}

uint16_t srv_ring_buffer_get_count(const ring_buffer_t *rb)
{
    if (!ring_buffer_valid(rb))
    {
        return 0;
    }
    uint16_t tail = rb->tail;
    uint16_t head = rb->head;
    return (head >= tail) ? (uint16_t)(head - tail) : (uint16_t)(rb->size - tail + head);
}

bool srv_ring_buffer_is_empty(const ring_buffer_t *rb)
{
    return srv_ring_buffer_get_count(rb) == 0;
}

bool srv_ring_buffer_is_full(const ring_buffer_t *rb)
{
    return ring_buffer_valid(rb) && ((rb->head + 1U) % rb->size == rb->tail);
}

void srv_ring_buffer_clear(ring_buffer_t *rb)
{
    if (ring_buffer_valid(rb))
    {
        /* Consumer discards the current snapshot; never rewinds producer head. */
        rb->tail = rb->head;
    }
}

uint32_t srv_ring_buffer_get_overflow_count(const ring_buffer_t *rb)
{
    return (rb != NULL) ? rb->overflow_count : 0;
}
