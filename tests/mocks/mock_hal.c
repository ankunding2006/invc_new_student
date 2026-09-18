#include "main.h"
#include <assert.h>
#include <string.h>
GPIO_TypeDef mock_a = {0xFFFF}, mock_b = {0xFFFF};
UART_HandleTypeDef huart1, huart2;
SPI_HandleTypeDef hspi1;
ADC_HandleTypeDef hadc1;
I2C_HandleTypeDef hi2c1;
uint32_t mock_now;
bool mock_i2c_busy;
HAL_StatusTypeDef mock_spi_result = HAL_OK, mock_i2c_result = HAL_OK, mock_adc_result = HAL_OK;
uint8_t mock_registers[128], *mock_tx_data, *mock_rx_data[2];
uint16_t mock_tx_len, *mock_adc_data;
uint8_t *mock_tx_ports[2];
uint16_t mock_tx_lengths[2];
uint8_t mock_i2c_data[129];
uint16_t mock_i2c_len;
unsigned mock_i2c_writes;
uint32_t HAL_GetTick(void)
{
    return mock_now;
}
void HAL_NVIC_SetPriority(int i, uint32_t p, uint32_t sub)
{
    (void)i;
    (void)p;
    (void)sub;
}
void HAL_NVIC_EnableIRQ(int i)
{
    (void)i;
}
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *h, uint8_t *p, uint16_t n)
{
    assert(n == 1);
    mock_rx_data[h == &huart2] = p;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *h, uint8_t *p, uint16_t n)
{
    if (h->gState)
        return HAL_BUSY;
    mock_tx_data = p;
    mock_tx_len = n;
    mock_tx_ports[h == &huart2] = p;
    mock_tx_lengths[h == &huart2] = n;
    h->gState = 1;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_AbortTransmit(UART_HandleTypeDef *h)
{
    h->gState = 0;
    mock_tx_lengths[h == &huart2] = 0;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *h)
{
    (void)h;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef *h)
{
    (void)h;
    return mock_adc_result;
}
HAL_StatusTypeDef HAL_ADC_Start_DMA(ADC_HandleTypeDef *h, uint32_t *p, uint32_t n)
{
    (void)h;
    assert(n == 256);
    mock_adc_data = (uint16_t *)p;
    return mock_adc_result;
}
HAL_StatusTypeDef HAL_ADC_Stop_DMA(ADC_HandleTypeDef *h)
{
    (void)h;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_SPI_Init(SPI_HandleTypeDef *h)
{
    assert(h->Init.BaudRatePrescaler == 128);
    return mock_spi_result;
}
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *h, uint8_t *tx, uint8_t *rx,
                                          uint16_t n, uint32_t timeout)
{
    (void)h;
    assert(n <= 15 && timeout <= 2);
    assert(!(GPIOA->IDR & MPU_CS_Pin));
    if (mock_spi_result != HAL_OK)
        return mock_spi_result;
    uint8_t reg = tx[0] & 0x7F;
    if (tx[0] & 0x80)
    {
        for (unsigned i = 1; i < n; i++)
            rx[i] = mock_registers[reg + i - 1];
    }
    else
    {
        for (unsigned i = 1; i < n; i++)
            mock_registers[reg + i - 1] = tx[i];
        if (reg == 0x6B && tx[1] == 0x80)
        {
            memset(mock_registers, 0, 128);
            mock_registers[0x75] = 0x70;
        }
    }
    return HAL_OK;
}
void HAL_GPIO_WritePin(GPIO_TypeDef *p, uint16_t pin, GPIO_PinState s)
{
    if (s)
        p->IDR |= pin;
    else
        p->IDR &= ~pin;
}
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *p, uint16_t pin)
{
    return (p->IDR & pin) ? GPIO_PIN_SET : GPIO_PIN_RESET;
}
void HAL_GPIO_Init(GPIO_TypeDef *p, GPIO_InitTypeDef *c)
{
    (void)p;
    (void)c;
}
HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *h, uint16_t addr, uint8_t *p,
                                          uint16_t n, uint32_t t)
{
    (void)h;
    assert(addr == 0x78 || addr == 0x7A);
    assert(n <= 129 && t <= 4);
    mock_i2c_writes++;
    memcpy(mock_i2c_data, p, n);
    mock_i2c_len = n;
    return mock_i2c_result;
}
HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *h, uint16_t addr, uint32_t trials,
                                        uint32_t timeout)
{
    (void)h;
    (void)addr;
    (void)trials;
    (void)timeout;
    return mock_i2c_result;
}
HAL_StatusTypeDef HAL_I2C_Init(I2C_HandleTypeDef *h)
{
    (void)h;
    return mock_i2c_result;
}
HAL_StatusTypeDef HAL_I2C_DeInit(I2C_HandleTypeDef *h)
{
    (void)h;
    return HAL_OK;
}
