/**
 * @file bsp_usart.c
 * @brief 双串口驱动实现 (中断非阻塞收发、环形缓冲区缓冲与超时自愈)
 */

#include "bsp_usart.h"
#include "srv_ring_buffer.h"
#include "usart.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/**
 * @brief 单串口运行状态控制结构体
 */
typedef struct
{
    ring_buffer_t rx;                      /**< 接收环形缓冲区对象 */
    uint8_t storage[256], rx_byte, tx[256];/**< 接收缓冲区空间(256B)、单字节中断接收缓存、发送数据暂存(256B) */
    volatile bool tx_busy, recover;        /**< 发送正忙标志、需要触发接收错误恢复标志 */
    volatile uint32_t errors;              /**< 硬件错误累加计数 (ORE, NE, FE 等) */
    uint32_t tx_started, drops;            /**< 发送启动时刻时间戳、发送丢包计数 */
} uart_port_state_t;

/** @brief 两个串口端口 (0: 无线, 1: 调试/PC) 的状态实例 */
static uart_port_state_t ports[2];

/**
 * @brief 内部根据端口号返回对应的 HAL 库 UART 句柄
 * @param[in] port 端口编号 (0 为 huart1, 1 为 huart2)
 * @return 指向对应 UART_HandleTypeDef 的指针
 */
static UART_HandleTypeDef *handle(unsigned port)
{
    return port ? &huart2 : &huart1;
}

/**
 * @brief 内部根据 HAL 句柄查找对应的端口索引
 * @param[in] h UART 句柄指针
 * @return 0 或 1; 若不匹配返回 -1
 */
static int index_of(UART_HandleTypeDef *h)
{
    return h == &huart1 ? 0 : h == &huart2 ? 1 : -1;
}

/**
 * @brief 初始化串口硬件中断与环形缓冲区
 */
void bsp_usart_init(void)
{
    /* 配置中断优先级: SysTick 最高 (0,0), USART1 无线串口次之 (1,0), USART2 调试串口较低 (3,0) */
    HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
    HAL_NVIC_SetPriority(USART1_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    HAL_NVIC_SetPriority(USART2_IRQn, 3, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);

    /* 分别初始化两个串口的接收环形缓冲区，并开启单字节中断接收 */
    for (unsigned i = 0; i < 2; i++)
    {
        memset(&ports[i], 0, sizeof(ports[i]));
        srv_ring_buffer_init(&ports[i].rx, ports[i].storage, sizeof(ports[i].storage));
        /* 启动单字节非阻塞中断接收 */
        if (HAL_UART_Receive_IT(handle(i), &ports[i].rx_byte, 1) != HAL_OK)
        {
            ports[i].recover = true;
            ports[i].errors++;
        }
    }
}

/**
 * @brief 查询指定端口是否就绪可发送
 * @param[in] port 端口
 * @return true 空闲就绪; false 正忙
 */
bool bsp_usart_tx_ready(usart_port_t port)
{
    return (unsigned)port < 2 && !ports[port].tx_busy &&
           handle(port)->gState == HAL_UART_STATE_READY;
}

/**
 * @brief 启动中断发送数据块
 * @param[in] port 逻辑端口
 * @param[in] data 数据指针
 * @param[in] len  数据长度 (<= 256)
 * @return true 启动发送成功; false 繁忙或失败
 */
bool bsp_usart_transmit(usart_port_t port, const uint8_t *data, uint16_t len)
{
    if ((unsigned)port >= 2 || !data || !len || len > 256)
        return false;
    uart_port_state_t *s = &ports[port];
    /* 若当前正在发送中或硬件未就绪，丢弃本帧 */
    if (s->tx_busy || handle(port)->gState != HAL_UART_STATE_READY)
    {
        s->drops++;
        return false;
    }
    /* 拷贝数据至本地发送缓冲区，标记正忙并启动中断发送 */
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

/**
 * @brief 从指定端口的接收环形缓冲区中提取字节
 * @param[in]  port 端口号
 * @param[out] out  输出缓冲区
 * @param[in]  cap  提取容量上限
 * @return 读取到的字节数
 */
uint16_t bsp_usart_receive(usart_port_t port, uint8_t *out, uint16_t cap)
{
    if ((unsigned)port >= 2 || !out)
        return 0;
    uint16_t n = 0;
    while (n < cap && srv_ring_buffer_pop(&ports[port].rx, out + n))
        n++;
    return n;
}

/**
 * @brief 串口周期后台维护服务 (发送超时看门狗与接收错误中断重启)
 * @param[in] now 当前时间戳 (毫秒)
 */
void bsp_usart_service(uint32_t now)
{
    for (unsigned i = 0; i < 2; i++)
    {
        uart_port_state_t *s = &ports[i];
        /* 1. 发送看门狗: 发送中持续超过 100ms 未触发完成中断，强制中止以解挂 */
        if (s->tx_busy && (uint32_t)(now - s->tx_started) > 100)
        {
            if (HAL_UART_AbortTransmit(handle(i)) == HAL_OK)
                s->tx_busy = false;
            s->drops++;
            s->tx_started = now;
        }
        /* 2. 接收错误自愈: 中断因溢出或帧错误挂起时，中止接收、清除溢出标志并重新使能中断 */
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

/**
 * @brief HAL 库串口单字节接收完成中断回调函数
 * @param[in] h 触发中断的 UART 句柄
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *h)
{
    int i = index_of(h);
    if (i < 0)
        return;
    /* 将接收到的字节压入环形缓冲区 */
    (void)srv_ring_buffer_push(&ports[i].rx, ports[i].rx_byte);
    /* 立即重新启动下一次单字节中断接收 */
    if (HAL_UART_Receive_IT(h, &ports[i].rx_byte, 1) != HAL_OK)
    {
        ports[i].recover = true;
        ports[i].errors++;
    }
}

/**
 * @brief HAL 库串口发送完成中断回调函数
 * @param[in] h 触发中断的 UART 句柄
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *h)
{
    int i = index_of(h);
    if (i >= 0)
        ports[i].tx_busy = false; /* 清除忙标志，允许下一帧发送 */
}

/**
 * @brief HAL 库串口错误中断回调函数
 * @param[in] h 发生错误的 UART 句柄
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *h)
{
    int i = index_of(h);
    if (i >= 0)
    {
        ports[i].errors++;
        ports[i].recover = true; /* 标记待恢复，在 service 中重启接收 */
    }
}

/**
 * @brief 获取指定串口的接收错误与环形缓冲区溢出总数
 * @param[in] p 端口号
 * @return 错误总数
 */
uint32_t bsp_usart_rx_errors(usart_port_t p)
{
    return (unsigned)p < 2 ? ports[p].errors + srv_ring_buffer_get_overflow_count(&ports[p].rx) : 0;
}

/**
 * @brief 获取因发送缓冲区忙导致的丢包总数
 * @param[in] p 端口号
 * @return 丢包总数
 */
uint32_t bsp_usart_tx_drops(usart_port_t p)
{
    return (unsigned)p < 2 ? ports[p].drops : 0;
}

/**
 * @brief 清空指定端口的接收环形缓冲区
 * @param[in] p 端口号
 */
void bsp_usart_flush_rx(usart_port_t p)
{
    if ((unsigned)p < 2)
        srv_ring_buffer_clear(&ports[p].rx);
}

/**
 * @brief 格式化输出到指定串口 (静态缓冲区 256 字节，保证线程安全)
 * @param[in] p   端口号
 * @param[in] fmt 格式化字符串
 */
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

/**
 * @brief 转发数据包到 PC 上位机串口
 * @param[in] p 数据指针
 * @param[in] n 长度
 */
void bsp_usart_forward_packet(const uint8_t *p, uint16_t n)
{
    (void)bsp_usart_transmit(USART_PORT_PC_FORWARD, p, n);
}
