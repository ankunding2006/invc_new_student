#include "bsp_switch.h"
#include "main.h"
static uint8_t stable, candidate;
static uint32_t changed[2];
static uint8_t pins(void)
{
    uint32_t value = GPIOB->IDR;
    return (uint8_t)(((value & SW1_Pin) ? 1U : 0U) | ((value & SW2_Pin) ? 2U : 0U));
}
void bsp_switch_init(void)
{
    stable = candidate = pins();
    changed[0] = changed[1] = HAL_GetTick();
}
uint8_t bsp_switch_get_mask(void)
{
    uint8_t current = pins();
    uint32_t now = HAL_GetTick();
    for (unsigned i = 0; i < 2; i++)
    {
        uint8_t bit = (uint8_t)(1U << i);
        if ((current ^ candidate) & bit)
        {
            candidate ^= bit;
            changed[i] = now;
        }
        else if ((uint32_t)(now - changed[i]) >= 60U)
            stable = (uint8_t)((stable & ~bit) | (candidate & bit));
    }
    return stable;
}
bool bsp_switch_get_state(switch_id_t id)
{
    return (unsigned)id < SWITCH_ID_COUNT && (bsp_switch_get_mask() & (1U << id)) != 0;
}
