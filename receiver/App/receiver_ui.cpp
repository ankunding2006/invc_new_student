/**
 * @file receiver_ui.cpp
 * @brief 接收端 OLED 用户界面绘制实现
 */

#include "receiver_ui.hpp"
extern "C"
{
#include "app_ui.h"
#include "app_receiver.h"
#include "bsp_oled.h"
}
#include <stdio.h>
#include <stdarg.h>
#include <type_traits>

namespace invc::receiver
{
namespace
{
// 零初始化的静态存储，保持原有 C 启动状态。
static_assert(std::is_trivially_default_constructible_v<ReceiverUi>);
static_assert(std::is_trivially_destructible_v<ReceiverUi>);
/** @brief 接收端 UI 单例实例 */
ReceiverUi instance{};
} // 匿名命名空间

/**
 * @brief 获取接收端 UI 单例引用
 */
ReceiverUi &receiver_ui() noexcept
{
    return instance;
}

/**
 * @brief 内部行文本格式化绘制
 * @param[in] y   起始 Y 坐标
 * @param[in] fmt 格式化字符串
 */
static void row(unsigned y, const char *fmt, ...)
{
    char s[24];
    va_list a;
    va_start(a, fmt);
    (void)vsnprintf(s, sizeof s, fmt, a);
    va_end(a);
    bsp_oled_show_string(0, (uint8_t)y, s, 12, 1);
}

/**
 * @brief 初始化 UI 并清空显存
 */
void ReceiverUi::init(void)
{
    bsp_oled_clear();
}

/**
 * @brief 绘制接收端监控页面
 * 
 * 画面排布:
 *  - 第 0 行 (Y=0):  链路状态 (LINK/LOST/WAIT)、接收频率 (Hz)、丢包率 (%)
 *  - 第 1 行 (Y=12): 摇杆 X/Y 归一化输出数值
 *  - 第 2 行 (Y=24): 按键掩码、拨码开关掩码、故障标志掩码
 *  - 第 3 行 (Y=36): 俯仰角 P、横滚角 R (0.1度单位)
 *  - 第 4 行 (Y=48): 偏航角 Y (0.1度单位)
 * 
 * @param[in] p    遥测载荷数据指针
 * @param[in] freq 实际有效帧接收频率 (Hz)
 * @param[in] loss 丢包率百分比 (%)
 */
void ReceiverUi::update(const telemetry_payload_t *p, float freq, float loss)
{
    if (!p)
        return;
    bsp_oled_clear();
    receiver_state_t state = app_receiver_get_state();
    /* 顶部状态栏: LINK(在线), LOST(超时离线), WAIT(等待初次同步) */
    row(0, "%s %uHz L:%u.%u%%",
        state == RECEIVER_STATE_CONNECTED ? "LINK"
        : state == RECEIVER_STATE_OFFLINE ? "LOST"
                                          : "WAIT",
        (unsigned)freq, (unsigned)loss, (unsigned)(loss * 10) % 10);
    /* 摇杆坐标行 */
    row(12, "X:%5d Y:%5d", p->joy_x_raw, p->joy_y_raw);
    /* 输入与故障标志行 */
    row(24, "KEY:%X SW:%X ERR:%X", p->key_mask & 15, p->switch_mask & 3, p->key_mask >> 4);
    /* 姿态俯仰与横滚行 */
    row(36, "P:%5d R:%5d", p->pitch_cd, p->roll_cd);
    /* 姿态偏航行 */
    row(48, "Y:%5d (0.1 deg)", p->yaw_cd);
}

} // invc::receiver 命名空间

/* ==============================================================================
 * C 语言兼容导出接口实现
 * ============================================================================== */

extern "C" void app_ui_init(void)
{
    invc::receiver::receiver_ui().init();
}

extern "C" void app_ui_update(const telemetry_payload_t *p, float freq, float loss)
{
    invc::receiver::receiver_ui().update(p, freq, loss);
}
