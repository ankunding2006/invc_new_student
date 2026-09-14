#ifndef __APP_UI_H
#define __APP_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "srv_protocol.h"

void app_ui_init(void);
void app_ui_update(const telemetry_payload_t *p_telemetry, float freq_hz, float loss_rate_pct);

#ifdef __cplusplus
}
#endif

#endif /* __APP_UI_H */
