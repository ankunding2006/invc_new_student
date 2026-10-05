/**
 * @file srv_protocol_parser.c
 * @brief 接收端协议解析器适配器与状态查询实现
 */

#include "srv_protocol_parser.h"
#include <stddef.h>

/** @brief 内部协议解析器状态实例 */
static protocol_parser_t parser;
/** @brief 解析成功回调函数指针 */
static protocol_frame_cb_t callback;

/**
 * @brief 初始化解析器并配置回调
 * @param[in] cb 回调函数
 */
void srv_protocol_parser_init(protocol_frame_cb_t cb)
{
    callback = cb;
    protocol_parser_init(&parser);
}

/**
 * @brief 重置解析器内部缓存
 */
void srv_protocol_parser_reset(void)
{
    parser.used = 0;
}

/**
 * @brief 喂入单字节并触发回调
 * @param[in] b 接收字节
 */
void srv_protocol_parser_feed_byte(uint8_t b)
{
    protocol_packet_t p;
    telemetry_payload_t data;
    /* 喂入底层解析器，若解包成功且通过遥测解包验证，则触发上层回调 */
    if (protocol_parser_feed(&parser, b, 0, &p) && srv_protocol_unpack(&p, &data) && callback)
        callback(p.seq, &data);
}

/**
 * @brief 根据当前已缓冲的字节数推导对应的解析状态阶段
 * @return 状态阶段枚举
 */
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

/**
 * @brief 获取累计错误计数
 * @return 错误总数
 */
uint32_t srv_protocol_parser_get_error_count(void)
{
    return parser.errors;
}
