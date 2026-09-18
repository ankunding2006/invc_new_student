#ifndef BSP_IMU_H
#define BSP_IMU_H
#include <stdbool.h>
#include <stdint.h>
#include "imu_types.h"
typedef enum
{
    IMU_STATUS_OK,
    IMU_STATUS_DEV_NOT_FOUND,
    IMU_STATUS_TIMEOUT,
    IMU_STATUS_NOT_INITIALIZED,
    IMU_STATUS_STARTING
} imu_status_t;
/* init starts an asynchronous reset. service must run until OK. */
bool bsp_imu_init(void);
void bsp_imu_service(uint32_t now);
bool bsp_imu_check_id(void);
bool bsp_imu_read_raw(imu_raw_t *out);
imu_status_t bsp_imu_get_status(void);
bool bsp_imu_recover(void);
#endif
