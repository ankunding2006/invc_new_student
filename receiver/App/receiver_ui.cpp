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
// Zero-initialized static storage preserves the original C startup state.
static_assert(std::is_trivially_default_constructible_v<ReceiverUi>);
static_assert(std::is_trivially_destructible_v<ReceiverUi>);
ReceiverUi instance{};
} // namespace
ReceiverUi &receiver_ui() noexcept
{
    return instance;
}

static void row(unsigned y, const char *fmt, ...)
{
    char s[24];
    va_list a;
    va_start(a, fmt);
    (void)vsnprintf(s, sizeof s, fmt, a);
    va_end(a);
    bsp_oled_show_string(0, (uint8_t)y, s, 12, 1);
}
void ReceiverUi::init(void)
{
    bsp_oled_clear();
}
void ReceiverUi::update(const telemetry_payload_t *p, float freq, float loss)
{
    if (!p)
        return;
    bsp_oled_clear();
    receiver_state_t state = app_receiver_get_state();
    row(0, "%s %uHz L:%u.%u%%",
        state == RECEIVER_STATE_CONNECTED ? "LINK"
        : state == RECEIVER_STATE_OFFLINE ? "LOST"
                                          : "WAIT",
        (unsigned)freq, (unsigned)loss, (unsigned)(loss * 10) % 10);
    row(12, "X:%5d Y:%5d", p->joy_x_raw, p->joy_y_raw);
    row(24, "KEY:%X SW:%X ERR:%X", p->key_mask & 15, p->switch_mask & 3, p->key_mask >> 4);
    row(36, "P:%5d R:%5d", p->pitch_cd, p->roll_cd);
    row(48, "Y:%5d (0.1 deg)", p->yaw_cd);
}

} // namespace invc::receiver

extern "C" void app_ui_init(void)
{
    invc::receiver::receiver_ui().init();
}

extern "C" void app_ui_update(const telemetry_payload_t *p, float freq, float loss)
{
    invc::receiver::receiver_ui().update(p, freq, loss);
}
