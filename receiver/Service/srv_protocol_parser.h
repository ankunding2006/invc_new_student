#ifndef __SRV_PROTOCOL_PARSER_H
#define __SRV_PROTOCOL_PARSER_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdbool.h>
#include "srv_protocol.h"

    /**
     * @brief 流式单字节解包有限状态机状态枚举
     */
    typedef enum
    {
        PARSER_WAIT_HEADER_1 = 0, /* 等待帧头 1: 0xAA */
        PARSER_WAIT_HEADER_2,     /* 等待帧头 2: 0x55 */
        PARSER_READ_SEQ,          /* 读取包序列号 */
        PARSER_READ_LEN,          /* 读取有效长度 */
        PARSER_READ_CMD,          /* 读取功能码 */
        PARSER_READ_PAYLOAD,      /* 逐字节读取 16 字节载荷 */
        PARSER_VERIFY_CHECKSUM,   /* 比对累加和校验 */
        PARSER_WAIT_TAIL          /* 验证帧尾 0x0D */
    } parser_state_t;

    /**
     * @brief 合法遥测包解析回调函数指针类型
     */
    typedef void (*protocol_frame_cb_t)(uint8_t seq_id, const telemetry_payload_t *p_payload);

    void srv_protocol_parser_init(protocol_frame_cb_t callback);
    void srv_protocol_parser_feed_byte(uint8_t byte);
    void srv_protocol_parser_reset(void);
    parser_state_t srv_protocol_parser_get_state(void);
    uint32_t srv_protocol_parser_get_error_count(void); /* 校验失败或格式错误累计 (E-COM-03~05) */

#ifdef __cplusplus
}
#endif

#endif /* __SRV_PROTOCOL_PARSER_H */
