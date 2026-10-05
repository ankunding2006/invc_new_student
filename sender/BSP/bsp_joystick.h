#ifndef __BSP_JOYSTICK_H
#define __BSP_JOYSTICK_H

/**
 * @file bsp_joystick.h
 * @brief 双轴模拟摇杆 (ADC1 + DMA 循环采样) 硬件驱动与校准
 * 
 * 驱动架构:
 * 1. ADC1 双通道连续扫描 + DMA 双缓冲半传输/完成中断驱动；
 * 2. 64 样本均值滤波与 epoch 时钟戳防竞态保护；
 * 3. 静止中位参考电压自动校准；
 * 4. 故障检测与 1000ms 周期 DMA 重启自愈。
 */

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdbool.h>

    /**
     * @brief 摇杆数据结构体
     */
    typedef struct
    {
        int16_t x_mapped;      /* 归一化输出: -1000 ~ +1000 */
        int16_t y_mapped;      /* 归一化输出: -1000 ~ +1000 */
        uint16_t x_voltage_mv; /* 实际物理采样电压: 0 ~ 3300 mV */
        uint16_t y_voltage_mv; /* 实际物理采样电压: 0 ~ 3300 mV */
        bool is_centered;      /* 处于中心死区标志 */
    } joystick_data_t;

    /**
     * @brief 查询摇杆 ADC 采样是否就绪且数据有效
     * @return true 采样正常有效; false DMA未启动、出错或数据未就绪
     */
    bool bsp_joystick_ready(void);

    /**
     * @brief 摇杆后台维护服务 (ADC 异常监测与 1000ms 自动重启)
     * @param[in] now 当前时间戳 (毫秒)
     */
    void bsp_joystick_service(uint32_t now);

    /**
     * @brief 初始化 ADC1 校准并启动 256 样本循环 DMA 连续采集
     */
    void bsp_joystick_init(void);

    /**
     * @brief 将当前实测采样电压捕获记录为摇杆静止中位参考电压
     * @note 仅在采样电压处于 [500mV, 2800mV] 合理区间内才确认有效。
     */
    void bsp_joystick_calibrate_zero(void);

    /**
     * @brief 获取最新滤波并归一化后的摇杆坐标与电压数据
     * @param[out] p_data 输出保存摇杆数据的结构体指针
     */
    void bsp_joystick_get_data(joystick_data_t *p_data);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_JOYSTICK_H */
