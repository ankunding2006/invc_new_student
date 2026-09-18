#ifndef APP_RECEIVER_H
#define APP_RECEIVER_H
#include "srv_protocol.h"
#include "srv_stats.h"
typedef enum
{
    RECEIVER_STATE_WAIT_SYNC,
    RECEIVER_STATE_CONNECTED,
    RECEIVER_STATE_OFFLINE
} receiver_state_t;
void app_receiver_init(void);
void app_receiver_task(void);
receiver_state_t app_receiver_get_state(void);
const telemetry_payload_t *app_receiver_telemetry(void);
uint32_t app_receiver_pc_drops(void);
#endif
