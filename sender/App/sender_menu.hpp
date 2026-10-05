#pragma once

/**
 * @file sender_menu.hpp
 * @brief 发送端 C++17 菜单管理单例类定义
 */

extern "C"
{
#include "app_menu.h"
}

namespace invc::sender
{
// 每个固件仅保留一个菜单实例：底层 C 服务维护硬件和算法状态。
// 构造过程不会访问硬件；请调用 init() 完成初始化。
/**
 * @brief 发送端 UI 菜单管理器类 (单例、零动态内存分配)
 */
class SenderMenu final
{
  public:
    SenderMenu() = default;
    SenderMenu(const SenderMenu &) = delete;
    SenderMenu &operator=(const SenderMenu &) = delete;

    /**
     * @brief 初始化菜单状态
     */
    void init();

    /**
     * @brief 光标向上导航
     */
    void navigate_up();

    /**
     * @brief 光标向下导航
     */
    void navigate_down();

    /**
     * @brief 确认进入详情或执行子页面动作
     */
    void enter();

    /**
     * @brief 返回主菜单列表
     */
    void back();

    /**
     * @brief 响应长按事件
     */
    void long_press();

    /**
     * @brief 渲染绘制当前页面至显存
     */
    void render();

    /**
     * @brief 获取当前选中的页面类型
     */
    menu_page_t current_page();

    /**
     * @brief 查询是否位于二级详情页
     */
    bool in_detail();

  private:
    menu_page_t selected; /**< 当前选中的菜单项 */
    bool detail;          /**< 详情页标志: true 为处于详情页，false 为主菜单列表 */
};

// 供 C ABI 桥接层和 C++ 调用方共用；不进行动态内存分配。
/**
 * @brief 获取发送端菜单单例对象引用
 */
SenderMenu &sender_menu() noexcept;
} // invc::sender 命名空间
