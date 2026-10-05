/**
 * @file bsp_imu.c
 * @brief MPU-6500 SPI 通信驱动、分阶段状态机与轴向映射实现
 */

#include "bsp_imu.h"
#include "system_config.h"
#include "spi.h"
#include "main.h"
#include <string.h>

/* RM-MPU-6500A-00 sections 4.4-4.8,4.15-4.17,4.32-4.34,4.37.
 * Raw-register path only: no DMP firmware or vendor DMP functions compiled.
 * All accesses use 72MHz/128 = 562.5kHz (<1MHz register-access limit). */

/** @brief 坐标轴绝对值宏 */
#define AXIS_ABS(x) ((x) < 0 ? -(x) : (x))

/** @brief 编译期断言: 校验坐标轴配置数值是否在 1~3 范围内 */
typedef char check_axis_bounds[(AXIS_ABS(IMU_BODY_X) >= 1 && AXIS_ABS(IMU_BODY_X) <= 3 &&
                                AXIS_ABS(IMU_BODY_Y) >= 1 && AXIS_ABS(IMU_BODY_Y) <= 3 &&
                                AXIS_ABS(IMU_BODY_Z) >= 1 && AXIS_ABS(IMU_BODY_Z) <= 3)
                                   ? 1
                                   : -1];

/** @brief 编译期断言: 校验各轴向是否互不重复 */
typedef char check_axis_unique[(AXIS_ABS(IMU_BODY_X) != AXIS_ABS(IMU_BODY_Y) &&
                                AXIS_ABS(IMU_BODY_X) != AXIS_ABS(IMU_BODY_Z) &&
                                AXIS_ABS(IMU_BODY_Y) != AXIS_ABS(IMU_BODY_Z))
                                   ? 1
                                   : -1];

/* A proper rotation has determinant +1; reject mirrored coordinate systems. */
/** @brief 计算轴向置换奇偶性 */
#define AXIS_PARITY                                                                                \
    ((AXIS_ABS(IMU_BODY_X) - AXIS_ABS(IMU_BODY_Y)) *                                               \
     (AXIS_ABS(IMU_BODY_Y) - AXIS_ABS(IMU_BODY_Z)) *                                               \
     (AXIS_ABS(IMU_BODY_Z) - AXIS_ABS(IMU_BODY_X)))

/** @brief 编译期断言: 确保定义的机体坐标系严格符合右手定则 (行列式为正) */
typedef char check_right_handed[(AXIS_PARITY * IMU_BODY_X * IMU_BODY_Y * IMU_BODY_Z > 0) ? 1 : -1];

/** @brief IMU 硬件当前状态 */
static imu_status_t status = IMU_STATUS_NOT_INITIALIZED;
/** @brief 异步初始化状态机阶段编号与寄存器配置游标 */
static uint8_t phase, config_index;
/** @brief 各状态阶段计时戳、重试时间戳、健康探活时间戳与采样时间戳 */
static uint32_t since, retry_at, health_at, last_sample;

/**
 * @brief MPU6500 核心配置寄存器与写入值表:
 *  - 0x6B (PWR_MGMT_1) = 0x01: 解除睡眠，时钟源选择最佳的 Auto Select (陀螺仪PLL)
 *  - 0x6C (PWR_MGMT_2) = 0x00: 加速度与陀螺仪所有 6 轴全部使能
 *  - 0x6A (USER_CTRL)  = 0x10: 禁用 I2C 从机模式，强制仅使能 SPI 模式
 *  - 0x1A (CONFIG)     = 0x04: 低通滤波器带宽 DLPF_CFG = 4 (陀螺仪 20Hz 滤波)
 *  - 0x1B (GYRO_CONFIG)= 0x18: 陀螺仪满量程 ±2000 dps (灵敏度 16.4 LSB/dps)
 *  - 0x1C (ACCEL_CONFIG)=0x10: 加速度计满量程 ±8g (灵敏度 4096 LSB/g)
 *  - 0x1D (ACCEL_CONFIG2)=0x04: 加速度计数字低通滤波 21.2Hz
 *  - 0x19 (SMPLRT_DIV) = 19: 采样率分频 = 1kHz / (1 + 19) = 50Hz (与遥测周期 20ms 一致)
 *  - 0x23 (FIFO_EN)    = 0: 禁用 FIFO，直接读实时寄存器
 *  - 0x38 (INT_ENABLE) = 0: 禁用硬件中断引脚
 */
static const uint8_t config[][2] = {{0x6B, 0x01}, {0x6C, 0x00}, {0x6A, 0x10}, {0x1A, 0x04},
                                    {0x1B, 0x18}, {0x1C, 0x10}, {0x1D, 0x04}, {0x19, 19},
                                    {0x23, 0},    {0x38, 0}};

/**
 * @brief 通过 SPI 全双工传输读写 MPU6500 寄存器
 * @param[in]     reg  寄存器地址
 * @param[in,out] data 读写数据缓冲区
 * @param[in]     len  传输数据字节数 (<= 14)
 * @param[in]     read true 为读取 (最高位置 1); false 为写入 (最高位为 0)
 * @return true 传输成功; false 通信超时或失败
 */
static bool transfer(uint8_t reg, uint8_t *data, uint8_t len, bool read)
{
    uint8_t tx[15] = {0}, rx[15] = {0};
    if (len > 14 || !data)
        return false;
    /* SPI 读取命令最高位需置 1 (0x80) */
    tx[0] = read ? (reg | 0x80U) : reg;
    if (!read)
        memcpy(tx + 1, data, len);
    /* 拉低片选使能器件 */
    HAL_GPIO_WritePin(MPU_CS_GPIO_Port, MPU_CS_Pin, GPIO_PIN_RESET);
    HAL_StatusTypeDef rc = HAL_SPI_TransmitReceive(&hspi1, tx, rx, (uint16_t)(len + 1), 2);
    /* 拉高片选释放总线 */
    HAL_GPIO_WritePin(MPU_CS_GPIO_Port, MPU_CS_Pin, GPIO_PIN_SET);
    if (rc != HAL_OK)
    {
        status = IMU_STATUS_TIMEOUT;
        phase = 0;
        retry_at = HAL_GetTick();
        return false;
    }
    if (read)
        memcpy(data, rx + 1, len);
    return true;
}

/**
 * @brief 读取并校验 MPU6500 芯片 ID
 * @return true ID 为 0x70 确认匹配; false 匹配失败或通信异常
 */
bool bsp_imu_check_id(void)
{
    uint8_t id = 0;
    if (!transfer(0x75, &id, 1, true))
        return false;
    if (id != 0x70)
    {
        status = IMU_STATUS_DEV_NOT_FOUND;
        phase = 0;
        retry_at = HAL_GetTick();
        return false;
    }
    return true;
}

/**
 * @brief 开启 MPU-6500 异步初始化状态机
 * @return true 启动成功; false SPI 硬件初始化失败
 */
bool bsp_imu_init(void)
{
    /* 设置 SPI 波特率分频为 128 (72MHz/128 = 562.5kHz)，安全低于 1MHz 上限 */
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_128;
    if (HAL_SPI_Init(&hspi1) != HAL_OK)
    {
        status = IMU_STATUS_TIMEOUT;
        retry_at = HAL_GetTick();
        return false;
    }
    status = IMU_STATUS_STARTING;
    phase = 1;
    config_index = 0;
    since = HAL_GetTick();
    retry_at = since;
    return true;
}

/**
 * @brief 恢复 IMU 状态机
 * @return true 启动成功
 */
bool bsp_imu_recover(void)
{
    return bsp_imu_init();
}

/**
 * @brief 获取 IMU 当前状态
 * @return 状态枚举
 */
imu_status_t bsp_imu_get_status(void)
{
    return status;
}

/**
 * @brief IMU 后台异步状态机调度与周期性探活
 * @param[in] now 当前时间戳 (毫秒)
 */
void bsp_imu_service(uint32_t now)
{
    /* 1. 若处于正常 OK 状态，每隔 1000ms 检查一次芯片 ID 执行心跳探活 */
    if (status == IMU_STATUS_OK)
    {
        if ((uint32_t)(now - health_at) >= 1000)
        {
            health_at = now;
            (void)bsp_imu_check_id();
        }
        return;
    }
    /* 若初始化失败重置到阶段 0，等待 1000ms 后重新发起初始化 */
    if (!phase)
    {
        if ((uint32_t)(now - retry_at) >= 1000)
            (void)bsp_imu_init();
        return;
    }
    /* 阶段 1: 等待芯片上电稳定 100ms，验证器件 ID，随后发送软件复位命令 (PWR_MGMT_1=0x80) */
    if (phase == 1)
    {
        if ((uint32_t)(now - since) < 100)
            return;
        if (!bsp_imu_check_id())
            return;
        uint8_t reset = 0x80;
        if (transfer(0x6B, &reset, 1, false))
        {
            phase = 2;
            since = now;
        }
        return;
    }
    /* 阶段 2: 等待软件复位生效完成 (延时 100ms) */
    if (phase == 2)
    {
        if ((uint32_t)(now - since) < 100)
            return;
        phase = 3;
    }
    /* 阶段 3: 逐一写入配置寄存器并立即回读校验 */
    if (phase == 3)
    {
        if (config_index < sizeof(config) / sizeof(config[0]))
        {
            uint8_t value = config[config_index][1];
            if (!transfer(config[config_index][0], &value, 1, false))
                return;
            uint8_t check = 0;
            if (!transfer(config[config_index][0], &check, 1, true))
                return;
            /* 回读校验失败，标记设备未找到并重新开始 */
            if (check != value)
            {
                status = IMU_STATUS_DEV_NOT_FOUND;
                phase = 0;
                retry_at = now;
                return;
            }
            config_index++;
            return;
        }
        phase = 4;
        since = now;
        return;
    }
    /* 阶段 4: 等待陀螺仪 PLL 时钟与滤波器完全稳定 (100ms)，正式标记 OK 投入使用 */
    if (phase == 4 && (uint32_t)(now - since) >= 100)
    {
        status = IMU_STATUS_OK;
        phase = 0;
        health_at = last_sample = now;
    }
}

/**
 * @brief 从大端序 (Big-Endian) 字节流解析 16 位有符号整数
 * @param[in] p 指向 2 字节大端序缓冲区的指针
 * @return 还原出的 16 位有符号整数
 */
static int16_t be16(const uint8_t *p)
{
    uint16_t v = (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
    return (int16_t)((v & 0x8000U) ? (int32_t)v - 65536 : (int32_t)v);
}

/**
 * @brief 内部轴向重映射与正负号取反
 * @param[in] v    三轴原始数据数组 [X, Y, Z]
 * @param[in] axis 目标轴定义 (+1, -1, +2, -2, +3, -3)
 * @return 重映射并限幅后的单轴数值
 */
static int16_t map_axis(const int16_t v[3], int axis)
{
    int32_t n = v[(axis < 0 ? -axis : axis) - 1];
    if (axis < 0)
        n = -n;
    return (int16_t)(n > 32767 ? 32767 : n);
}

/**
 * @brief 从 MPU-6500 读取 14 字节原始六轴数据
 * @param[out] out 接收结构体指针
 * @return true 读取成功; false 未就绪或超时
 */
bool bsp_imu_read_raw(imu_raw_t *out)
{
    if (!out)
        return false;
    memset(out, 0, sizeof(*out));
    if (status != IMU_STATUS_OK)
        return false;
    uint8_t ready = 0, data[14];
    /* 读取中断状态寄存器 INT_STATUS (0x3A) 检查 DATA_RDY_INT 标志位 (bit0) */
    if (!transfer(0x3A, &ready, 1, true))
        return false;
    /* 若数据未就绪，且超过 200ms 未产生新采样，则判定为超时故障 */
    if (!(ready & 1))
    {
        if ((uint32_t)(HAL_GetTick() - last_sample) > 200)
        {
            status = IMU_STATUS_TIMEOUT;
            phase = 0;
            retry_at = HAL_GetTick();
        }
        return false;
    }
    /* 突发读取 14 字节数据 (0x3B~0x48: ACCEL_X/Y/Z, TEMP, GYRO_X/Y/Z) */
    if (!transfer(0x3B, data, 14, true))
        return false;
    /* 按照大端序解析 3 轴加速度与 3 轴角速度 (跳过温度寄存器) */
    int16_t a[3] = {be16(data), be16(data + 2), be16(data + 4)},
            g[3] = {be16(data + 8), be16(data + 10), be16(data + 12)};
    /* 根据载体机体坐标系定义 (IMU_BODY_X/Y/Z) 重映射输出轴 */
    out->ax = map_axis(a, IMU_BODY_X);
    out->ay = map_axis(a, IMU_BODY_Y);
    out->az = map_axis(a, IMU_BODY_Z);
    out->gx = map_axis(g, IMU_BODY_X);
    out->gy = map_axis(g, IMU_BODY_Y);
    out->gz = map_axis(g, IMU_BODY_Z);
    last_sample = HAL_GetTick();
    return true;
}
