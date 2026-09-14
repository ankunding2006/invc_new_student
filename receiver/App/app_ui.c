#include "app_ui.h"
#include "bsp_oled.h"
#include <stddef.h>

void app_ui_init(void)
{
    bsp_oled_clear();
}

void app_ui_update(const telemetry_payload_t *p_telemetry, float freq_hz, float loss_rate_pct)
{
    (void)p_telemetry;
    (void)freq_hz;
    (void)loss_rate_pct;
    /* 排版渲染摇杆、按键、姿态角以及 Freq Hz / Loss % 指标骨架 */
}
