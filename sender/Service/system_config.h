#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H
/* Software defaults, not measurements of the transparent radio. */
#define TELEMETRY_PERIOD_MS 20U
#define LINK_ACK_TIMEOUT_MS 80U
#define LINK_MAX_RETRIES 2U
#define LINK_OFFLINE_MS 1000U
/* Boot silence lets a still-running peer expire its previous seq baseline.
 * Validate this guard against the physical radio buffering/latency. */
#define LINK_STARTUP_QUIET_MS 1500U
#define PARSER_GAP_MS 100U
#define KEY_DEBOUNCE_MS 20U
#define KEY_LONG_MS 2000U
#define KEY_DOUBLE_MS 300U
#define IMU_CAL_SAMPLES 100U
#define IMU_GYRO_LSB_PER_DPS 16.4f
#define IMU_ACCEL_LSB_PER_G 4096.0f
#define IMU_CORRECTION_KP 2.0f
#define JOYSTICK_VREF_MV 3300U
#define JOYSTICK_DEAD_MV 80U
/* Signed, one-based sensor axes: +1=X, +2=Y, +3=Z.
 * Use the same right-handed orthonormal mapping for accel and gyro. */
#define IMU_BODY_X 1
#define IMU_BODY_Y 2
#define IMU_BODY_Z 3
#endif
