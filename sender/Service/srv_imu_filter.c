/**
 * @file srv_imu_filter.c
 * @brief 六轴姿态解算互补滤波器实现
 * 
 * 算法数学原理:
 * 1. 静态检测与陀螺零偏标定:
 *    使用 Welford 单遍在线均值与方差递推算法：
 *      delta = x_k - mean_{k-1}
 *      mean_k = mean_{k-1} + delta / k
 *      M2_k = M2_{k-1} + delta * (x_k - mean_k)
 *      variance = M2_k / k
 *    当加速度模长处于 [0.8g, 1.2g] 且各轴角速度 < 20 dps 且方差 < 0.25 (dps)^2 时确认为静止，
 *    采集满 100 个样本后锁定零偏。
 * 
 * 2. 初始姿态对齐:
 *    利用重力加速度在机体轴投影直接计算初始横滚角与俯仰角：
 *      roll0 = atan2(ay, az)
 *      pitch0 = atan2(-ax, sqrt(ay^2 + az^2))
 *    转换为初始四元数 q0。
 * 
 * 3. Mahony 互补滤波:
 *    理论重力方向在机体坐标系的投影向量 v:
 *      vx = 2 * (q1*q3 - q0*q2)
 *      vy = 2 * (q0*q1 + q2*q3)
 *      vz = q0^2 - q1^2 - q2^2 + q3^2
 *    加速度测得的重力方向 a_norm 与理论方向 v 的叉积误差：
 *      e = a_norm x v
 *    误差乘以 Kp (IMU_CORRECTION_KP=2.0) 补偿到陀螺仪角速度向量中。
 * 
 * 4. 一阶 Runge-Kutta 四元数积分:
 *    q_{k+1} = q_k + 0.5 * dt * (q_k * omega)
 *    积分后执行四元数模长归一化。
 * 
 * 5. 欧拉角转换:
 *    Roll  = atan2(2*(q0*q1 + q2*q3), 1 - 2*(q1^2 + q2^2))
 *    Pitch = asin(2*(q0*q2 - q3*q1))
 *    Yaw   = atan2(2*(q0*q3 + q1*q2), 1 - 2*(q2^2 + q3^2))
 */

#include "srv_imu_filter.h"
#include "system_config.h"
#include <math.h>
#include <string.h>

/** @brief 弧度转角度系数: 180 / PI */
#define DEG 57.2957795131f

/** @brief 姿态四元数 [q0(w), q1(x), q2(y), q3(z)] */
static float q[4], bias[3], mean[3], m2[3];
/** @brief 标定样本计数 */
static unsigned samples;
/** @brief 标志位: calibrated=标定完成, aligned=初始姿态对齐完成 */
static bool calibrated, aligned;
/** @brief 滤波后欧拉角与加速度计几何解算的原始姿态角 */
static imu_euler_t angles, raw_angles;

/**
 * @brief 内部辅助函数: 将数值限制在 [-1.0, +1.0] 闭区间内，防止 asinf 越界产生 NaN
 * @param[in] x 输入值
 * @return 限幅后的值
 */
static float clamp1(float x)
{
    return x > 1 ? 1 : x < -1 ? -1 : x;
}

/**
 * @brief 重置并重新触发陀螺仪静态零偏标定流程
 */
void srv_imu_filter_calibrate_gyro(void)
{
    samples = 0;
    calibrated = aligned = false;
    memset(mean, 0, sizeof(mean));
    memset(m2, 0, sizeof(m2));
    memset(bias, 0, sizeof(bias));
    /* 初始化为单位四元数 [1, 0, 0, 0] */
    q[0] = 1;
    q[1] = q[2] = q[3] = 0;
    memset(&angles, 0, sizeof(angles));
    memset(&raw_angles, 0, sizeof(raw_angles));
}

/**
 * @brief 初始化姿态滤波器
 * @param[in] hz 采样频率
 */
void srv_imu_filter_init(float hz)
{
    (void)hz;
    srv_imu_filter_calibrate_gyro();
}

/**
 * @brief 查询陀螺仪是否已完成标定
 * @return true 已标定完成; false 仍未完成
 */
bool srv_imu_filter_is_calibrated(void)
{
    return calibrated;
}

/**
 * @brief 获取当前有效标定样本累积数
 * @return 样本数 (0~100)
 */
unsigned srv_imu_filter_calibration_count(void)
{
    return samples;
}

/**
 * @brief 获取加速度计几何解算的未滤波原始姿态角
 * @param[out] out 输出原始角度指针
 */
void srv_imu_filter_raw_angles(imu_euler_t *out)
{
    if (out)
        *out = raw_angles;
}

/**
 * @brief 姿态解算核心更新函数
 * @param[in]  r   六轴 IMU 原始读数
 * @param[in]  dt  时间差 (秒)
 * @param[out] out 输出滤波后的欧拉角
 */
void srv_imu_filter_update(const imu_raw_t *r, float dt, imu_euler_t *out)
{
    if (!r || !out)
        return;
    *out = angles;
    /* 异常时间步长检查: dt 必须为有限正数且不超过 100ms */
    if (!isfinite(dt) || dt <= 0 || dt > 0.1f)
        return;

    /* 1. 物理单位换算: 加速度转为 g，角速度转为 deg/s */
    float ax = r->ax / IMU_ACCEL_LSB_PER_G, ay = r->ay / IMU_ACCEL_LSB_PER_G,
          az = r->az / IMU_ACCEL_LSB_PER_G;
    float g[3] = {r->gx / IMU_GYRO_LSB_PER_DPS, r->gy / IMU_GYRO_LSB_PER_DPS,
                  r->gz / IMU_GYRO_LSB_PER_DPS};

    /* 计算当前实测加速度向量模长 */
    float norm = sqrtf(ax * ax + ay * ay + az * az);
    if (norm > 0.01f)
    {
        /* 利用重力方向计算原始横滚角与俯仰角 */
        raw_angles.roll = atan2f(ay, az) * DEG;
        raw_angles.pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * DEG;
    }

    /* 2. 静态检测与陀螺仪在线零偏标定阶段 */
    if (!calibrated)
    {
        /* 严苛静止门限: 加速度模长偏离1g不得超过20%，且角速度必须小于20 deg/s */
        if (norm < 0.8f || norm > 1.2f || fabsf(g[0]) > 20 || fabsf(g[1]) > 20 || fabsf(g[2]) > 20)
        {
            /* 一旦检测到晃动，立即清空此前采集的所有样本重新开始 */
            samples = 0;
            memset(mean, 0, sizeof(mean));
            memset(m2, 0, sizeof(m2));
            return;
        }
        samples++;
        /* Welford 在线方差递推 */
        for (unsigned i = 0; i < 3; i++)
        {
            float d = g[i] - mean[i];
            mean[i] += d / (float)samples;
            m2[i] += d * (g[i] - mean[i]);
        }
        /* 采满 100 样本后检查样本方差 */
        if (samples >= IMU_CAL_SAMPLES)
        {
            /* 若任一轴方差 > 0.25 (dps)^2 说明期间存在微弱振动，标定不合格并复位重来 */
            if (m2[0] / samples > 0.25f || m2[1] / samples > 0.25f || m2[2] / samples > 0.25f)
            {
                samples = 0;
                memset(mean, 0, sizeof(mean));
                memset(m2, 0, sizeof(m2));
                return;
            }
            /* 标定合格，锁定零偏 */
            memcpy(bias, mean, sizeof(bias));
            calibrated = true;
        }
        else
            return;
    }

    /* 3. 初始姿态对齐: 首次利用加速度计重力倾角初始化四元数姿态 */
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

    /* 扣除陀螺仪零偏，并转换为 rad/s 供积分使用 */
    for (unsigned i = 0; i < 3; i++)
        g[i] = (g[i] - bias[i]) / DEG;

    /* 原始航向角积分 (0~360度环形) */
    raw_angles.yaw += g[2] * dt * DEG;
    if (raw_angles.yaw >= 360)
        raw_angles.yaw -= 360;
    if (raw_angles.yaw < 0)
        raw_angles.yaw += 360;

    /* 4. Mahony 互补滤波修正: 仅当加速度处于标准重力有效带 (0.8g ~ 1.2g) 时才进行纠正 */
    /* Acceleration outside the gravity band is excluded from correction. */
    if (norm > 0.8f && norm < 1.2f)
    {
        /* 加速度向量归一化 */
        ax /= norm;
        ay /= norm;
        az /= norm;
        /* 由当前四元数推算机体坐标系下的重力理论投影向量 v */
        float vx = 2 * (q[1] * q[3] - q[0] * q[2]), vy = 2 * (q[0] * q[1] + q[2] * q[3]),
              vz = q[0] * q[0] - q[1] * q[1] - q[2] * q[2] + q[3] * q[3];
        /* 计算叉积误差 a x v 并乘以比例系数 Kp 修正角速度 */
        g[0] += IMU_CORRECTION_KP * (ay * vz - az * vy);
        g[1] += IMU_CORRECTION_KP * (az * vx - ax * vz);
        g[2] += IMU_CORRECTION_KP * (ax * vy - ay * vx);
    }

    /* 5. 一阶四元数微分方程更新 */
    float w = q[0], x = q[1], y = q[2], z = q[3], h = dt * 0.5f;
    q[0] += (-x * g[0] - y * g[1] - z * g[2]) * h;
    q[1] += (w * g[0] + y * g[2] - z * g[1]) * h;
    q[2] += (w * g[1] - x * g[2] + z * g[0]) * h;
    q[3] += (w * g[2] + x * g[1] - y * g[0]) * h;

    /* 6. 四元数归一化 */
    float n = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    if (!isfinite(n) || n < 0.0001f)
    {
        /* 发生数值奇异时重置滤波状态 */
        srv_imu_filter_calibrate_gyro();
        memset(out, 0, sizeof(*out));
        return;
    }
    for (unsigned i = 0; i < 4; i++)
        q[i] /= n;

    /* 7. 四元数转欧拉角 (单位: 度) */
    angles.roll =
        atan2f(2 * (q[0] * q[1] + q[2] * q[3]), 1 - 2 * (q[1] * q[1] + q[2] * q[2])) * DEG;
    angles.pitch = asinf(clamp1(2 * (q[0] * q[2] - q[3] * q[1]))) * DEG;
    angles.yaw = atan2f(2 * (q[0] * q[3] + q[1] * q[2]), 1 - 2 * (q[2] * q[2] + q[3] * q[3])) * DEG;
    if (angles.yaw < 0)
        angles.yaw += 360;
    *out = angles;
}
