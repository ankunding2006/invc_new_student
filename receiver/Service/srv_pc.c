/**
 * @file srv_pc.c
 * @brief PC 上位机 (VOFA+ / FireWater 协议) 数据帧序列化实现
 */

#include "srv_pc.h"
#include <string.h>
#include <float.h>

/**
 * @brief 编译期断言: 确保系统编译器采用标准单精度 IEEE 754 浮点格式 (4字节、基数2、尾数24位)
 */
typedef char
    require_ieee_float[(sizeof(float) == 4 && FLT_RADIX == 2 && FLT_MANT_DIG == 24) ? 1 : -1];

/**
 * @brief 打包 14 通道 IEEE 754 浮点数据帧 + 固定帧尾
 * @param[in]  p      遥测数据指针
 * @param[in]  freq   当前有效帧接收率 (Hz)
 * @param[in]  loss   当前丢包率 (%)
 * @param[in]  wire   空中物理帧率 (Hz)
 * @param[in]  online 链路在线标志
 * @param[out] out    输出缓冲区
 * @param[in]  cap    缓冲区容量 (需 >= 60)
 * @return 写入字节数 (60)
 */
uint16_t srv_pc_pack(const telemetry_payload_t *p, float freq, float loss, float wire, bool online,
                     uint8_t *out, uint16_t cap)
{
    if (!p || !out || cap < PC_FLOAT_FRAME_LEN)
        return 0;
    /* 构造 14 路待输出的浮点数数组 */
    float values[14] = {(float)p->joy_x_raw,
                        (float)p->joy_y_raw,
                        p->joy_x_mv * 0.001f,
                        p->joy_y_mv * 0.001f,
                        (float)(p->key_mask & 15),
                        (float)(p->switch_mask & 3),
                        p->pitch_cd * 0.1f,
                        p->roll_cd * 0.1f,
                        p->yaw_cd * 0.1f,
                        freq,
                        loss,
                        wire,
                        (float)(p->key_mask >> 4),
                        online ? 1.0f : 0.0f};
    /* 将每个 float 序列化为 4 字节小端序写入输出缓冲区 */
    for (unsigned i = 0; i < 14; i++)
    {
        uint32_t bits;
        memcpy(&bits, &values[i], 4);
        for (unsigned b = 0; b < 4; b++)
            out[4 * i + b] = (uint8_t)(bits >> (8 * b));
    }
    /* 写入 VOFA+ FireWater 尾部同步标记: 0x00 0x00 0x80 0x7F (+Inf) */
    out[56] = 0;
    out[57] = 0;
    out[58] = 0x80;
    out[59] = 0x7F;
    return 60;
}
