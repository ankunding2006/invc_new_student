#include "srv_ring_buffer.h"
#include <stddef.h>

void srv_ring_buffer_init(ring_buffer_t *rb, uint8_t *storage, uint16_t size)
{
    if (rb != NULL && storage != NULL && size > 0) {
        rb->p_buffer = storage;
        rb->size = size;
        rb->head = 0;
        rb->tail = 0;
        rb->overflow_count = 0;
    }
}

bool srv_ring_buffer_push(ring_buffer_t *rb, uint8_t byte)
{
    if (rb == NULL || rb->p_buffer == NULL) {
        return false;
    }
    uint16_t next_head = (rb->head + 1) % rb->size;
    if (next_head == rb->tail) {
        rb->overflow_count++; /* 溢出丢弃并累加错误计数 (E-COM-02) */
        return false; /* 队列已满, 拒绝写入并禁止推进写指针 */
    }
    rb->p_buffer[rb->head] = byte;
    rb->head = next_head;
    return true;
}

bool srv_ring_buffer_pop(ring_buffer_t *rb, uint8_t *p_byte)
{
    if (rb == NULL || rb->p_buffer == NULL || p_byte == NULL) {
        return false;
    }
    if (rb->head == rb->tail) {
        return false; /* 队列为空 */
    }
    *p_byte = rb->p_buffer[rb->tail];
    rb->tail = (rb->tail + 1) % rb->size;
    return true;
}

bool srv_ring_buffer_peek(const ring_buffer_t *rb, uint16_t offset, uint8_t *p_byte)
{
    if (rb == NULL || rb->p_buffer == NULL || p_byte == NULL) {
        return false;
    }
    uint16_t count = srv_ring_buffer_get_count(rb);
    if (offset >= count) {
        return false;
    }
    uint16_t index = (rb->tail + offset) % rb->size;
    *p_byte = rb->p_buffer[index];
    return true;
}

uint16_t srv_ring_buffer_get_count(const ring_buffer_t *rb)
{
    if (rb == NULL) {
        return 0;
    }
    if (rb->head >= rb->tail) {
        return rb->head - rb->tail;
    } else {
        return rb->size - rb->tail + rb->head;
    }
}

bool srv_ring_buffer_is_empty(const ring_buffer_t *rb)
{
    if (rb == NULL) {
        return true;
    }
    return (rb->head == rb->tail);
}

bool srv_ring_buffer_is_full(const ring_buffer_t *rb)
{
    if (rb == NULL) {
        return false;
    }
    return ((rb->head + 1) % rb->size == rb->tail);
}

void srv_ring_buffer_clear(ring_buffer_t *rb)
{
    if (rb != NULL) {
        rb->head = 0;
        rb->tail = 0;
    }
}

uint32_t srv_ring_buffer_get_overflow_count(const ring_buffer_t *rb)
{
    return (rb != NULL) ? rb->overflow_count : 0;
}
