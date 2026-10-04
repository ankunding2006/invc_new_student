#pragma once
extern "C"
{
#include "app_receiver.h"
}

namespace invc::receiver
{
// 每个固件仅保留一个应用实例：底层 C 服务维护硬件和算法状态。
// 构造过程不会访问硬件；请调用 init() 完成初始化。
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

// 供 C ABI 桥接层和 C++ 调用方共用；不进行动态内存分配。
ReceiverApplication &receiver_application() noexcept;
} // invc::receiver 命名空间
