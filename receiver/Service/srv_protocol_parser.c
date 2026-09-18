#include "srv_protocol_parser.h"
#include <stddef.h>
static protocol_parser_t parser;
static protocol_frame_cb_t callback;
void srv_protocol_parser_init(protocol_frame_cb_t cb)
{
    callback = cb;
    protocol_parser_init(&parser);
}
void srv_protocol_parser_reset(void)
{
    parser.used = 0;
}
void srv_protocol_parser_feed_byte(uint8_t b)
{
    protocol_packet_t p;
    telemetry_payload_t data;
    if (protocol_parser_feed(&parser, b, 0, &p) && srv_protocol_unpack(&p, &data) && callback)
        callback(p.seq, &data);
}
parser_state_t srv_protocol_parser_get_state(void)
{
    unsigned n = parser.used;
    if (n < 2)
        return n ? PARSER_WAIT_HEADER_2 : PARSER_WAIT_HEADER_1;
    if (n == 2)
        return PARSER_READ_SEQ;
    if (n == 3)
        return PARSER_READ_LEN;
    if (n == 4)
        return PARSER_READ_CMD;
    if (n < 21)
        return PARSER_READ_PAYLOAD;
    return n == 21 ? PARSER_VERIFY_CHECKSUM : PARSER_WAIT_TAIL;
}
uint32_t srv_protocol_parser_get_error_count(void)
{
    return parser.errors;
}
