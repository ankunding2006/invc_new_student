#ifndef BSP_IMU_H
#define BSP_IMU_H

/**
 * @file bsp_imu.h
 * @brief MPU-6500 六轴惯性传感器 (SPI1 接口) 驱动与异步初始化状态机
 * 
 * 架构特点:
 * 1. 采用纯寄存器直接配置 (无臃肿的 DMP 固件，响应确定性高);
 * 2. SPI 通信时钟严格配置为 72MHz/128 = 562.5kHz (< 1MHz 寄存器访问上限);
 * 3. 异步分阶段状态机初始化 (非阻塞延时等待传感器内部上电与滤波收敛);
 * 4. 周期性 WHO_AM_I 探活自愈与超时重连。
 */

#include <stdbool.h>
#include <stdint.h>
#include "imu_types.h"

/**
 * @brief IMU 硬件状态枚举
 */
typedef enum
{
    IMU_STATUS_OK,              /**< 正常工作，通信与数据就绪 */
    IMU_STATUS_DEV_NOT_FOUND,   /**< 器件未找到 (WHO_AM_I 寄存器读数不为 0x70) */
    IMU_STATUS_TIMEOUT,         /**< SPI 通信超时或数据准备超时 */
    IMU_STATUS_NOT_INITIALIZED, /**< 尚未初始化 */
    IMU_STATUS_STARTING         /**< 正在异步执行分步配置初始化流程中 */
} imu_status_t;

/**
 * @brief 启动 IMU 硬件异步初始化流程
 * @return true 成功启动状态机; false SPI 初始化失败
 * @note 此函数仅开启第一阶段并发送复位命令，必须在主循环中持续调用 bsp_imu_service 直至返回 OK。
 */
bool bsp_imu_init(void);

/**
 * @brief IMU 异步初始化与定期探活维护任务
 * @param[in] now 当前时间戳 (毫秒)
 */
void bsp_imu_service(uint32_t now);

/**
 * @brief 读取并核验 MPU6500 硬件器件 ID (WHO_AM_I 寄存器 0x75)
 * @return true ID 确认为 0x70; false 读取超时或芯片型号不符
 */
bool bsp_imu_check_id(void);

/**
 * @brief 从 MPU-6500 突发读取 14 字节原始传感器采样数据并重映射轴向
 * @param[out] out 接收 6 轴加速度与角速度原始读数的结构体指针
 * @return true 数据读取成功; false 未就绪、未完成初始化或通信超时
 */
bool bsp_imu_read_raw(imu_raw_t *out);

/**
 * @brief 查询 IMU 当前硬件状态
 * @return 状态枚举值
 */
imu_status_t bsp_imu_get_status(void);

/**
 * @brief 触发 IMU 故障重连自愈
 * @return true 成功重启; false 失败
 */
bool bsp_imu_recover(void);

#endif
