#include "app_menu.h"
#include "bsp_oled.h"

static menu_page_t s_current_page = MENU_PAGE_JOYSTICK;

void app_menu_init(void)
{
    s_current_page = MENU_PAGE_JOYSTICK;
}

void app_menu_navigate_up(void)
{
    if (s_current_page == 0) {
        s_current_page = MENU_PAGE_COUNT - 1;
    } else {
        s_current_page--;
    }
}

void app_menu_navigate_down(void)
{
    s_current_page = (menu_page_t)((s_current_page + 1) % MENU_PAGE_COUNT);
}

void app_menu_action_enter(void)
{
    /* 确认 / 动作触发 */
}

void app_menu_action_back(void)
{
    s_current_page = MENU_PAGE_JOYSTICK;
}

void app_menu_render(void)
{
    /* 页面渲染骨架 */
}

menu_page_t app_menu_get_current_page(void)
{
    return s_current_page;
}
