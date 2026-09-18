#include "main.h"
#include "bsp_oled.h"
#include <string.h>
#ifdef TEST_SENDER
#include "app_sender.h"
#include "app_menu.h"
#include "srv_imu_filter.h"
#else
#include "app_receiver.h"
#include "srv_stats.h"
#endif
void sim_init(uint32_t now)
{
    mock_now = now;
    mock_a.IDR = mock_b.IDR = 0xFFFF;
    huart1.gState = huart2.gState = 0;
    mock_spi_result = mock_i2c_result = mock_adc_result = HAL_OK;
    mock_i2c_busy = false;
    memset(mock_registers, 0, 128);
    mock_registers[0x75] = 0x70;
    mock_registers[0x3A] = 1;
    mock_registers[0x3F] = 0x10;
    memset(mock_tx_lengths, 0, sizeof mock_tx_lengths);
#ifdef TEST_SENDER
    app_sender_init();
    for (unsigned i = 0; i < 256; i++)
        mock_adc_data[i] = 2048;
#else
    app_receiver_init();
#endif
}
void sim_step(uint32_t now)
{
    mock_now = now;
#ifdef TEST_SENDER
    mock_registers[0x3A] = 1;
    mock_registers[0x3F] = 0x10;
    HAL_ADC_ConvHalfCpltCallback(&hadc1);
    app_sender_task();
#else
    app_receiver_task();
#endif
}
void sim_receive(unsigned port, const uint8_t *data, unsigned n)
{
    if (port >= 2)
        return;
    for (unsigned i = 0; i < n; i++)
    {
        *mock_rx_data[port] = data[i];
        HAL_UART_RxCpltCallback(port ? &huart2 : &huart1);
    }
}
/* Completion is controlled by the test, allowing UART busy/backpressure tests. */
unsigned sim_take_tx(unsigned port, uint8_t *out, unsigned cap)
{
    if (port >= 2 || mock_tx_lengths[port] > cap)
        return 0;
    unsigned n = mock_tx_lengths[port];
    if (!n)
        return 0;
    memcpy(out, mock_tx_ports[port], n);
    mock_tx_lengths[port] = 0;
    UART_HandleTypeDef *h = port ? &huart2 : &huart1;
    h->gState = 0;
    HAL_UART_TxCpltCallback(h);
    return n;
}
void sim_set_keys(unsigned mask)
{
    const uint16_t bits[] = {KEY1_Pin, KEY2_Pin, KEY3_Pin, KEY4_Pin};
    for (unsigned i = 0; i < 4; i++)
        if (mask & (1U << i))
            mock_b.IDR &= ~bits[i];
        else
            mock_b.IDR |= bits[i];
}
void sim_set_joystick(uint16_t x, uint16_t y)
{
#ifdef TEST_SENDER
    for (unsigned i = 0; i < 256; i += 2)
    {
        mock_adc_data[i] = x;
        mock_adc_data[i + 1] = y;
    }
#else
    (void)x;
    (void)y;
#endif
}
void sim_fault(unsigned device, bool on)
{
    if (device == 0)
        mock_spi_result = on ? HAL_TIMEOUT : HAL_OK;
    else if (device == 1)
        mock_i2c_result = on ? HAL_TIMEOUT : HAL_OK;
}
void sim_screen(uint8_t *out)
{
    memcpy(out, bsp_oled_framebuffer(), 1024);
}
uint32_t sim_value(unsigned index)
{
#ifdef TEST_SENDER
    const sender_view_t *v = app_sender_view();
    switch (index)
    {
    case 0:
        return v->state;
    case 1:
        return v->acked;
    case 2:
        return v->retries;
    case 3:
        return v->expired;
    case 4:
        return (uint32_t)(int32_t)v->telemetry.joy_x_raw;
    case 5:
        return v->events;
    case 6:
        return srv_imu_filter_calibration_count();
    case 7:
        return app_menu_get_current_page();
    case 8:
        return app_menu_in_detail();
    case 9:
        return v->telemetry.key_mask;
    default:
        return 0;
    }
#else
    stats_metrics_t m;
    srv_stats_get_metrics(&m);
    switch (index)
    {
    case 0:
        return app_receiver_get_state();
    case 1:
        return m.total_received;
    case 2:
        return m.total_duplicate;
    case 3:
        return m.total_lost;
    case 4:
        return app_receiver_pc_drops();
    case 5:
        return (uint32_t)(int32_t)app_receiver_telemetry()->joy_x_raw;
    case 6:
        return (uint32_t)(m.freq_hz * 100);
    default:
        return 0;
    }
#endif
}
