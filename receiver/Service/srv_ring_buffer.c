/**
 * @file srv_ring_buffer.c
 * @brief 通用轻量环形字节缓冲区实现
 * @note 采用单生产者单消费者无锁设计。head 仅由生产者更新，tail 仅由消费者更新。
 */

#include "srv_ring_buffer.h"
#include <stddef.h>

/**
 * @brief 内部校验环形缓冲区结构体的有效性
 * @param[in] rb 环形缓冲区指针
 * @return true 结构体非空、底层缓冲指针有效且容量 >= 2; false 无效
 */
static bool ring_buffer_valid(const ring_buffer_t *rb)
{
    return rb != NULL && rb->p_buffer != NULL && rb->size >= 2;
}

/**
 * @brief 初始化环形缓冲区结构
 * @param[out] rb      缓冲区结构体指针
 * @param[in]  storage 外部存储数组指针
 * @param[in]  size    存储数组大小 (字节数, 需 >= 2)
 */
void srv_ring_buffer_init(ring_buffer_t *rb, uint8_t *storage, uint16_t size)
{
    if (rb == NULL)
    {
        return;
    }
    /* 若入参无效，则置为空指针与大小0，保持安全的禁用查询状态 */
    rb->p_buffer = (storage != NULL && size >= 2) ? storage : NULL;
    rb->size = (rb->p_buffer != NULL) ? size : 0;
    rb->head = 0;
    rb->tail = 0;
    rb->overflow_count = 0;
}

/**
 * @brief 向环形缓冲区压入单字节 (生产者 ISR)
 * @param[in,out] rb   环形缓冲区指针
 * @param[in]     byte 写入字节
 * @return true 写入成功; false 溢出或缓冲区不可用
 */
bool srv_ring_buffer_push(ring_buffer_t *rb, uint8_t byte)
{
    if (!ring_buffer_valid(rb))
    {
        return false;
    }
    uint16_t head = rb->head;
    /* 计算下一个写入位置 (模 size 循环) */
    uint16_t next_head = (uint16_t)((head + 1U) % rb->size);
    /* 若下一个位置与读游标 tail 重合，则表示队列已满，无法再写入 */
    if (next_head == rb->tail)
    {
        rb->overflow_count++; /* 累加溢出丢弃计数 */
        return false;
    }
    /* 写入数据并更新 head 游标 (单向移动，原子可见) */
    rb->p_buffer[head] = byte;
    rb->head = next_head;
    return true;
}

/**
 * @brief 从环形缓冲区弹出单字节 (消费者主循环)
 * @param[in,out] rb     环形缓冲区指针
 * @param[out]    p_byte 读取目标变量指针
 * @return true 读出成功; false 队列为空或无效
 */
bool srv_ring_buffer_pop(ring_buffer_t *rb, uint8_t *p_byte)
{
    if (!ring_buffer_valid(rb) || p_byte == NULL)
    {
        return false;
    }
    uint16_t tail = rb->tail;
    /* 若读游标与写游标重合，说明队列中无待读取数据 */
    if (rb->head == tail)
    {
        return false;
    }
    /* 读取当前 tail 处字节，并向前步进 tail 游标 */
    *p_byte = rb->p_buffer[tail];
    rb->tail = (uint16_t)((tail + 1U) % rb->size);
    return true;
}

/**
 * @brief 随机窥探缓冲区中的数据 (不改变 tail 游标)
 * @param[in]  rb     环形缓冲区指针
 * @param[in]  offset 相对当前 tail 的字节偏移 (0 为队首元素)
 * @param[out] p_byte 输出保存窥探值的地址指针
 * @return true 成功窥探; false 越界或不可用
 */
bool srv_ring_buffer_peek(const ring_buffer_t *rb, uint16_t offset, uint8_t *p_byte)
{
    if (!ring_buffer_valid(rb) || p_byte == NULL || offset >= srv_ring_buffer_get_count(rb))
    {
        return false;
    }
    /* 计算环形偏移并取值 */
    *p_byte = rb->p_buffer[(rb->tail + offset) % rb->size];
    return true;
}

/**
 * @brief 获取当前队列中有效数据字节数
 * @param[in] rb 环形缓冲区指针
 * @return 有效字节数
 */
uint16_t srv_ring_buffer_get_count(const ring_buffer_t *rb)
{
    if (!ring_buffer_valid(rb))
    {
        return 0;
    }
    uint16_t tail = rb->tail;
    uint16_t head = rb->head;
    /* 若 head >= tail，差值即为元素个数；若 head < tail，说明已绕环跨过末尾 */
    return (head >= tail) ? (uint16_t)(head - tail) : (uint16_t)(rb->size - tail + head);
}

/**
 * @brief 判断队列是否为空
 * @param[in] rb 环形缓冲区指针
 * @return true 为空; false 非空
 */
bool srv_ring_buffer_is_empty(const ring_buffer_t *rb)
{
    return srv_ring_buffer_get_count(rb) == 0;
}

/**
 * @brief 判断队列是否已满
 * @param[in] rb 环形缓冲区指针
 * @return true 已满; false 未满
 */
bool srv_ring_buffer_is_full(const ring_buffer_t *rb)
{
    return ring_buffer_valid(rb) && ((rb->head + 1U) % rb->size == rb->tail);
}

/**
 * @brief 清空缓冲区内已有内容
 * @param[in,out] rb 环形缓冲区指针
 * @note 消费者仅将 tail 对齐到当前采样的 head 位置，不会倒退或干扰正在进行的中断写入
 */
void srv_ring_buffer_clear(ring_buffer_t *rb)
{
    if (ring_buffer_valid(rb))
    {
        /* Consumer discards the current snapshot; never rewinds producer head. */
        rb->tail = rb->head;
    }
}

/**
 * @brief 获取溢出丢弃总数
 * @param[in] rb 环形缓冲区指针
 * @return 溢出计数
 */
uint32_t srv_ring_buffer_get_overflow_count(const ring_buffer_t *rb)
{
    return (rb != NULL) ? rb->overflow_count : 0;
}
