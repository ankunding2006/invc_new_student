#ifndef __APP_SENDER_H
#define __APP_SENDER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 手柄系统工作模式枚举
 */
typedef enum {
    APP_STATE_INIT = 0,         /* 上电初始化与硬件自检 */
    APP_STATE_CALIBRATING,      /* 传感器零偏静止校准中 */
    APP_STATE_NORMAL,           /* 正常运行遥控模式 */
    APP_STATE_FAULT_DEGRADED    /* 外设掉线或总线异常降级运行 */
} app_state_t;

void        app_sender_init(void);
void        app_sender_task(void);
app_state_t app_sender_get_state(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_SENDER_H */
