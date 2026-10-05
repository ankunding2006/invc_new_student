#ifndef APP_SENDER_H
#define APP_SENDER_H

/**
 * @file app_sender.h
 * @brief 发送端应用层主状态机与 C 兼容接口定义
 */

#ifdef __cplusplus
extern "C"
{
#endif

#include "srv_protocol.h"
#include "imu_types.h"
#include "srv_input.h"

    /**
     * @brief 发送端系统工作状态枚举
     */
    typedef enum
    {
        APP_STATE_INIT,           /**< 开机初始化与传感器等待状态 */
        APP_STATE_CALIBRATING,    /**< 正在进行陀螺仪静止零偏标定 */
        APP_STATE_NORMAL,         /**< 正常运行状态 (传感器/外设全部就绪) */
        APP_STATE_FAULT_DEGRADED  /**< 降级运行状态 (IMU、摇杆或显示屏出现异常) */
    } app_state_t;

    /**
     * @brief 发送端应用视图聚合快照结构体 (只读供 UI 菜单渲染或测试快照比对)
     */
    typedef struct
    {
        telemetry_payload_t telemetry;                  /* 当前准备发往接收端的遥测帧载荷数据 */
        imu_raw_t raw;                                  /* 最新采样的六轴 IMU 原始物理读数 */
        imu_euler_t euler, raw_angles;                 /* euler: 融合滤波后的姿态角; raw_angles: 仅加速度计解算的几何姿态角 */
        app_state_t state;                              /* 当前系统主状态 */
        uint32_t tx, acked, retries, expired, events;   /* 通信与事件统计: 发送数、ACK确认数、重传数、超时丢包数、累计按键事件数 */
        key_msg_t last_key;                             /* 最近一次触发的按键消息 */
        bool link_online;                               /* 与接收端的无线链路是否在线 (在超时时间内有 ACK) */
    } sender_view_t;

    /**
     * @brief 发送端应用初始化入口 (C ABI 导出函数，供 main.c 调用)
     */
    void app_sender_init(void);

    /**
     * @brief 发送端主循环任务 (C ABI 导出函数，在 main.c while(1) 中调用)
     */
    void app_sender_task(void);

    /**
     * @brief 获取当前发送端系统运行状态
     * @return 系统状态枚举
     */
    app_state_t app_sender_get_state(void);

    /**
     * @brief 获取发送端最新运行时数据快照指针
     * @return 只读结构体指针
     */
    const sender_view_t *app_sender_view(void);

    /**
     * @brief 触发重新标定陀螺仪零偏
     */
    void app_sender_recalibrate(void);

    /**
     * @brief 重置当前显示的发送/重传/丢包统计基准计数
     */
    void app_sender_reset_tx_count(void);

    /**
     * @brief 重置按键事件计数器
     */
    void app_sender_reset_events(void);

#ifdef __cplusplus
}
#endif
#endif
