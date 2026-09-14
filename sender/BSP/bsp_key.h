#ifndef __BSP_KEY_H
#define __BSP_KEY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    KEY_ID_1 = 0,
    KEY_ID_2,
    KEY_ID_3,
    KEY_ID_4,
    KEY_ID_COUNT
} key_id_t;

typedef enum {
    KEY_EVENT_NONE = 0,
    KEY_EVENT_PRESS,            /* 短按按下瞬间触发 */
    KEY_EVENT_RELEASE,          /* 按键抬起释放瞬间触发 */
    KEY_EVENT_LONG_PRESS,       /* 持续按下 >= 2.0s 触发 */
    KEY_EVENT_DOUBLE_CLICK      /* 300ms 窗口内连续双击触发 */
} key_event_t;

typedef struct {
    key_id_t    id;
    key_event_t event;
} key_msg_t;

void    bsp_key_init(void);
void    bsp_key_tick_10ms(void);
bool    bsp_key_get_event(key_msg_t *p_msg);
uint8_t bsp_key_get_mask(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_KEY_H */
