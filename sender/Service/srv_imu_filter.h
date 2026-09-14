#ifndef __SRV_IMU_FILTER_H
#define __SRV_IMU_FILTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 解算欧拉角结构体 (单位: 度)
 */
typedef struct {
    float pitch;    /* 俯仰角 (-180.0° ~ +180.0°) */
    float roll;     /* 横滚角 (-180.0° ~ +180.0°) */
    float yaw;      /* 航向角 (0.0° ~ 360.0°) */
} imu_euler_t;

/**
 * @brief 六轴传感器原始数据结构体
 */
typedef struct {
    int16_t ax; int16_t ay; int16_t az; /* 加速度计 ADC 原始读数 */
    int16_t gx; int16_t gy; int16_t gz; /* 陀螺仪 ADC 原始读数 */
} imu_raw_t;

void srv_imu_filter_init(float sample_freq_hz);
void srv_imu_filter_update(const imu_raw_t *p_raw, float dt_s, imu_euler_t *p_euler);
void srv_imu_filter_calibrate_gyro(void);
bool srv_imu_filter_is_calibrated(void);

#ifdef __cplusplus
}
#endif

#endif /* __SRV_IMU_FILTER_H */
