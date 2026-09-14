#ifndef __APP_RECEIVER_H
#define __APP_RECEIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 接收端运行状态枚举
 */
typedef enum {
    RECEIVER_STATE_WAIT_SYNC = 0,   /* 等待接收首次有效数据包 */
    RECEIVER_STATE_CONNECTED,       /* 正常联机接收状态 */
    RECEIVER_STATE_OFFLINE          /* 通信超时断开状态 (>1000ms 未收包) */
} receiver_state_t;

void             app_receiver_init(void);
void             app_receiver_task(void);
receiver_state_t app_receiver_get_state(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_RECEIVER_H */
