#ifndef __BSP_USART_H
#define __BSP_USART_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    USART_PORT_WIRELESS = 0,    /* USART1: PA9/PA10 115200 8N1 (无线透明接收通道) */
    USART_PORT_PC_FORWARD       /* USART2: PA2/PA3 115200 8N1 (向上位机高速转发通道) */
} rx_usart_port_t;

void     bsp_usart_init(void);
bool     bsp_usart_transmit(rx_usart_port_t port, const uint8_t *p_data, uint16_t len);
uint16_t bsp_usart_receive(rx_usart_port_t port, uint8_t *p_buf, uint16_t max_len);
void     bsp_usart_forward_packet(const uint8_t *p_data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_USART_H */
