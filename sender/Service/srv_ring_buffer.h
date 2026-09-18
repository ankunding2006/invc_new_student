#ifndef __SRV_RING_BUFFER_H
#define __SRV_RING_BUFFER_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdbool.h>

    /**
     * @brief 通用轻量环形字节缓冲区结构体
     */
    typedef struct
    {
        volatile uint8_t *p_buffer;
        uint16_t size;
        volatile uint16_t head;
        volatile uint16_t tail;
        volatile uint32_t overflow_count; /* 溢出丢弃计数器 (E-COM-02 防御性标记) */
    } ring_buffer_t;

    /* Single producer (ISR), single consumer (main loop) on single-core STM32.
     * Storage size >= 2; usable capacity = size - 1. Initialize before enabling ISR.
     * init requires exclusive access; pop/peek/clear belong to the consumer.
     * clear keeps overflow_count and discards only bytes visible at its snapshot. */
    void srv_ring_buffer_init(ring_buffer_t *rb, uint8_t *storage, uint16_t size);
    bool srv_ring_buffer_push(ring_buffer_t *rb, uint8_t byte);
    bool srv_ring_buffer_pop(ring_buffer_t *rb, uint8_t *p_byte);
    bool srv_ring_buffer_peek(const ring_buffer_t *rb, uint16_t offset, uint8_t *p_byte);
    uint16_t srv_ring_buffer_get_count(const ring_buffer_t *rb);
    bool srv_ring_buffer_is_empty(const ring_buffer_t *rb);
    bool srv_ring_buffer_is_full(const ring_buffer_t *rb);
    uint32_t srv_ring_buffer_get_overflow_count(const ring_buffer_t *rb);
    void srv_ring_buffer_clear(ring_buffer_t *rb);

#ifdef __cplusplus
}
#endif

#endif /* __SRV_RING_BUFFER_H */
