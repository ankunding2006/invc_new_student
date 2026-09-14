#include "srv_protocol_parser.h"
#include <stddef.h>

static parser_state_t s_parser_state = PARSER_WAIT_HEADER_1;
static protocol_frame_cb_t s_callback = NULL;
static uint32_t s_error_count = 0;

void srv_protocol_parser_init(protocol_frame_cb_t callback)
{
    s_callback = callback;
    s_error_count = 0;
    srv_protocol_parser_reset();
}

void srv_protocol_parser_reset(void)
{
    s_parser_state = PARSER_WAIT_HEADER_1;
}

void srv_protocol_parser_feed_byte(uint8_t byte)
{
    (void)byte;
    /* 单字节流式状态机转移骨架 */
}

parser_state_t srv_protocol_parser_get_state(void)
{
    return s_parser_state;
}

uint32_t srv_protocol_parser_get_error_count(void)
{
    return s_error_count;
}
