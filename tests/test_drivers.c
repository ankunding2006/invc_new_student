#include "main.h"
#include "bsp_usart.h"
#include "bsp_imu.h"
#include "bsp_joystick.h"
#include "bsp_oled.h"
#include "bsp_key.h"
#include "bsp_switch.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void)
{
    bsp_usart_init();
    uint8_t data[] = {1, 2, 3}, out[256];
    assert(bsp_usart_transmit(USART_PORT_WIRELESS, data, 3));
    data[0] = 99;
    assert(mock_tx_data[0] == 1);
    assert(!bsp_usart_transmit(USART_PORT_WIRELESS, data, 3));
    huart1.gState = 0;
    HAL_UART_TxCpltCallback(&huart1);
    assert(bsp_usart_transmit(USART_PORT_WIRELESS, data, 3));
    mock_now = 101;
    bsp_usart_service(mock_now);
    assert(huart1.gState == 0);
    for (unsigned i = 0; i < 300; i++)
    {
        *mock_rx_data[0] = (uint8_t)i;
        HAL_UART_RxCpltCallback(&huart1);
    }
    assert(bsp_usart_rx_errors(USART_PORT_WIRELESS) == 45);
    assert(bsp_usart_receive(USART_PORT_WIRELESS, out, sizeof(out)) == 255);
    for (unsigned i = 0; i < 255; i++)
        assert(out[i] == (uint8_t)i);
    HAL_UART_ErrorCallback(&huart1);
    bsp_usart_service(mock_now);
    assert(bsp_usart_rx_errors(USART_PORT_WIRELESS) == 46);
    assert(!bsp_usart_transmit((usart_port_t)2, data, 3));
    bsp_joystick_init();
    assert(!bsp_joystick_ready());
    for (unsigned i = 0; i < 256; i++)
        mock_adc_data[i] = 2048;
    HAL_ADC_ConvHalfCpltCallback(&hadc1);
    joystick_data_t joy;
    bsp_joystick_get_data(&joy);
    assert(joy.is_centered && joy.x_voltage_mv == 1650);
    for (unsigned i = 0; i < 128; i += 2)
    {
        mock_adc_data[i] = 0;
        mock_adc_data[i + 1] = 4095;
    }
    HAL_ADC_ConvHalfCpltCallback(&hadc1);
    bsp_joystick_get_data(&joy);
    assert(joy.x_mapped == -1000 && joy.y_mapped == 1000);
    HAL_ADC_ErrorCallback(&hadc1);
    assert(!bsp_joystick_ready());
    bsp_key_init();
    GPIOB->IDR &= ~KEY1_Pin;
    mock_now = 200;
    bsp_key_tick_10ms();
    mock_now = 220;
    bsp_key_tick_10ms();
    assert(bsp_key_get_mask() == 1);
    bsp_switch_init();
    GPIOB->IDR |= SW1_Pin;
    GPIOB->IDR &= ~SW2_Pin;
    assert(bsp_switch_get_mask() == 3);
    mock_now += 59;
    assert(bsp_switch_get_mask() == 3);
    mock_now++;
    assert(bsp_switch_get_mask() == 1);
    assert(!bsp_switch_get_state((switch_id_t)-1));
    mock_registers[0x75] = 0x70;
    mock_now = 0;
    assert(bsp_imu_init());
    assert(bsp_imu_get_status() == IMU_STATUS_STARTING);
    for (mock_now = 0; mock_now < 500; mock_now += 5)
        bsp_imu_service(mock_now);
    assert(bsp_imu_get_status() == IMU_STATUS_OK);
    assert(mock_registers[0x1B] == 0x18 && mock_registers[0x1C] == 0x10 &&
           mock_registers[0x19] == 19);
    mock_registers[0x3A] = 1;
    mock_registers[0x3F] = 0x10;
    imu_raw_t raw;
    assert(bsp_imu_read_raw(&raw) && raw.az == 4096);
    assert(GPIOA->IDR & MPU_CS_Pin);
    mock_spi_result = HAL_TIMEOUT;
    assert(!bsp_imu_read_raw(&raw) && raw.az == 0);
    assert(GPIOA->IDR & MPU_CS_Pin);
    assert(bsp_imu_get_status() == IMU_STATUS_TIMEOUT);
    mock_spi_result = HAL_OK;
    mock_now += 1100;
    bsp_imu_service(mock_now);
    assert(bsp_imu_get_status() == IMU_STATUS_STARTING);
    bsp_oled_init();
    mock_now += 100;
    bsp_oled_service(mock_now);
    assert(bsp_oled_ready());
    bsp_oled_draw_pixel(127, 7, 1);
    bsp_oled_draw_pixel(128, 64, 1);
    bsp_oled_update_slice();
    assert(mock_i2c_len == 129 && mock_i2c_data[128] == 0x80 && mock_i2c_data[0] == 0x40);
    bsp_oled_draw_pixel(1, 8, 1);
    bsp_oled_update_slice();
    assert(mock_i2c_data[2] == 1);
    bsp_oled_show_string(120, 60, "BOUNDARY", 12, 1);
    bsp_oled_draw_line(0, 0, 127, 63, 1);
    mock_i2c_result = HAL_TIMEOUT;
    bsp_oled_update_slice();
    assert(!bsp_oled_ready());
    mock_i2c_result = HAL_OK;
    mock_now += 1000;
    bsp_oled_service(mock_now);
    assert(bsp_oled_ready());
    bsp_oled_bus_unlock();
    assert(!bsp_oled_ready());
    for (unsigned i = 0; i < 50; i++)
        bsp_oled_service(++mock_now);
    assert(bsp_oled_ready());
    puts("Step 12: UART ownership/overflow/recovery, ADC, GPIO, SPI init/failure, OLED "
         "page/bounds/recovery PASS");
    return 0;
}
