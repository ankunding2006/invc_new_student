#ifndef __BSP_SWITCH_H
#define __BSP_SWITCH_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdbool.h>

    typedef enum
    {
        SWITCH_ID_1 = 0,
        SWITCH_ID_2,
        SWITCH_ID_COUNT
    } switch_id_t;

    void bsp_switch_init(void);
    bool bsp_switch_get_state(switch_id_t sw_id);
    uint8_t bsp_switch_get_mask(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_SWITCH_H */
