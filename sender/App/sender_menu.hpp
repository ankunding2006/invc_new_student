#pragma once
extern "C"
{
#include "app_menu.h"
}

namespace invc::sender
{
// 每个固件仅保留一个菜单实例：底层 C 服务维护硬件和算法状态。
// 构造过程不会访问硬件；请调用 init() 完成初始化。
class SenderMenu final
{
  public:
    SenderMenu() = default;
    SenderMenu(const SenderMenu &) = delete;
    SenderMenu &operator=(const SenderMenu &) = delete;
    void init();
    void navigate_up();
    void navigate_down();
    void enter();
    void back();
    void long_press();
    void render();
    menu_page_t current_page();
    bool in_detail();

  private:
    menu_page_t selected;
    bool detail;
};

// 供 C ABI 桥接层和 C++ 调用方共用；不进行动态内存分配。
SenderMenu &sender_menu() noexcept;
} // invc::sender 命名空间
