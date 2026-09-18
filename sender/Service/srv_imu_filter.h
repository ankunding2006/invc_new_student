#ifndef SRV_IMU_FILTER_H
#define SRV_IMU_FILTER_H
#include <stdbool.h>
#include "imu_types.h"
void srv_imu_filter_init(float sample_freq_hz);
void srv_imu_filter_update(const imu_raw_t *raw, float dt_s, imu_euler_t *out);
void srv_imu_filter_calibrate_gyro(void);
bool srv_imu_filter_is_calibrated(void);
unsigned srv_imu_filter_calibration_count(void);
void srv_imu_filter_raw_angles(imu_euler_t *out);
#endif
