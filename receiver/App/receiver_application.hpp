#pragma once
extern "C"
{
#include "app_receiver.h"
}

namespace invc::receiver
{
// One application instance per firmware: the retained C services own singleton
// hardware/algorithm state. Construction never touches hardware; call init().
class ReceiverApplication final
{
  public:
    ReceiverApplication() = default;
    ReceiverApplication(const ReceiverApplication &) = delete;
    ReceiverApplication &operator=(const ReceiverApplication &) = delete;
    void init();
    void task();
    receiver_state_t current_state();
    const telemetry_payload_t *snapshot();
    uint32_t forwarding_drops();

  private:
    bool forward();
    static bool valid_payload(const telemetry_payload_t *p);
    receiver_state_t state;
    protocol_parser_t parser;
    telemetry_payload_t telemetry;
    stats_metrics_t metrics;
    uint32_t last_packet, stats_tick, slice_tick, rx_errors, pc_drops;
    uint8_t slice_interval, last_seq, ack[9];
    bool ack_pending, text_mode, stats_log_pending;
};

// Shared by the C ABI bridges and C++ callers; no dynamic allocation.
ReceiverApplication &receiver_application() noexcept;
} // namespace invc::receiver
