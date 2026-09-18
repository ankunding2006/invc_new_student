#include "bsp_imu.h"
#include "system_config.h"
#include "spi.h"
#include "main.h"
#include <string.h>
/* RM-MPU-6500A-00 sections 4.4-4.8,4.15-4.17,4.32-4.34,4.37.
 * Raw-register path only: no DMP firmware or vendor DMP functions compiled.
 * All accesses use 72MHz/128 = 562.5kHz (<1MHz register-access limit). */
#define AXIS_ABS(x) ((x) < 0 ? -(x) : (x))
typedef char check_axis_bounds[(AXIS_ABS(IMU_BODY_X) >= 1 && AXIS_ABS(IMU_BODY_X) <= 3 &&
                                AXIS_ABS(IMU_BODY_Y) >= 1 && AXIS_ABS(IMU_BODY_Y) <= 3 &&
                                AXIS_ABS(IMU_BODY_Z) >= 1 && AXIS_ABS(IMU_BODY_Z) <= 3)
                                   ? 1
                                   : -1];
typedef char check_axis_unique[(AXIS_ABS(IMU_BODY_X) != AXIS_ABS(IMU_BODY_Y) &&
                                AXIS_ABS(IMU_BODY_X) != AXIS_ABS(IMU_BODY_Z) &&
                                AXIS_ABS(IMU_BODY_Y) != AXIS_ABS(IMU_BODY_Z))
                                   ? 1
                                   : -1];
/* A proper rotation has determinant +1; reject mirrored coordinate systems. */
#define AXIS_PARITY                                                                                \
    ((AXIS_ABS(IMU_BODY_X) - AXIS_ABS(IMU_BODY_Y)) *                                               \
     (AXIS_ABS(IMU_BODY_Y) - AXIS_ABS(IMU_BODY_Z)) *                                               \
     (AXIS_ABS(IMU_BODY_Z) - AXIS_ABS(IMU_BODY_X)))
typedef char check_right_handed[(AXIS_PARITY * IMU_BODY_X * IMU_BODY_Y * IMU_BODY_Z > 0) ? 1 : -1];
static imu_status_t status = IMU_STATUS_NOT_INITIALIZED;
static uint8_t phase, config_index;
static uint32_t since, retry_at, health_at, last_sample;
static const uint8_t config[][2] = {{0x6B, 0x01}, {0x6C, 0x00}, {0x6A, 0x10}, {0x1A, 0x04},
                                    {0x1B, 0x18}, {0x1C, 0x10}, {0x1D, 0x04}, {0x19, 19},
                                    {0x23, 0},    {0x38, 0}};
static bool transfer(uint8_t reg, uint8_t *data, uint8_t len, bool read)
{
    uint8_t tx[15] = {0}, rx[15] = {0};
    if (len > 14 || !data)
        return false;
    tx[0] = read ? (reg | 0x80U) : reg;
    if (!read)
        memcpy(tx + 1, data, len);
    HAL_GPIO_WritePin(MPU_CS_GPIO_Port, MPU_CS_Pin, GPIO_PIN_RESET);
    HAL_StatusTypeDef rc = HAL_SPI_TransmitReceive(&hspi1, tx, rx, (uint16_t)(len + 1), 2);
    HAL_GPIO_WritePin(MPU_CS_GPIO_Port, MPU_CS_Pin, GPIO_PIN_SET);
    if (rc != HAL_OK)
    {
        status = IMU_STATUS_TIMEOUT;
        phase = 0;
        retry_at = HAL_GetTick();
        return false;
    }
    if (read)
        memcpy(data, rx + 1, len);
    return true;
}
bool bsp_imu_check_id(void)
{
    uint8_t id = 0;
    if (!transfer(0x75, &id, 1, true))
        return false;
    if (id != 0x70)
    {
        status = IMU_STATUS_DEV_NOT_FOUND;
        phase = 0;
        retry_at = HAL_GetTick();
        return false;
    }
    return true;
}
bool bsp_imu_init(void)
{
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_128;
    if (HAL_SPI_Init(&hspi1) != HAL_OK)
    {
        status = IMU_STATUS_TIMEOUT;
        retry_at = HAL_GetTick();
        return false;
    }
    status = IMU_STATUS_STARTING;
    phase = 1;
    config_index = 0;
    since = HAL_GetTick();
    retry_at = since;
    return true;
}
bool bsp_imu_recover(void)
{
    return bsp_imu_init();
}
imu_status_t bsp_imu_get_status(void)
{
    return status;
}
void bsp_imu_service(uint32_t now)
{
    if (status == IMU_STATUS_OK)
    {
        if ((uint32_t)(now - health_at) >= 1000)
        {
            health_at = now;
            (void)bsp_imu_check_id();
        }
        return;
    }
    if (!phase)
    {
        if ((uint32_t)(now - retry_at) >= 1000)
            (void)bsp_imu_init();
        return;
    }
    if (phase == 1)
    {
        if ((uint32_t)(now - since) < 100)
            return;
        if (!bsp_imu_check_id())
            return;
        uint8_t reset = 0x80;
        if (transfer(0x6B, &reset, 1, false))
        {
            phase = 2;
            since = now;
        }
        return;
    }
    if (phase == 2)
    {
        if ((uint32_t)(now - since) < 100)
            return;
        phase = 3;
    }
    if (phase == 3)
    {
        if (config_index < sizeof(config) / sizeof(config[0]))
        {
            uint8_t value = config[config_index][1];
            if (!transfer(config[config_index][0], &value, 1, false))
                return;
            uint8_t check = 0;
            if (!transfer(config[config_index][0], &check, 1, true))
                return;
            if (check != value)
            {
                status = IMU_STATUS_DEV_NOT_FOUND;
                phase = 0;
                retry_at = now;
                return;
            }
            config_index++;
            return;
        }
        phase = 4;
        since = now;
        return;
    }
    if (phase == 4 && (uint32_t)(now - since) >= 100)
    {
        status = IMU_STATUS_OK;
        phase = 0;
        health_at = last_sample = now;
    }
}
static int16_t be16(const uint8_t *p)
{
    uint16_t v = (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
    return (int16_t)((v & 0x8000U) ? (int32_t)v - 65536 : (int32_t)v);
}
static int16_t map_axis(const int16_t v[3], int axis)
{
    int32_t n = v[(axis < 0 ? -axis : axis) - 1];
    if (axis < 0)
        n = -n;
    return (int16_t)(n > 32767 ? 32767 : n);
}
bool bsp_imu_read_raw(imu_raw_t *out)
{
    if (!out)
        return false;
    memset(out, 0, sizeof(*out));
    if (status != IMU_STATUS_OK)
        return false;
    uint8_t ready = 0, data[14];
    if (!transfer(0x3A, &ready, 1, true))
        return false;
    if (!(ready & 1))
    {
        if ((uint32_t)(HAL_GetTick() - last_sample) > 200)
        {
            status = IMU_STATUS_TIMEOUT;
            phase = 0;
            retry_at = HAL_GetTick();
        }
        return false;
    }
    if (!transfer(0x3B, data, 14, true))
        return false;
    int16_t a[3] = {be16(data), be16(data + 2), be16(data + 4)},
            g[3] = {be16(data + 8), be16(data + 10), be16(data + 12)};
    out->ax = map_axis(a, IMU_BODY_X);
    out->ay = map_axis(a, IMU_BODY_Y);
    out->az = map_axis(a, IMU_BODY_Z);
    out->gx = map_axis(g, IMU_BODY_X);
    out->gy = map_axis(g, IMU_BODY_Y);
    out->gz = map_axis(g, IMU_BODY_Z);
    last_sample = HAL_GetTick();
    return true;
}
