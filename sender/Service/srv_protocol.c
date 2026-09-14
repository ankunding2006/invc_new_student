#include "srv_protocol.h"
#include <string.h>

void srv_protocol_init(void)
{
    /* 初始化协议封包引擎 */
}

uint8_t srv_protocol_calc_checksum(const uint8_t *p_data, uint16_t len)
{
    uint8_t sum = 0;
    if (p_data == NULL || len == 0) {
        return 0;
    }
    for (uint16_t i = 0; i < len; i++) {
        sum += p_data[i];
    }
    return sum;
}

uint16_t srv_protocol_pack(uint8_t seq_id, const telemetry_payload_t *p_payload, uint8_t *p_out_buf, uint16_t max_len)
{
    if (p_payload == NULL || p_out_buf == NULL || max_len < PROTOCOL_FRAME_TOTAL_LEN) {
        return 0;
    }

    p_out_buf[0] = PROTOCOL_FRAME_HEADER_1;
    p_out_buf[1] = PROTOCOL_FRAME_HEADER_2;
    p_out_buf[2] = seq_id;
    p_out_buf[3] = (uint8_t)sizeof(telemetry_payload_t);
    p_out_buf[4] = PROTOCOL_CMD_TELEMETRY;

    memcpy(&p_out_buf[5], p_payload, sizeof(telemetry_payload_t));

    /* 计算累加和校验 (从 seq_id 到 payload 末尾) */
    uint16_t check_len = 3 + (uint16_t)sizeof(telemetry_payload_t); /* seq_id + len + cmd + payload */
    uint8_t checksum = srv_protocol_calc_checksum(&p_out_buf[2], check_len);

    p_out_buf[5 + sizeof(telemetry_payload_t)] = checksum;
    p_out_buf[5 + sizeof(telemetry_payload_t) + 1] = PROTOCOL_FRAME_TAIL;

    return PROTOCOL_FRAME_TOTAL_LEN;
}
