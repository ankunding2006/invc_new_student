#include "bsp_usart.h"
#include "srv_ring_buffer.h"
#include "usart.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
typedef struct
{
    ring_buffer_t rx;
    uint8_t storage[256], rx_byte, tx[256];
    volatile bool tx_busy, recover;
    volatile uint32_t errors;
    uint32_t tx_started, drops;
} uart_port_state_t;
static uart_port_state_t ports[2];
static UART_HandleTypeDef *handle(unsigned port)
{
    return port ? &huart2 : &huart1;
}
static int index_of(UART_HandleTypeDef *h)
{
    return h == &huart1 ? 0 : h == &huart2 ? 1 : -1;
}
void bsp_usart_init(void)
{
    HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
    HAL_NVIC_SetPriority(USART1_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    HAL_NVIC_SetPriority(USART2_IRQn, 3, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
    for (unsigned i = 0; i < 2; i++)
    {
        memset(&ports[i], 0, sizeof(ports[i]));
        srv_ring_buffer_init(&ports[i].rx, ports[i].storage, sizeof(ports[i].storage));
        if (HAL_UART_Receive_IT(handle(i), &ports[i].rx_byte, 1) != HAL_OK)
        {
            ports[i].recover = true;
            ports[i].errors++;
        }
    }
}
bool bsp_usart_tx_ready(usart_port_t port)
{
    return (unsigned)port < 2 && !ports[port].tx_busy &&
           handle(port)->gState == HAL_UART_STATE_READY;
}
bool bsp_usart_transmit(usart_port_t port, const uint8_t *data, uint16_t len)
{
    if ((unsigned)port >= 2 || !data || !len || len > 256)
        return false;
    uart_port_state_t *s = &ports[port];
    if (s->tx_busy || handle(port)->gState != HAL_UART_STATE_READY)
    {
        s->drops++;
        return false;
    }
    memcpy(s->tx, data, len);
    s->tx_started = HAL_GetTick();
    s->tx_busy = true;
    if (HAL_UART_Transmit_IT(handle(port), s->tx, len) != HAL_OK)
    {
        s->tx_busy = false;
        s->drops++;
        return false;
    }
    return true;
}
uint16_t bsp_usart_receive(usart_port_t port, uint8_t *out, uint16_t cap)
{
    if ((unsigned)port >= 2 || !out)
        return 0;
    uint16_t n = 0;
    while (n < cap && srv_ring_buffer_pop(&ports[port].rx, out + n))
        n++;
    return n;
}
void bsp_usart_service(uint32_t now)
{
    for (unsigned i = 0; i < 2; i++)
    {
        uart_port_state_t *s = &ports[i];
        if (s->tx_busy && (uint32_t)(now - s->tx_started) > 100)
        {
            if (HAL_UART_AbortTransmit(handle(i)) == HAL_OK)
                s->tx_busy = false;
            s->drops++;
            s->tx_started = now;
        }
        if (s->recover)
        {
            /* HAL abort is register-only here (no DMA). ISR flags the request. */
            s->recover = false;
            if (HAL_UART_AbortReceive(handle(i)) != HAL_OK)
            {
                s->recover = true;
                continue;
            }
            __HAL_UART_CLEAR_OREFLAG(handle(i));
            if (HAL_UART_Receive_IT(handle(i), &s->rx_byte, 1) != HAL_OK)
                s->recover = true;
        }
    }
}
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *h)
{
    int i = index_of(h);
    if (i < 0)
        return;
    (void)srv_ring_buffer_push(&ports[i].rx, ports[i].rx_byte);
    if (HAL_UART_Receive_IT(h, &ports[i].rx_byte, 1) != HAL_OK)
    {
        ports[i].recover = true;
        ports[i].errors++;
    }
}
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *h)
{
    int i = index_of(h);
    if (i >= 0)
        ports[i].tx_busy = false;
}
void HAL_UART_ErrorCallback(UART_HandleTypeDef *h)
{
    int i = index_of(h);
    if (i >= 0)
    {
        ports[i].errors++;
        ports[i].recover = true;
    }
}
uint32_t bsp_usart_rx_errors(usart_port_t p)
{
    return (unsigned)p < 2 ? ports[p].errors + srv_ring_buffer_get_overflow_count(&ports[p].rx) : 0;
}
uint32_t bsp_usart_tx_drops(usart_port_t p)
{
    return (unsigned)p < 2 ? ports[p].drops : 0;
}
void bsp_usart_flush_rx(usart_port_t p)
{
    if ((unsigned)p < 2)
        srv_ring_buffer_clear(&ports[p].rx);
}
void bsp_usart_printf(usart_port_t p, const char *fmt, ...)
{
    static char text[256];
    if (!fmt)
        return;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);
    if (n > 0)
    {
        if (n >= (int)sizeof(text))
            n = (int)sizeof(text) - 1;
        (void)bsp_usart_transmit(p, (const uint8_t *)text, (uint16_t)n);
    }
}
void bsp_usart_forward_packet(const uint8_t *p, uint16_t n)
{
    (void)bsp_usart_transmit(USART_PORT_PC_FORWARD, p, n);
}
