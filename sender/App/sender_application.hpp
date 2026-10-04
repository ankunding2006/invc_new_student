#pragma once
extern "C"
{
#include "app_sender.h"
#include "srv_link.h"
}

namespace invc::sender
{
// One application instance per firmware: the retained C services own singleton
// hardware/algorithm state. Construction never touches hardware; call init().
class SenderApplication final
{
  public:
    SenderApplication() = default;
    SenderApplication(const SenderApplication &) = delete;
    SenderApplication &operator=(const SenderApplication &) = delete;
    void init();
    void task();
    app_state_t current_state();
    const sender_view_t *snapshot();
    void recalibrate();
    void reset_tx_count();
    void reset_events();

  private:
    void log_key(key_msg_t key, uint32_t now);
    void drain_key_log();
    static bool radio_send(const uint8_t *p, uint16_t n);
    static bool due(uint32_t now, uint32_t *last, uint32_t period);
    static int16_t angle10(float a);
    void sample_and_send(uint32_t now);
    sender_view_t view;
    link_tx_t link;
    protocol_parser_t ack_parser;
    uint32_t key_tick, sample_tick, slice_tick, debug_tick, filter_tick, rx_errors;
    uint32_t boot_tick;
    uint32_t tx_base, ack_base, retry_base, expired_base;
    bool imu_was_ready;
    uint8_t slice_interval;
    struct EventLogEntry
    {
        key_msg_t key;
        uint32_t tick;
    };
    EventLogEntry event_log[32];
    uint8_t event_head, event_tail;
    uint32_t event_drops;
};

// Shared by the C ABI bridges and C++ callers; no dynamic allocation.
SenderApplication &sender_application() noexcept;
} // namespace invc::sender
