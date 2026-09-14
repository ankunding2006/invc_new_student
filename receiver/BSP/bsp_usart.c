#include "bsp_usart.h"
#include <stddef.h>

void bsp_usart_init(void)
{
    /* 启动 USART1 接收中断压入环形队列；初始化 USART2 上位机转发端口 */
}

bool bsp_usart_transmit(rx_usart_port_t port, const uint8_t *p_data, uint16_t len)
{
    if (p_data == NULL || len == 0) {
        return false;
    }
    (void)port;
    return true;
}

uint16_t bsp_usart_receive(rx_usart_port_t port, uint8_t *p_buf, uint16_t max_len)
{
    if (p_buf == NULL || max_len == 0) {
        return 0;
    }
    (void)port;
    return 0;
}

void bsp_usart_forward_packet(const uint8_t *p_data, uint16_t len)
{
    if (p_data != NULL && len > 0) {
        bsp_usart_transmit(USART_PORT_PC_FORWARD, p_data, len);
    }
}
