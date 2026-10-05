#ifndef IMU_TYPES_H
#define IMU_TYPES_H

/**
 * @file imu_types.h
 * @brief 六轴 IMU (加速度计与陀螺仪) 基础数据类型定义
 */

#include <stdint.h>

/**
 * @brief IMU 原始传感器寄存器采样数据结构体
 * 各分量说明:
 *  - ax, ay, az: X/Y/Z轴加速度计原始读数 (量程±8g, 灵敏度4096 LSB/g)
 *  - gx, gy, gz: X/Y/Z轴陀螺仪原始角速度读数 (量程±2000 dps, 灵敏度16.4 LSB/(deg/s))
 * @note 各分量已根据 IMU_BODY_X/Y/Z 完成载体坐标系轴向重映射
 */
typedef struct
{
    int16_t ax, ay, az, gx, gy, gz;
} imu_raw_t;

/**
 * @brief IMU 欧拉角姿态解算结果结构体
 * 各分量说明:
 *  - pitch: 俯仰角 (Pitch), 范围 -180° ~ +180°
 *  - roll:  横滚角 (Roll),  范围 -180° ~ +180°
 *  - yaw:   偏航角 (Yaw),   范围 0° ~ 360° (相对基准)
 * @note 角度单位为度 (degree), 遵循航空航天通用欧拉角定义 (Yaw-Pitch-Roll 旋转序列)
 */
typedef struct
{
    float pitch, roll, yaw;
} imu_euler_t;

#endif
