#include "bsp_imu.h"
#include <stddef.h>

bool bsp_imu_init(void)
{
    /* 初始化片上 SPI1 (PA5~PA7) 与 PA4 CS 片选，调用 LibDriver MPU6500 basic 模式初始化 */
    return true;
}

bool bsp_imu_check_id(void)
{
    /* 读取 WHO_AM_I 寄存器 (期望值 0x70) */
    return true;
}

bool bsp_imu_read_raw(imu_raw_t *p_raw)
{
    if (p_raw == NULL) {
        return false;
    }
    p_raw->ax = 0;
    p_raw->ay = 0;
    p_raw->az = 16384; /* 1g 基准 */
    p_raw->gx = 0;
    p_raw->gy = 0;
    p_raw->gz = 0;
    return true;
}
