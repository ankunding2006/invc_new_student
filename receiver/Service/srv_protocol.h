#ifndef SRV_PROTOCOL_H
#define SRV_PROTOCOL_H
#include <stdint.h>
#include <stdbool.h>
#define PROTOCOL_FRAME_HEADER_1 0xAAU
#define PROTOCOL_FRAME_HEADER_2 0x55U
#define PROTOCOL_FRAME_TAIL 0x0DU
#define PROTOCOL_CMD_TELEMETRY 0x01U
#define PROTOCOL_CMD_ACK 0x80U
#define PROTOCOL_PAYLOAD_LEN 16U
#define PROTOCOL_FRAME_TOTAL_LEN 23U
#define PROTOCOL_ACK_LEN 9U
/* key_mask low nibble = keys, high nibble = source status. */
#define TELEMETRY_IMU_FAULT 0x10U
#define TELEMETRY_CALIBRATING 0x20U
#define TELEMETRY_ADC_FAULT 0x40U
#define TELEMETRY_OLED_FAULT 0x80U
#pragma pack(push, 1)
typedef struct
{
    int16_t joy_x_raw, joy_y_raw;
    uint16_t joy_x_mv, joy_y_mv;
    uint8_t key_mask, switch_mask;
    int16_t pitch_cd, roll_cd, yaw_cd; /* tenths of a degree */
} telemetry_payload_t;
#pragma pack(pop)
typedef char telemetry_size_check[(sizeof(telemetry_payload_t) == 16) ? 1 : -1];
typedef struct
{
    uint8_t bytes[23], used;
    uint32_t errors, last_byte_ms;
} protocol_parser_t;
typedef struct
{
    uint8_t seq, cmd, len, payload[16];
} protocol_packet_t;
void srv_protocol_init(void);
uint8_t srv_protocol_calc_checksum(const uint8_t *data, uint16_t len);
uint16_t srv_protocol_pack(uint8_t seq, const telemetry_payload_t *p, uint8_t *out, uint16_t cap);
bool srv_protocol_unpack(const protocol_packet_t *packet, telemetry_payload_t *out);
uint16_t srv_protocol_token(const uint8_t *data, uint16_t len);
uint16_t srv_protocol_pack_ack(uint8_t seq, uint16_t token, uint8_t *out, uint16_t cap);
void protocol_parser_init(protocol_parser_t *p);
/* Returns one complete validated frame. Safe for arbitrary noisy byte streams.
 * now is arrival/consumption time in ms. Call in main, never in ISR. */
bool protocol_parser_feed(protocol_parser_t *p, uint8_t byte, uint32_t now, protocol_packet_t *out);
#endif
