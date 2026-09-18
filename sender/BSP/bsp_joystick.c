#include "bsp_joystick.h"
#include "srv_input.h"
#include "system_config.h"
#include "adc.h"
#include <string.h>
static volatile uint16_t dma_samples[256];
static volatile uint32_t epoch;
static volatile uint8_t complete_half;
static volatile bool failed;
static bool active, center_valid, sample_valid;
static uint16_t last_x, last_y;
static uint16_t center_x = 1650, center_y = 1650;
static uint32_t retry_ms;
void bsp_joystick_init(void)
{
    epoch = 0;
    failed = false;
    active = false;
    center_valid = false;
    sample_valid = false;
    retry_ms = HAL_GetTick();
    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 2, 0);
    if (HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK)
    {
        failed = true;
        return;
    }
    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)(void *)dma_samples, 256) != HAL_OK)
    {
        failed = true;
        return;
    }
    active = true;
}
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *h)
{
    if (h == &hadc1)
    {
        complete_half = 0;
        epoch++;
    }
}
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *h)
{
    if (h == &hadc1)
    {
        complete_half = 1;
        epoch++;
    }
}
void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *h)
{
    if (h == &hadc1)
        failed = true;
}
bool bsp_joystick_ready(void)
{
    return active && !failed && sample_valid;
}
static bool voltage(uint16_t *x, uint16_t *y)
{
    if (!active || failed || epoch == 0)
        return false;
    uint32_t before = epoch, sx = 0, sy = 0;
    unsigned base = complete_half ? 128 : 0;
    /* ISR only publishes a half index; averaging runs in main context. */
    for (unsigned i = base; i < base + 128; i += 2)
    {
        sx += dma_samples[i];
        sy += dma_samples[i + 1];
    }
    if (before != epoch || failed)
        return false;
    *x = (uint16_t)((sx * JOYSTICK_VREF_MV + 64U * 4095U / 2U) / (64U * 4095U));
    *y = (uint16_t)((sy * JOYSTICK_VREF_MV + 64U * 4095U / 2U) / (64U * 4095U));
    last_x = *x;
    last_y = *y;
    sample_valid = true;
    return true;
}
void bsp_joystick_calibrate_zero(void)
{
    uint16_t x, y;
    if (voltage(&x, &y) && x > 500 && x < 2800 && y > 500 && y < 2800)
    {
        center_x = x;
        center_y = y;
        center_valid = true;
    }
}
void bsp_joystick_get_data(joystick_data_t *out)
{
    if (!out)
        return;
    uint16_t x, y;
    if (!voltage(&x, &y))
    {
        /* A DMA boundary invalidates this average, not the last coherent sample. */
        if (!bsp_joystick_ready())
        {
            memset(out, 0, sizeof(*out));
            return;
        }
        x = last_x;
        y = last_y;
    }
    if (!center_valid)
        bsp_joystick_calibrate_zero();
    out->x_voltage_mv = x;
    out->y_voltage_mv = y;
    out->x_mapped = srv_joystick_map(x, center_x);
    out->y_mapped = srv_joystick_map(y, center_y);
    out->is_centered = out->x_mapped == 0 && out->y_mapped == 0;
}
void bsp_joystick_service(uint32_t now)
{
    if ((failed || !active) && (uint32_t)(now - retry_ms) >= 1000)
    {
        retry_ms = now;
        if (HAL_ADC_Stop_DMA(&hadc1) == HAL_OK)
            bsp_joystick_init();
    }
}
