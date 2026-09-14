#ifndef __BSP_IMU_H
#define __BSP_IMU_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "srv_imu_filter.h"

typedef enum {
    IMU_STATUS_OK = 0,
    IMU_STATUS_DEV_NOT_FOUND,
    IMU_STATUS_TIMEOUT,
    IMU_STATUS_NOT_INITIALIZED
} imu_status_t;

bool bsp_imu_init(void);
bool bsp_imu_check_id(void);
bool bsp_imu_read_raw(imu_raw_t *p_raw);
imu_status_t bsp_imu_get_status(void);
bool bsp_imu_recover(void); /* 后台1Hz尝试重新检测WHO_AM_I与自愈重连 */

#ifdef __cplusplus
}
#endif

#endif /* __BSP_IMU_H */
