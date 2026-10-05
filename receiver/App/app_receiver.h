#ifndef APP_RECEIVER_H
#define APP_RECEIVER_H

/**
 * @file app_receiver.h
 * @brief 接收端应用层状态机与 C 导出接口定义
 */

#ifdef __cplusplus
extern "C"
{
#endif

#include "srv_protocol.h"
#include "srv_stats.h"

    /**
     * @brief 接收端连接状态枚举
     */
    typedef enum
    {
        RECEIVER_STATE_WAIT_SYNC, /**< 等待初次建立通信连接并同步序号基准 */
        RECEIVER_STATE_CONNECTED, /**< 链路连接正常，遥测数据按时到达 */
        RECEIVER_STATE_OFFLINE    /**< 链路超时断开 (超过 1000ms 未收到有效帧) */
    } receiver_state_t;

    /**
     * @brief 接收端应用初始化入口 (供 main.c 调用)
     */
    void app_receiver_init(void);

    /**
     * @brief 接收端主循环调度任务 (供 main.c while(1) 调用)
     */
    void app_receiver_task(void);

    /**
     * @brief 获取当前接收端连接状态
     * @return 状态枚举
     */
    receiver_state_t app_receiver_get_state(void);

    /**
     * @brief 获取当前最新的遥测数据只读快照
     * @return 指向 telemetry_payload_t 结构体的指针
     */
    const telemetry_payload_t *app_receiver_telemetry(void);

    /**
     * @brief 获取发往上位机时因串口拥塞导致的丢包总数
     * @return 丢包计数
     */
    uint32_t app_receiver_pc_drops(void);

#ifdef __cplusplus
}
#endif
#endif
