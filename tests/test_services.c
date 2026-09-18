#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include "srv_ring_buffer.h"
static void test_ring(void)
{
    ring_buffer_t rb = {0};
    uint8_t storage[4] = {0}, byte = 77;
    assert(!srv_ring_buffer_is_full(&rb));
    assert(!srv_ring_buffer_push(&rb, 1));
    assert(!srv_ring_buffer_pop(&rb, &byte));
    assert(srv_ring_buffer_is_empty(&rb));
    assert(!srv_ring_buffer_push(NULL, 1));
    srv_ring_buffer_init(NULL, storage, 4);
    srv_ring_buffer_init(&rb, storage, 4);
    assert(srv_ring_buffer_push(&rb, 1));
    assert(srv_ring_buffer_push(&rb, 2));
    assert(srv_ring_buffer_push(&rb, 3));
    assert(srv_ring_buffer_is_full(&rb));
    assert(!srv_ring_buffer_push(&rb, 4));
    assert(srv_ring_buffer_get_overflow_count(&rb) == 1);
    assert(srv_ring_buffer_peek(&rb, 2, &byte) && byte == 3);
    assert(!srv_ring_buffer_peek(&rb, 3, &byte));
    assert(!srv_ring_buffer_pop(&rb, NULL));
    assert(srv_ring_buffer_pop(&rb, &byte) && byte == 1);
    assert(srv_ring_buffer_push(&rb, 4));
    assert(srv_ring_buffer_get_count(&rb) == 3);
    for (uint8_t i = 2; i <= 4; ++i)
    {
        assert(srv_ring_buffer_pop(&rb, &byte) && byte == i);
    }
    for (unsigned i = 0; i < 10000; ++i)
    {
        assert(srv_ring_buffer_push(&rb, (uint8_t)i));
        assert(srv_ring_buffer_pop(&rb, &byte) && byte == (uint8_t)i);
    }
    assert(srv_ring_buffer_push(&rb, 5));
    uint16_t head = rb.head;
    srv_ring_buffer_clear(&rb);
    assert(rb.head == head && rb.tail == head);
    assert(srv_ring_buffer_get_overflow_count(&rb) == 1);
    assert(srv_ring_buffer_push(&rb, 6));
    assert(srv_ring_buffer_pop(&rb, &byte) && byte == 6);
    /* Invalid reinitialization must not retain a stale storage pointer. */
    srv_ring_buffer_init(&rb, storage, 0);
    assert(rb.p_buffer == NULL && rb.size == 0);
    assert(!srv_ring_buffer_is_full(&rb));
    assert(!srv_ring_buffer_push(&rb, 1));
    srv_ring_buffer_init(&rb, storage, 1);
    assert(!srv_ring_buffer_push(&rb, 1));
    srv_ring_buffer_init(&rb, NULL, 4);
    assert(srv_ring_buffer_get_count(&rb) == 0);
    assert(!srv_ring_buffer_peek(&rb, 0, &byte));
    srv_ring_buffer_clear(&rb);
    srv_ring_buffer_init(&rb, storage, 2);
    assert(srv_ring_buffer_push(&rb, 9));
    assert(!srv_ring_buffer_push(&rb, 8));
    assert(srv_ring_buffer_pop(&rb, &byte) && byte == 9);
}

int main(void)
{
    test_ring();
    puts("Ring buffer regression PASS");
    return 0;
}
