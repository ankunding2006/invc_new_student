#ifndef __APP_MENU_H
#define __APP_MENU_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief OLED 层次化多级菜单页面枚举
 */
typedef enum {
    MENU_PAGE_JOYSTICK = 0,     /* Page 1: 摇杆模拟量与电压显示页 */
    MENU_PAGE_KEY_SW,           /* Page 2: 按键与拨码开关事件页 */
    MENU_PAGE_ATTITUDE,         /* Page 3: IMU 姿态角监控与现场校准页 */
    MENU_PAGE_COMM_STAT,        /* Page 4: 发包计数与通信统计页 */
    MENU_PAGE_COUNT
} menu_page_t;

void        app_menu_init(void);
void        app_menu_navigate_up(void);
void        app_menu_navigate_down(void);
void        app_menu_action_enter(void);
void        app_menu_action_back(void);
void        app_menu_render(void);
menu_page_t app_menu_get_current_page(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_MENU_H */
