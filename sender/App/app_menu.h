#ifndef APP_MENU_H
#define APP_MENU_H

/**
 * @file app_menu.h
 * @brief 发送端 OLED 屏幕多级菜单管理与 C 接口导出
 * 
 * 菜单层级结构:
 *  - 主菜单列表 (4项):
 *    1. Joystick (摇杆实时数据与校准)
 *    2. Keys / Switches (按键与拨码开关检测)
 *    3. Attitude (六轴姿态欧拉角与标定状态)
 *    4. Communication (无线链路与收发/丢包统计)
 *  - 详情页 (二级子页面):
 *    在对应子页面中可查看具体细节数值，并执行清零、标定等动作交互。
 */

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdbool.h>

    /**
     * @brief 菜单页面类型枚举
     */
    typedef enum
    {
        MENU_PAGE_JOYSTICK,   /**< 摇杆页面 (坐标、电压与中点校准) */
        MENU_PAGE_KEY_SW,     /**< 按键与开关页面 (按键事件与状态) */
        MENU_PAGE_ATTITUDE,   /**< 姿态页面 (俯仰/横滚/偏航与长按校准) */
        MENU_PAGE_COMM_STAT,  /**< 通信统计页面 (收发计数、重传、丢包与重置) */
        MENU_PAGE_COUNT       /**< 页面总数 (4) */
    } menu_page_t;

    /**
     * @brief 初始化菜单状态
     */
    void app_menu_init(void);

    /**
     * @brief 向上导航 (K1 触发: 光标上移)
     */
    void app_menu_navigate_up(void);

    /**
     * @brief 向下导航 (K2 触发: 光标下移)
     */
    void app_menu_navigate_down(void);

    /**
     * @brief 确认进入详情或执行页面操作 (K3 触发)
     */
    void app_menu_action_enter(void);

    /**
     * @brief 返回上一级主菜单列表 (K4 触发)
     */
    void app_menu_action_back(void);

    /**
     * @brief 长按动作响应 (K3 长按触发，如姿态页重新标定)
     */
    void app_menu_action_long(void);

    /**
     * @brief 渲染绘制当前菜单页面至 OLED 显存
     */
    void app_menu_render(void);

    /**
     * @brief 获取当前选中的菜单页面
     * @return 页面枚举
     */
    menu_page_t app_menu_get_current_page(void);

    /**
     * @brief 查询当前是否正处于二级详情页中
     * @return true 在详情页; false 在主菜单列表中
     */
    bool app_menu_in_detail(void);

#ifdef __cplusplus
}
#endif
#endif
