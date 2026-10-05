#ifndef BSP_USART_H
#define BSP_USART_H

/**
 * @file bsp_usart.h
 * @brief 双串口硬件抽象驱动 (USART1 无线串口 + USART2 调试/PC上位机串口)
 * 
 * 驱动架构:
 * 1. 接收: 中断单字节驱动 + 256 字节环形缓冲区 (无阻塞);
 * 2. 发送: 中断非阻塞发送 + 发送状态机与 100ms 超时看门狗自动中止;
 * 3. 错误恢复: 自动清除 ORE (过载) 错误标志并自动恢复中断接收。
 */

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 串口逻辑端口定义
 */
typedef enum
{
    USART_PORT_WIRELESS = 0,    /**< 端口 0: USART1 无线透明串口 */
    USART_PORT_DEBUG = 1,       /**< 端口 1: USART2 调试打印串口 (发送端使用) */
    USART_PORT_PC_FORWARD = 1   /**< 端口 1: USART2 PC 上位机遥测转发串口 (接收端使用) */
} usart_port_t;

/** @brief 兼容类型别名 */
typedef usart_port_t rx_usart_port_t;

/**
 * @brief 初始化双串口中断接收与环形缓冲区
 */
void bsp_usart_init(void);

/**
 * @brief 串口周期后台维护任务 (看门狗超时恢复与接收错误自愈)
 * @param[in] now 当前时间戳 (毫秒)
 */
void bsp_usart_service(uint32_t now);

/**
 * @brief 查询指定端口是否空闲可发送
 * @param[in] port 逻辑端口号
 * @return true 空闲就绪; false 正在发送中
 */
bool bsp_usart_tx_ready(usart_port_t port);

/**
 * @brief 非阻塞启动串口中断发送
 * @param[in] port 逻辑端口
 * @param[in] data 待发送数据指针
 * @param[in] len  数据长度 (1 ~ 256 字节)
 * @return true 成功启动发送; false 正在忙、参数非法或长度越界
 */
bool bsp_usart_transmit(usart_port_t port, const uint8_t *data, uint16_t len);

/**
 * @brief 从接收环形缓冲区中提取已收到的数据
 * @param[in]  port 逻辑端口
 * @param[out] out  输出缓冲区指针
 * @param[in]  cap  输出缓冲区容量上限
 * @return 实际读取出的字节数
 */
uint16_t bsp_usart_receive(usart_port_t port, uint8_t *out, uint16_t cap);

/**
 * @brief 格式化打印输出到指定串口 (类似 printf)
 * @param[in] port 逻辑端口
 * @param[in] fmt  格式化字符串
 */
void bsp_usart_printf(usart_port_t port, const char *fmt, ...);

/**
 * @brief 转发数据包到 PC 上位机串口
 * @param[in] data 数据指针
 * @param[in] len  长度
 */
void bsp_usart_forward_packet(const uint8_t *data, uint16_t len);

/**
 * @brief 查询指定串口累计发生的接收错误总数 (包含硬件错误与环形缓冲区溢出)
 * @param[in] port 逻辑端口
 * @return 错误计数
 */
uint32_t bsp_usart_rx_errors(usart_port_t port);

/**
 * @brief 查询因发送缓冲区忙导致的发送丢包总数
 * @param[in] port 逻辑端口
 * @return 丢包计数
 */
uint32_t bsp_usart_tx_drops(usart_port_t port);

/**
 * @brief 清空指定端口的接收环形缓冲区
 * @param[in] port 逻辑端口
 */
void bsp_usart_flush_rx(usart_port_t port);

#endif
