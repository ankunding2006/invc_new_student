#ifndef __SRV_RING_BUFFER_H
#define __SRV_RING_BUFFER_H

/**
 * @file srv_ring_buffer.h
 * @brief 通用轻量无锁单生产者单消费者(SPSC)环形字节缓冲区
 * 
 * 适用于单核 STM32 单中断生产者(如 UART RX ISR)与主循环单消费者之间的字节流解耦。
 * 缓冲区容量要求 >= 2，最大可用存储容量为 size - 1（保留一个槽位用于区分满与空）。
 */

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdbool.h>

    /**
     * @brief 通用轻量环形字节缓冲区结构体
     */
    typedef struct
    {
        volatile uint8_t *p_buffer;      /**< 指向外部实际存储空间的字节指针 */
        uint16_t size;                    /**< 缓冲区总空间大小(字节数, 需 >= 2) */
        volatile uint16_t head;           /**< 写入游标 (由生产者 ISR 独占更新) */
        volatile uint16_t tail;           /**< 读取游标 (由主循环消费者独占更新) */
        volatile uint32_t overflow_count; /* 溢出丢弃计数器 (E-COM-02 防御性标记) */
    } ring_buffer_t;

    /**
     * @brief 初始化环形缓冲区
     * @param[out] rb      指向待初始化的环形缓冲区对象结构体指针
     * @param[in]  storage 外部传入的静态或全局字节数组存储区地址
     * @param[in]  size    存储区总字节数，必须 >= 2
     * @note 必须在开启相关中断前调用此函数完成初始化。
     */
    void srv_ring_buffer_init(ring_buffer_t *rb, uint8_t *storage, uint16_t size);

    /**
     * @brief 向环形缓冲区压入单字节 (入队)
     * @param[in,out] rb   环形缓冲区指针
     * @param[in]     byte 待写入的数据字节
     * @return true 写入成功; false 缓冲区已满或未初始化，数据被丢弃且溢出计数器自增
     * @note 通常在串口接收中断 (ISR) 中被单生产者调用。
     */
    bool srv_ring_buffer_push(ring_buffer_t *rb, uint8_t byte);

    /**
     * @brief 从环形缓冲区弹出单字节 (出队)
     * @param[in,out] rb     环形缓冲区指针
     * @param[out]    p_byte 指向接收读取字节的内存地址指针
     * @return true 读取成功并移除该字节; false 缓冲区为空或指针无效
     * @note 属于消费端操作，通常在主循环或高层服务任务中调用。
     */
    bool srv_ring_buffer_pop(ring_buffer_t *rb, uint8_t *p_byte);

    /**
     * @brief 窥探 (查看) 缓冲区中指定偏移处的字节，但不移出队列
     * @param[in]  rb     环形缓冲区只读指针
     * @param[in]  offset 相对当前 tail 的偏移索引量 (0 <= offset < count)
     * @param[out] p_byte 输出保存窥探到的字节地址
     * @return true 窥探成功; false 偏移越界或队列无效
     */
    bool srv_ring_buffer_peek(const ring_buffer_t *rb, uint16_t offset, uint8_t *p_byte);

    /**
     * @brief 获取当前缓冲区中已存有的数据字节数量
     * @param[in] rb 环形缓冲区只读指针
     * @return 当前可读取的字节数 (0 ~ size - 1)
     */
    uint16_t srv_ring_buffer_get_count(const ring_buffer_t *rb);

    /**
     * @brief 查询环形缓冲区是否为空
     * @param[in] rb 环形缓冲区只读指针
     * @return true 为空 (head == tail); false 非空
     */
    bool srv_ring_buffer_is_empty(const ring_buffer_t *rb);

    /**
     * @brief 查询环形缓冲区是否已满
     * @param[in] rb 环形缓冲区只读指针
     * @return true 已满 ((head + 1) % size == tail); false 未满
     */
    bool srv_ring_buffer_is_full(const ring_buffer_t *rb);

    /**
     * @brief 获取发生缓冲区溢出的丢包计数
     * @param[in] rb 环形缓冲区只读指针
     * @return 历史累计溢出丢弃的字节总数
     */
    uint32_t srv_ring_buffer_get_overflow_count(const ring_buffer_t *rb);

    /**
     * @brief 清空环形缓冲区中的数据
     * @param[in,out] rb 环形缓冲区指针
     * @note 仅将 tail 移动至当前快照的 head 位置，不重置溢出计数器，亦不回拨中断中的 head。
     */
    void srv_ring_buffer_clear(ring_buffer_t *rb);

#ifdef __cplusplus
}
#endif

#endif /* __SRV_RING_BUFFER_H */
