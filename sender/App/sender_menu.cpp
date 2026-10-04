#include "sender_menu.hpp"
extern "C"
{
#include "app_menu.h"
#include "app_sender.h"
#include "bsp_oled.h"
#include "bsp_joystick.h"
#include "srv_imu_filter.h"
}
#include <stdio.h>
#include <stdarg.h>
#include <type_traits>

namespace invc::sender
{
namespace
{
// 零初始化的静态存储，保持原有 C 启动状态。
static_assert(std::is_trivially_default_constructible_v<SenderMenu>);
static_assert(std::is_trivially_destructible_v<SenderMenu>);
SenderMenu instance{};
} // 匿名命名空间
SenderMenu &sender_menu() noexcept
{
    return instance;
}

static const char *titles[4] = {"Joystick", "Keys / Switches", "Attitude", "Communication"};
static void row(unsigned y, const char *fmt, ...)
{
    char text[24];
    va_list a;
    va_start(a, fmt);
    (void)vsnprintf(text, sizeof text, fmt, a);
    va_end(a);
    bsp_oled_show_string(0, (uint8_t)y, text, 12, 1);
}
void SenderMenu::init(void)
{
    selected = MENU_PAGE_JOYSTICK;
    detail = false;
}
void SenderMenu::navigate_up(void)
{
    selected = (menu_page_t)((selected + MENU_PAGE_COUNT - 1) % MENU_PAGE_COUNT);
}
void SenderMenu::navigate_down(void)
{
    selected = (menu_page_t)((selected + 1) % MENU_PAGE_COUNT);
}
void SenderMenu::enter(void)
{
    if (!detail)
    {
        detail = true;
        return;
    }
    switch (selected)
    {
    case MENU_PAGE_JOYSTICK:
        bsp_joystick_calibrate_zero();
        break;
    case MENU_PAGE_KEY_SW:
        app_sender_reset_events();
        break;
    case MENU_PAGE_COMM_STAT:
        app_sender_reset_tx_count();
        break;
    default:
        break;
    }
}
void SenderMenu::long_press(void)
{
    if (detail && selected == MENU_PAGE_ATTITUDE)
        app_sender_recalibrate();
}
void SenderMenu::back(void)
{
    detail = false;
}
menu_page_t SenderMenu::current_page(void)
{
    return selected;
}
bool SenderMenu::in_detail(void)
{
    return detail;
}
void SenderMenu::render(void)
{
    bsp_oled_clear();
    const sender_view_t *v = app_sender_view();
    if (!detail)
    {
        row(0, "INVC / %s",
            v->state == APP_STATE_NORMAL        ? "READY"
            : v->state == APP_STATE_CALIBRATING ? "CAL"
                                                : "CHECK");
        for (unsigned i = 0; i < 4; i++)
            row(12 + i * 12, "%c %s", i == (unsigned)selected ? '>' : ' ', titles[i]);
        return;
    }
    row(0, "< %s", titles[selected]);
    switch (selected)
    {
    case MENU_PAGE_JOYSTICK:
        row(12, "X:%5d Y:%5d", v->telemetry.joy_x_raw, v->telemetry.joy_y_raw);
        row(24, "X:%4umV Y:%4umV", v->telemetry.joy_x_mv, v->telemetry.joy_y_mv);
        row(36, "ADC %s", (v->telemetry.key_mask & TELEMETRY_ADC_FAULT) ? "FAULT" : "OK");
        row(48, "K3:Center K4:Back");
        break;
    case MENU_PAGE_KEY_SW:
    {
        static const char *names[] = {"NONE", "PRESS", "RELEASE", "LONG", "DOUBLE"};
        row(12, "KEY:%X SW:%X", v->telemetry.key_mask & 15, v->telemetry.switch_mask & 3);
        row(24, "K%u %s", (unsigned)v->last_key.id + 1,
            names[v->last_key.event <= KEY_EVENT_DOUBLE_CLICK ? v->last_key.event : 0]);
        row(36, "Events:%lu", (unsigned long)v->events);
        row(48, "K3:Clear K4:Back");
        break;
    }
    case MENU_PAGE_ATTITUDE:
        row(12, "P:%5d R:%5d", v->telemetry.pitch_cd, v->telemetry.roll_cd);
        row(24, "Y:%5d (0.1 deg)", v->telemetry.yaw_cd);
        if (v->state == APP_STATE_CALIBRATING)
            row(36, "Still! CAL %u/100", srv_imu_filter_calibration_count());
        else
            row(36, "IMU %s / Y relative",
                (v->telemetry.key_mask & TELEMETRY_IMU_FAULT) ? "ERR" : "OK");
        row(48, "Hold K3:Recalibrate");
        break;
    case MENU_PAGE_COMM_STAT:
        row(12, "ACK link:%s", v->link_online ? "ONLINE" : "WAIT");
        row(24, "TX:%lu OK:%lu", (unsigned long)v->tx, (unsigned long)v->acked);
        row(36, "Retry:%lu Drop:%lu", (unsigned long)v->retries, (unsigned long)v->expired);
        row(48, "K3:Clear K4:Back");
        break;
    default:
        detail = false;
        selected = MENU_PAGE_JOYSTICK;
        break;
    }
}

} // invc::sender 命名空间

extern "C" void app_menu_init(void)
{
    invc::sender::sender_menu().init();
}

extern "C" void app_menu_navigate_up(void)
{
    invc::sender::sender_menu().navigate_up();
}

extern "C" void app_menu_navigate_down(void)
{
    invc::sender::sender_menu().navigate_down();
}

extern "C" void app_menu_action_enter(void)
{
    invc::sender::sender_menu().enter();
}

extern "C" void app_menu_action_back(void)
{
    invc::sender::sender_menu().back();
}

extern "C" void app_menu_action_long(void)
{
    invc::sender::sender_menu().long_press();
}

extern "C" void app_menu_render(void)
{
    invc::sender::sender_menu().render();
}

extern "C" menu_page_t app_menu_get_current_page(void)
{
    return invc::sender::sender_menu().current_page();
}

extern "C" bool app_menu_in_detail(void)
{
    return invc::sender::sender_menu().in_detail();
}
