/**
 * @file sender_menu.cpp
 * @brief 发送端 OLED 屏幕菜单导航、按键交互与内容渲染实现
 */

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
/** @brief 静态全局菜单单例实例 */
SenderMenu instance{};
} // 匿名命名空间

/**
 * @brief 获取菜单单例对象引用
 */
SenderMenu &sender_menu() noexcept
{
    return instance;
}

/** @brief 菜单项标题文本列表 */
static const char *titles[4] = {"Joystick", "Keys / Switches", "Attitude", "Communication"};

/**
 * @brief 内部辅助函数: 在指定垂直 Y 坐标处格式化绘制单行文本 (高度12字体)
 * @param[in] y   起始 Y 坐标 (0, 12, 24, 36, 48)
 * @param[in] fmt 格式化字符串
 */
static void row(unsigned y, const char *fmt, ...)
{
    char text[24];
    va_list a;
    va_start(a, fmt);
    (void)vsnprintf(text, sizeof text, fmt, a);
    va_end(a);
    bsp_oled_show_string(0, (uint8_t)y, text, 12, 1);
}

/**
 * @brief 初始化菜单状态为默认停留在摇杆项，处于主列表视图
 */
void SenderMenu::init(void)
{
    selected = MENU_PAGE_JOYSTICK;
    detail = false;
}

/**
 * @brief 向上导航光标 (循环上翻)
 */
void SenderMenu::navigate_up(void)
{
    selected = (menu_page_t)((selected + MENU_PAGE_COUNT - 1) % MENU_PAGE_COUNT);
}

/**
 * @brief 向下导航光标 (循环下翻)
 */
void SenderMenu::navigate_down(void)
{
    selected = (menu_page_t)((selected + 1) % MENU_PAGE_COUNT);
}

/**
 * @brief 确认键动作 (进入详情页或执行子页面快捷指令)
 */
void SenderMenu::enter(void)
{
    /* 若在主列表，按确认键进入对应详情页 */
    if (!detail)
    {
        detail = true;
        return;
    }
    /* 若已在详情页，按确认键执行该页面的快捷操作 */
    switch (selected)
    {
    case MENU_PAGE_JOYSTICK:
        bsp_joystick_calibrate_zero(); /* 校准摇杆中位 */
        break;
    case MENU_PAGE_KEY_SW:
        app_sender_reset_events();     /* 清空按键事件计数 */
        break;
    case MENU_PAGE_COMM_STAT:
        app_sender_reset_tx_count();   /* 重置发送统计计数 */
        break;
    default:
        break;
    }
}

/**
 * @brief 长按按键动作 (在姿态详情页长按 K3 重新校准陀螺仪)
 */
void SenderMenu::long_press(void)
{
    if (detail && selected == MENU_PAGE_ATTITUDE)
        app_sender_recalibrate();
}

/**
 * @brief 返回键动作 (从详情页返回主列表)
 */
void SenderMenu::back(void)
{
    detail = false;
}

/**
 * @brief 获取当前选中的菜单项
 */
menu_page_t SenderMenu::current_page(void)
{
    return selected;
}

/**
 * @brief 查询当前是否处于详情页
 */
bool SenderMenu::in_detail(void)
{
    return detail;
}

/**
 * @brief 渲染当前菜单页面或详情页面至 OLED 显存
 */
void SenderMenu::render(void)
{
    bsp_oled_clear();
    const sender_view_t *v = app_sender_view();

    /* 1. 一级主菜单列表视图 */
    if (!detail)
    {
        /* 顶部状态栏: 显示系统健康状态 (READY / CAL / CHECK) */
        row(0, "INVC / %s",
            v->state == APP_STATE_NORMAL        ? "READY"
            : v->state == APP_STATE_CALIBRATING ? "CAL"
                                                : "CHECK");
        /* 绘制 4 行菜单项列表，当前选中行前加 '>' 标记 */
        for (unsigned i = 0; i < 4; i++)
            row(12 + i * 12, "%c %s", i == (unsigned)selected ? '>' : ' ', titles[i]);
        return;
    }

    /* 2. 二级详情视图: 顶部显示 "< 标题" */
    row(0, "< %s", titles[selected]);
    switch (selected)
    {
    case MENU_PAGE_JOYSTICK:
        /* 显示归一化坐标、物理电压、ADC 状态与按键提示 */
        row(12, "X:%5d Y:%5d", v->telemetry.joy_x_raw, v->telemetry.joy_y_raw);
        row(24, "X:%4umV Y:%4umV", v->telemetry.joy_x_mv, v->telemetry.joy_y_mv);
        row(36, "ADC %s", (v->telemetry.key_mask & TELEMETRY_ADC_FAULT) ? "FAULT" : "OK");
        row(48, "K3:Center K4:Back");
        break;
    case MENU_PAGE_KEY_SW:
    {
        /* 显示按键电平掩码、拨码开关掩码、最近事件与事件总数 */
        static const char *names[] = {"NONE", "PRESS", "RELEASE", "LONG", "DOUBLE"};
        row(12, "KEY:%X SW:%X", v->telemetry.key_mask & 15, v->telemetry.switch_mask & 3);
        row(24, "K%u %s", (unsigned)v->last_key.id + 1,
            names[v->last_key.event <= KEY_EVENT_DOUBLE_CLICK ? v->last_key.event : 0]);
        row(36, "Events:%lu", (unsigned long)v->events);
        row(48, "K3:Clear K4:Back");
        break;
    }
    case MENU_PAGE_ATTITUDE:
        /* 显示欧拉角 (俯仰、横滚、偏航) 及 IMU 状态 / 标定进度 */
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
        /* 显示链路在线状态、已发包数、确认包数、重传数、超时丢包数 */
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

/* ==============================================================================
 * C 语言兼容导出接口实现
 * ============================================================================== */

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
