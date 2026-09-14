#include "bsp_usart.h"
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>

void bsp_usart_init(void)
{
    /* 硬件 USART1 (PA9/PA10) 与 USART2 (PA2/PA3) 已由 CubeMX 完成初始化，此处启动中断接收 */
}

bool bsp_usart_transmit(usart_port_t port, const uint8_t *p_data, uint16_t len)
{
    if (p_data == NULL || len == 0) {
        return false;
    }
    (void)port;
    /* 调用 HAL_UART_Transmit 发送数据 */
    return true;
}

uint16_t bsp_usart_receive(usart_port_t port, uint8_t *p_buf, uint16_t max_len)
{
    if (p_buf == NULL || max_len == 0) {
        return 0;
    }
    (void)port;
    /* 从环形队列取出数据 */
    return 0;
}

void bsp_usart_printf(usart_port_t port, const char *fmt, ...)
{
    (void)port;
    (void)fmt;
}
