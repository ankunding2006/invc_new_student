#ifndef SRV_PC_H
#define SRV_PC_H
#include "srv_protocol.h"
/* 14 IEEE754 float32 little-endian channels + 00 00 80 7F terminator. */
#define PC_FLOAT_FRAME_LEN 60U
uint16_t srv_pc_pack(const telemetry_payload_t *p, float freq, float loss, float wire, bool online,
                     uint8_t *out, uint16_t cap);
#endif
