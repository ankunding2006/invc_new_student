#ifndef __APP_UI_H
#define __APP_UI_H

/**
 * @file app_ui.h
 * @brief 接收端 OLED 用户界面绘制接口
 */

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdbool.h>
#include "srv_protocol.h"

    /**
     * @brief 初始化接收端 UI 界面
     */
    void app_ui_init(void);

    /**
     * @brief 刷新并渲染接收端界面内容
     * @param[in] p_telemetry   遥测数据指针
     * @param[in] freq_hz       当前帧接收频率 (Hz)
     * @param[in] loss_rate_pct 当前通信丢包率百分比 (%)
     */
    void app_ui_update(const telemetry_payload_t *p_telemetry, float freq_hz, float loss_rate_pct);

#ifdef __cplusplus
}
#endif

#endif /* 应用界面头文件保护宏 */
