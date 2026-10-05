#ifndef SRV_IMU_FILTER_H
#define SRV_IMU_FILTER_H

/**
 * @file srv_imu_filter.h
 * @brief 六轴姿态融合算法服务 (基于四元数的一阶 Mahony 互补滤波)
 * 
 * 核心功能:
 * 1. 静态检测与陀螺仪在线零偏标定 (基于 Welford 单遍方差算法)；
 * 2. 加速度计初始重力对齐；
 * 3. 基于四元数旋转矩阵的重力向量预测与加速度误差叉乘反馈纠正；
 * 4. 动态重力加速度有效区间门限检测 (0.8g ~ 1.2g)；
 * 5. 欧拉角转换与原始重力姿态角输出。
 */

#include <stdbool.h>
#include "imu_types.h"

/**
 * @brief 初始化姿态滤波解算器并触发重新标定流程
 * @param[in] sample_freq_hz 期望采样频率 (Hz)
 */
void srv_imu_filter_init(float sample_freq_hz);

/**
 * @brief 周期喂入原始 IMU 读数并更新姿态四元数与欧拉角
 * @param[in]  raw  IMU 原始采样数据指针 (ax, ay, az, gx, gy, gz)
 * @param[in]  dt_s 距上一帧的实际时间步长 (秒, 通常为 0.02s)
 * @param[out] out  输出融合解算后的欧拉角结构体指针 (Pitch, Roll, Yaw)
 */
void srv_imu_filter_update(const imu_raw_t *raw, float dt_s, imu_euler_t *out);

/**
 * @brief 重新触发陀螺仪零偏标定流程
 * @note 重置四元数为单位四元数 [1, 0, 0, 0]，清空历史均值与方差。
 */
void srv_imu_filter_calibrate_gyro(void);

/**
 * @brief 查询陀螺仪是否已完成静止零偏标定
 * @return true 标定完成; false 仍在采集静止样本或环境晃动
 */
bool srv_imu_filter_is_calibrated(void);

/**
 * @brief 获取当前已累积的有效静止标定样本数
 * @return 样本数 (0 ~ IMU_CAL_SAMPLES)
 */
unsigned srv_imu_filter_calibration_count(void);

/**
 * @brief 获取仅由加速度计几何计算得到的未滤波原始重力姿态角
 * @param[out] out 接收原始俯仰/横滚角结构体指针
 */
void srv_imu_filter_raw_angles(imu_euler_t *out);

#endif
