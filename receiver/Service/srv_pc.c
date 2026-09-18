#include "srv_pc.h"
#include <string.h>
#include <float.h>
typedef char
    require_ieee_float[(sizeof(float) == 4 && FLT_RADIX == 2 && FLT_MANT_DIG == 24) ? 1 : -1];
uint16_t srv_pc_pack(const telemetry_payload_t *p, float freq, float loss, float wire, bool online,
                     uint8_t *out, uint16_t cap)
{
    if (!p || !out || cap < PC_FLOAT_FRAME_LEN)
        return 0;
    float values[14] = {(float)p->joy_x_raw,
                        (float)p->joy_y_raw,
                        p->joy_x_mv * 0.001f,
                        p->joy_y_mv * 0.001f,
                        (float)(p->key_mask & 15),
                        (float)(p->switch_mask & 3),
                        p->pitch_cd * 0.1f,
                        p->roll_cd * 0.1f,
                        p->yaw_cd * 0.1f,
                        freq,
                        loss,
                        wire,
                        (float)(p->key_mask >> 4),
                        online ? 1.0f : 0.0f};
    for (unsigned i = 0; i < 14; i++)
    {
        uint32_t bits;
        memcpy(&bits, &values[i], 4);
        for (unsigned b = 0; b < 4; b++)
            out[4 * i + b] = (uint8_t)(bits >> (8 * b));
    }
    out[56] = 0;
    out[57] = 0;
    out[58] = 0x80;
    out[59] = 0x7F;
    return 60;
}
