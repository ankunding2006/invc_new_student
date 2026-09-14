#include "srv_imu_filter.h"
#include <stddef.h>

static bool s_is_calibrated = false;

void srv_imu_filter_init(float sample_freq_hz)
{
    (void)sample_freq_hz;
    s_is_calibrated = false;
}

void srv_imu_filter_update(const imu_raw_t *p_raw, float dt_s, imu_euler_t *p_euler)
{
    if (p_raw == NULL || p_euler == NULL) {
        return;
    }
    (void)dt_s;
    /* 姿态滤波解算骨架，后续阶段填充互补滤波 / Mahony 算法 */
    p_euler->pitch = 0.0f;
    p_euler->roll = 0.0f;
    p_euler->yaw = 0.0f;
}

void srv_imu_filter_calibrate_gyro(void)
{
    /* 零偏校准骨架 */
    s_is_calibrated = true;
}

bool srv_imu_filter_is_calibrated(void)
{
    return s_is_calibrated;
}
