#ifndef APP_MENU_H
#define APP_MENU_H
#include <stdint.h>
#include <stdbool.h>
typedef enum
{
    MENU_PAGE_JOYSTICK,
    MENU_PAGE_KEY_SW,
    MENU_PAGE_ATTITUDE,
    MENU_PAGE_COMM_STAT,
    MENU_PAGE_COUNT
} menu_page_t;
void app_menu_init(void);
void app_menu_navigate_up(void);
void app_menu_navigate_down(void);
void app_menu_action_enter(void);
void app_menu_action_back(void);
void app_menu_action_long(void);
void app_menu_render(void);
menu_page_t app_menu_get_current_page(void);
bool app_menu_in_detail(void);
#endif
