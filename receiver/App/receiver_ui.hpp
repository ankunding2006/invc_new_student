#pragma once
extern "C"
{
#include "app_ui.h"
}

namespace invc::receiver
{
// 每个固件仅保留一个界面实例：底层 C 服务维护硬件和算法状态。
// 构造过程不会访问硬件；请调用 init() 完成初始化。
class ReceiverUi final
{
  public:
    ReceiverUi() = default;
    ReceiverUi(const ReceiverUi &) = delete;
    ReceiverUi &operator=(const ReceiverUi &) = delete;
    void init();
    void update(const telemetry_payload_t *p, float freq, float loss);
};

// 供 C ABI 桥接层和 C++ 调用方共用；不进行动态内存分配。
ReceiverUi &receiver_ui() noexcept;
} // invc::receiver 命名空间
