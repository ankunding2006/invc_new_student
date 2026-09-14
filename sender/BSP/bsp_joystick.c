#include "bsp_joystick.h"
#include <stddef.h>

void bsp_joystick_init(void)
{
    /* 启动 ADC1 DMA 规则通道循环采样 (PA0: IN0, PA1: IN1) */
}

void bsp_joystick_calibrate_zero(void)
{
    /* 采样摇杆中位静止值作为零偏基准 */
}

void bsp_joystick_get_data(joystick_data_t *p_data)
{
    if (p_data == NULL) {
        return;
    }
    p_data->x_mapped = 0;
    p_data->y_mapped = 0;
    p_data->x_voltage_mv = 1650;
    p_data->y_voltage_mv = 1650;
    p_data->is_centered = true;
}
