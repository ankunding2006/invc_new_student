#pragma once
extern "C"
{
#include "app_menu.h"
}

namespace invc::sender
{
// One application instance per firmware: the retained C services own singleton
// hardware/algorithm state. Construction never touches hardware; call init().
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

// Shared by the C ABI bridges and C++ callers; no dynamic allocation.
SenderMenu &sender_menu() noexcept;
} // namespace invc::sender
