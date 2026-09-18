#ifndef BSP_USART_H
#define BSP_USART_H
#include <stdint.h>
#include <stdbool.h>
typedef enum
{
    USART_PORT_WIRELESS = 0,
    USART_PORT_DEBUG = 1,
    USART_PORT_PC_FORWARD = 1
} usart_port_t;
typedef usart_port_t rx_usart_port_t;
void bsp_usart_init(void);
void bsp_usart_service(uint32_t now);
bool bsp_usart_tx_ready(usart_port_t port);
bool bsp_usart_transmit(usart_port_t port, const uint8_t *data, uint16_t len);
uint16_t bsp_usart_receive(usart_port_t port, uint8_t *out, uint16_t cap);
void bsp_usart_printf(usart_port_t port, const char *fmt, ...);
void bsp_usart_forward_packet(const uint8_t *data, uint16_t len);
uint32_t bsp_usart_rx_errors(usart_port_t port);
uint32_t bsp_usart_tx_drops(usart_port_t port);
void bsp_usart_flush_rx(usart_port_t port);
#endif
