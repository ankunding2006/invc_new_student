#ifndef SRV_PC_H
#define SRV_PC_H

/**
 * @file srv_pc.h
 * @brief PC 上位机 (VOFA+ / FireWater 协议) 数据帧封包服务
 * 
 * 帧结构:
 * 包含 14 路 32 位 IEEE 754 单精度浮点数 (小端字节序, 共 56 字节)
 * 紧随 4 字节固定帧尾: 0x00 0x00 0x80 0x7F (正无穷大 +Inf / 常用协议帧尾)
 * 总帧长 60 字节。
 */

#include "srv_protocol.h"

/** @brief PC 浮点遥测帧总字节数 (14 通道 * 4 字节 + 4 字节帧尾 = 60 字节) */
#define PC_FLOAT_FRAME_LEN 60U

/**
 * @brief 将遥测与通信统计数据打包为 VOFA+ FireWater 浮点格式数据帧
 * 
 * 14 个通道分配映射:
 *  - 通道 0: 摇杆 X 归一化值 (-1000 ~ +1000)
 *  - 通道 1: 摇杆 Y 归一化值 (-1000 ~ +1000)
 *  - 通道 2: 摇杆 X 采样电压 (伏特 V, 0 ~ 3.3V)
 *  - 通道 3: 摇杆 Y 采样电压 (伏特 V, 0 ~ 3.3V)
 *  - 通道 4: 按键低 4 位状态掩码 (0 ~ 15)
 *  - 通道 5: 拨码开关低 2 位掩码 (0 ~ 3)
 *  - 通道 6: 俯仰角 Pitch (度 °)
 *  - 通道 7: 横滚角 Roll (度 °)
 *  - 通道 8: 偏航角 Yaw (度 °)
 *  - 通道 9: 实际有效遥测帧率 (Hz)
 *  - 通道 10: 估算丢包率百分比 (0 ~ 100%)
 *  - 通道 11: 无线物理裸包帧率 (Hz)
 *  - 通道 12: 故障与标定标志 (key_mask 高 4 位)
 *  - 通道 13: 链路在线状态 (1.0 = 在线 Connected, 0.0 = 离线 Offline)
 * 
 * @param[in]  p      遥测核心载荷数据指针
 * @param[in]  freq   当前有效帧接收频率 (Hz)
 * @param[in]  loss   当前丢包率百分比 (%)
 * @param[in]  wire   空中物理包频率 (Hz)
 * @param[in]  online 当前链路是否处于在线状态
 * @param[out] out    输出数据缓冲区
 * @param[in]  cap    输出缓冲区可用容量 (需 >= 60)
 * @return 实际写入的字节数 (60)，失败返回 0
 */
uint16_t srv_pc_pack(const telemetry_payload_t *p, float freq, float loss, float wire, bool online,
                     uint8_t *out, uint16_t cap);

#endif
