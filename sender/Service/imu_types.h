#ifndef IMU_TYPES_H
#define IMU_TYPES_H
#include <stdint.h>
typedef struct
{
    int16_t ax, ay, az, gx, gy, gz;
} imu_raw_t;
typedef struct
{
    float pitch, roll, yaw;
} imu_euler_t;
#endif
