#ifndef APP_SENDER_H
#define APP_SENDER_H
#include "srv_protocol.h"
#include "imu_types.h"
#include "srv_input.h"
typedef enum
{
    APP_STATE_INIT,
    APP_STATE_CALIBRATING,
    APP_STATE_NORMAL,
    APP_STATE_FAULT_DEGRADED
} app_state_t;
typedef struct
{
    telemetry_payload_t telemetry;
    imu_raw_t raw;
    imu_euler_t euler, raw_angles;
    app_state_t state;
    uint32_t tx, acked, retries, expired, events;
    key_msg_t last_key;
    bool link_online;
} sender_view_t;
void app_sender_init(void);
void app_sender_task(void);
app_state_t app_sender_get_state(void);
const sender_view_t *app_sender_view(void);
void app_sender_recalibrate(void);
void app_sender_reset_tx_count(void);
void app_sender_reset_events(void);
#endif
