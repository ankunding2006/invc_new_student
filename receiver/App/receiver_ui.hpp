#pragma once
extern "C"
{
#include "app_ui.h"
}

namespace invc::receiver
{
// One application instance per firmware: the retained C services own singleton
// hardware/algorithm state. Construction never touches hardware; call init().
class ReceiverUi final
{
  public:
    ReceiverUi() = default;
    ReceiverUi(const ReceiverUi &) = delete;
    ReceiverUi &operator=(const ReceiverUi &) = delete;
    void init();
    void update(const telemetry_payload_t *p, float freq, float loss);
};

// Shared by the C ABI bridges and C++ callers; no dynamic allocation.
ReceiverUi &receiver_ui() noexcept;
} // namespace invc::receiver
