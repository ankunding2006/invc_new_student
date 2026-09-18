#include "srv_imu_filter.h"
#include "system_config.h"
#include <math.h>
#include <string.h>
#define DEG 57.2957795131f
static float q[4], bias[3], mean[3], m2[3];
static unsigned samples;
static bool calibrated, aligned;
static imu_euler_t angles, raw_angles;
static float clamp1(float x)
{
    return x > 1 ? 1 : x < -1 ? -1 : x;
}
void srv_imu_filter_calibrate_gyro(void)
{
    samples = 0;
    calibrated = aligned = false;
    memset(mean, 0, sizeof(mean));
    memset(m2, 0, sizeof(m2));
    memset(bias, 0, sizeof(bias));
    q[0] = 1;
    q[1] = q[2] = q[3] = 0;
    memset(&angles, 0, sizeof(angles));
    memset(&raw_angles, 0, sizeof(raw_angles));
}
void srv_imu_filter_init(float hz)
{
    (void)hz;
    srv_imu_filter_calibrate_gyro();
}
bool srv_imu_filter_is_calibrated(void)
{
    return calibrated;
}
unsigned srv_imu_filter_calibration_count(void)
{
    return samples;
}
void srv_imu_filter_raw_angles(imu_euler_t *out)
{
    if (out)
        *out = raw_angles;
}
void srv_imu_filter_update(const imu_raw_t *r, float dt, imu_euler_t *out)
{
    if (!r || !out)
        return;
    *out = angles;
    if (!isfinite(dt) || dt <= 0 || dt > 0.1f)
        return;
    float ax = r->ax / IMU_ACCEL_LSB_PER_G, ay = r->ay / IMU_ACCEL_LSB_PER_G,
          az = r->az / IMU_ACCEL_LSB_PER_G;
    float g[3] = {r->gx / IMU_GYRO_LSB_PER_DPS, r->gy / IMU_GYRO_LSB_PER_DPS,
                  r->gz / IMU_GYRO_LSB_PER_DPS};
    float norm = sqrtf(ax * ax + ay * ay + az * az);
    if (norm > 0.01f)
    {
        raw_angles.roll = atan2f(ay, az) * DEG;
        raw_angles.pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * DEG;
    }
    if (!calibrated)
    {
        if (norm < 0.9f || norm > 1.1f || fabsf(g[0]) > 20 || fabsf(g[1]) > 20 || fabsf(g[2]) > 20)
        {
            samples = 0;
            memset(mean, 0, sizeof(mean));
            memset(m2, 0, sizeof(m2));
            return;
        }
        samples++;
        for (unsigned i = 0; i < 3; i++)
        {
            float d = g[i] - mean[i];
            mean[i] += d / (float)samples;
            m2[i] += d * (g[i] - mean[i]);
        }
        if (samples >= IMU_CAL_SAMPLES)
        {
            if (m2[0] / samples > 0.25f || m2[1] / samples > 0.25f || m2[2] / samples > 0.25f)
            {
                samples = 0;
                memset(mean, 0, sizeof(mean));
                memset(m2, 0, sizeof(m2));
                return;
            }
            memcpy(bias, mean, sizeof(bias));
            calibrated = true;
        }
        else
            return;
    }
    if (!aligned)
    {
        if (norm < 0.8f || norm > 1.2f)
            return;
        float r2 = raw_angles.roll / DEG * 0.5f, p2 = raw_angles.pitch / DEG * 0.5f;
        q[0] = cosf(r2) * cosf(p2);
        q[1] = sinf(r2) * cosf(p2);
        q[2] = cosf(r2) * sinf(p2);
        q[3] = -sinf(r2) * sinf(p2);
        aligned = true;
    }
    for (unsigned i = 0; i < 3; i++)
        g[i] = (g[i] - bias[i]) / DEG;
    raw_angles.yaw += g[2] * dt * DEG;
    if (raw_angles.yaw >= 360)
        raw_angles.yaw -= 360;
    if (raw_angles.yaw < 0)
        raw_angles.yaw += 360;
    /* Acceleration outside the gravity band is excluded from correction. */
    if (norm > 0.8f && norm < 1.2f)
    {
        ax /= norm;
        ay /= norm;
        az /= norm;
        float vx = 2 * (q[1] * q[3] - q[0] * q[2]), vy = 2 * (q[0] * q[1] + q[2] * q[3]),
              vz = q[0] * q[0] - q[1] * q[1] - q[2] * q[2] + q[3] * q[3];
        g[0] += IMU_CORRECTION_KP * (ay * vz - az * vy);
        g[1] += IMU_CORRECTION_KP * (az * vx - ax * vz);
        g[2] += IMU_CORRECTION_KP * (ax * vy - ay * vx);
    }
    float w = q[0], x = q[1], y = q[2], z = q[3], h = dt * 0.5f;
    q[0] += (-x * g[0] - y * g[1] - z * g[2]) * h;
    q[1] += (w * g[0] + y * g[2] - z * g[1]) * h;
    q[2] += (w * g[1] - x * g[2] + z * g[0]) * h;
    q[3] += (w * g[2] + x * g[1] - y * g[0]) * h;
    float n = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    if (!isfinite(n) || n < 0.0001f)
    {
        srv_imu_filter_calibrate_gyro();
        memset(out, 0, sizeof(*out));
        return;
    }
    for (unsigned i = 0; i < 4; i++)
        q[i] /= n;
    angles.roll =
        atan2f(2 * (q[0] * q[1] + q[2] * q[3]), 1 - 2 * (q[1] * q[1] + q[2] * q[2])) * DEG;
    angles.pitch = asinf(clamp1(2 * (q[0] * q[2] - q[3] * q[1]))) * DEG;
    angles.yaw = atan2f(2 * (q[0] * q[3] + q[1] * q[2]), 1 - 2 * (q[2] * q[2] + q[3] * q[3])) * DEG;
    if (angles.yaw < 0)
        angles.yaw += 360;
    *out = angles;
}
