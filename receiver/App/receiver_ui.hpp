#pragma once

/**
 * @file receiver_ui.hpp
 * @brief 接收端 C++17 界面管理单例类定义
 */

extern "C"
{
#include "app_ui.h"
}

namespace invc::receiver
{
// 每个固件仅保留一个界面实例：底层 C 服务维护硬件和算法状态。
// 构造过程不会访问硬件；请调用 init() 完成初始化。
/**
 * @brief 接收端 OLED UI 渲染管理类 (单例、零动态内存分配)
 */
class ReceiverUi final
{
  public:
    ReceiverUi() = default;
    ReceiverUi(const ReceiverUi &) = delete;
    ReceiverUi &operator=(const ReceiverUi &) = delete;

    /**
     * @brief 初始化 UI
     */
    void init();

    /**
     * @brief 更新并重绘接收端 UI
     * @param[in] p    遥测数据指针
     * @param[in] freq 接收频率 (Hz)
     * @param[in] loss 丢包率 (%)
     */
    void update(const telemetry_payload_t *p, float freq, float loss);
};

// 供 C ABI 桥接层和 C++ 调用方共用；不进行动态内存分配。
/**
 * @brief 获取接收端 UI 单例对象引用
 */
ReceiverUi &receiver_ui() noexcept;
} // invc::receiver 命名空间
