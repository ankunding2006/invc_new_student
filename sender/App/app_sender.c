#include "app_sender.h"
#include "app_menu.h"
#include "srv_protocol.h"
#include "srv_imu_filter.h"
#include "bsp_joystick.h"
#include "bsp_key.h"
#include "bsp_switch.h"
#include "bsp_imu.h"
#include "bsp_oled.h"
#include "bsp_usart.h"

static app_state_t s_app_state = APP_STATE_INIT;

void app_sender_init(void)
{
    s_app_state = APP_STATE_INIT;
    /* 外设底层驱动初始化调度 */
    bsp_joystick_init();
    bsp_key_init();
    bsp_switch_init();
    bsp_imu_init();
    bsp_oled_init();
    bsp_usart_init();

    /* 业务服务层初始化 */
    srv_protocol_init();
    srv_imu_filter_init(50.0f);

    /* UI 与菜单初始化 */
    app_menu_init();

    s_app_state = APP_STATE_NORMAL;
}

void app_sender_task(void)
{
    /* 手柄端周期任务调度流水线骨架 */
}

app_state_t app_sender_get_state(void)
{
    return s_app_state;
}
