#ifndef MOCK_HAL_H
#define MOCK_HAL_H
#include <stdint.h>
#include <stdbool.h>
typedef enum
{
    HAL_OK,
    HAL_ERROR,
    HAL_BUSY,
    HAL_TIMEOUT
} HAL_StatusTypeDef;
typedef enum
{
    GPIO_PIN_RESET,
    GPIO_PIN_SET
} GPIO_PinState;
typedef struct
{
    uint32_t IDR;
} GPIO_TypeDef;
extern GPIO_TypeDef mock_a, mock_b;
#define GPIOA (&mock_a)
#define GPIOB (&mock_b)
#define GPIO_PIN_6 (1U << 6)
#define GPIO_PIN_7 (1U << 7)
#define KEY1_Pin 1U
#define KEY2_Pin 2U
#define KEY3_Pin (1U << 10)
#define KEY4_Pin (1U << 11)
#define SW1_Pin (1U << 12)
#define SW2_Pin (1U << 13)
#define MPU_CS_Pin (1U << 4)
#define MPU_CS_GPIO_Port GPIOA
#define GPIO_MODE_OUTPUT_OD 1U
#define GPIO_SPEED_FREQ_HIGH 1U
#define SPI_BAUDRATEPRESCALER_128 128U
#define HAL_UART_STATE_READY 0U
#define SysTick_IRQn 0
#define USART1_IRQn 1
#define USART2_IRQn 2
#define DMA1_Channel1_IRQn 3
#define I2C_FLAG_BUSY 1U
typedef struct
{
    uint32_t Pin, Mode, Pull, Speed;
} GPIO_InitTypeDef;
typedef struct
{
    uint32_t gState;
} UART_HandleTypeDef;
typedef struct
{
    struct
    {
        uint32_t BaudRatePrescaler;
    } Init;
} SPI_HandleTypeDef;
typedef struct
{
    uint32_t unused;
} ADC_HandleTypeDef;
typedef struct
{
    uint32_t unused;
} I2C_HandleTypeDef;
extern UART_HandleTypeDef huart1, huart2;
extern SPI_HandleTypeDef hspi1;
extern ADC_HandleTypeDef hadc1;
extern I2C_HandleTypeDef hi2c1;
extern uint32_t mock_now;
extern bool mock_i2c_busy;
extern HAL_StatusTypeDef mock_spi_result, mock_i2c_result, mock_adc_result;
extern uint8_t mock_registers[128], *mock_tx_data, *mock_rx_data[2];
extern uint16_t mock_tx_len, *mock_adc_data;
extern uint8_t *mock_tx_ports[2];
extern uint16_t mock_tx_lengths[2];
extern uint8_t mock_i2c_data[129];
extern uint16_t mock_i2c_len;
extern unsigned mock_i2c_writes;
#define __HAL_I2C_GET_FLAG(h, f) (mock_i2c_busy)
#define __HAL_UART_CLEAR_OREFLAG(h) ((void)(h))
#define __HAL_RCC_I2C1_FORCE_RESET() ((void)0)
#define __HAL_RCC_I2C1_RELEASE_RESET() ((void)0)
uint32_t HAL_GetTick(void);
void HAL_NVIC_SetPriority(int irq, uint32_t p, uint32_t sub);
void HAL_NVIC_EnableIRQ(int irq);
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *h, uint8_t *p, uint16_t n);
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *h, uint8_t *p, uint16_t n);
HAL_StatusTypeDef HAL_UART_AbortTransmit(UART_HandleTypeDef *h);
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *h);
HAL_StatusTypeDef HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef *h);
HAL_StatusTypeDef HAL_ADC_Start_DMA(ADC_HandleTypeDef *h, uint32_t *p, uint32_t n);
HAL_StatusTypeDef HAL_ADC_Stop_DMA(ADC_HandleTypeDef *h);
HAL_StatusTypeDef HAL_SPI_Init(SPI_HandleTypeDef *h);
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *h, uint8_t *tx, uint8_t *rx,
                                          uint16_t n, uint32_t timeout);
void HAL_GPIO_WritePin(GPIO_TypeDef *p, uint16_t pin, GPIO_PinState state);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *p, uint16_t pin);
void HAL_GPIO_Init(GPIO_TypeDef *p, GPIO_InitTypeDef *cfg);
HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *h, uint16_t addr, uint8_t *p,
                                          uint16_t n, uint32_t timeout);
HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *h, uint16_t addr, uint32_t trials,
                                        uint32_t timeout);
HAL_StatusTypeDef HAL_I2C_Init(I2C_HandleTypeDef *h);
HAL_StatusTypeDef HAL_I2C_DeInit(I2C_HandleTypeDef *h);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *h);
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *h);
void HAL_UART_ErrorCallback(UART_HandleTypeDef *h);
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *h);
void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *h);
#endif
