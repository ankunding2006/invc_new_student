#ifndef __SRV_PROTOCOL_H
#define __SRV_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#define PROTOCOL_FRAME_HEADER_1   (0xAA)
#define PROTOCOL_FRAME_HEADER_2   (0x55)
#define PROTOCOL_FRAME_TAIL       (0x0D)

#define PROTOCOL_CMD_TELEMETRY    (0x01)
#define PROTOCOL_PAYLOAD_LEN      (16)   /* 遥测数据载荷定长 16 字节 */
#define PROTOCOL_FRAME_TOTAL_LEN  (23)   /* 帧头2 + 序号1 + 长度1 + CMD1 + 载荷16 + 校验1 + 帧尾1 = 23 */

#pragma pack(push, 1)
/**
 * @brief 遥测数据载荷结构体 (定长 16 字节)
 */
typedef struct {
    int16_t  joy_x_raw;        /* 摇杆 X 归一化映射值 (-1000 ~ +1000) */
    int16_t  joy_y_raw;        /* 摇杆 Y 归一化映射值 (-1000 ~ +1000) */
    uint16_t joy_x_mv;         /* 摇杆 X 物理采样电压 (0 ~ 3300 mV) */
    uint16_t joy_y_mv;         /* 摇杆 Y 物理采样电压 (0 ~ 3300 mV) */
    uint8_t  key_mask;         /* 按键位掩码 (Bit0:K1, Bit1:K2, Bit2:K3, Bit3:K4; 1=按下) */
    uint8_t  switch_mask;      /* 拨码开关位掩码 (Bit0:SW1, Bit1:SW2; 1=高电平) */
    int16_t  pitch_cd;         /* 俯仰角 (单位: 0.1度, -1800 ~ +1800) */
    int16_t  roll_cd;          /* 横滚角 (单位: 0.1度, -1800 ~ +1800) */
    int16_t  yaw_cd;           /* 航向角 (单位: 0.1度, 0 ~ 3600) */
} telemetry_payload_t;
#pragma pack(pop)

void     srv_protocol_init(void);
uint16_t srv_protocol_pack(uint8_t seq_id, const telemetry_payload_t *p_payload, uint8_t *p_out_buf, uint16_t max_len);
uint8_t  srv_protocol_calc_checksum(const uint8_t *p_data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* __SRV_PROTOCOL_H */
