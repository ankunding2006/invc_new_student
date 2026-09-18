#ifndef __BSP_JOYSTICK_H
#define __BSP_JOYSTICK_H

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

    bool bsp_joystick_ready(void);
    void bsp_joystick_service(uint32_t now);
    void bsp_joystick_init(void);
    void bsp_joystick_calibrate_zero(void);
    void bsp_joystick_get_data(joystick_data_t *p_data);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_JOYSTICK_H */
