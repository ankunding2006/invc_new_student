/**
 * @file bsp_joystick.c
 * @brief 摇杆 ADC1 + DMA 双缓冲采样、均值滤波与中位校准驱动实现
 */

#include "bsp_joystick.h"
#include "srv_input.h"
#include "system_config.h"
#include "adc.h"
#include <string.h>

/** @brief DMA 循环采样的 256 点样本缓冲区 (包含 128 个 X 样本和 128 个 Y 样本交叉排列) */
static volatile uint16_t dma_samples[256];
/** @brief DMA 中断更新纪元计数 (用于检测主循环读取与 DMA 中断之间的竞态) */
static volatile uint32_t epoch;
/** @brief 最近完成采样的缓冲区半区标识: 0 表示前半区 (0~127), 1 表示后半区 (128~255) */
static volatile uint8_t complete_half;
/** @brief 硬件 ADC 发生错误故障标志 */
static volatile bool failed;
/** @brief 状态标志: active=采样已启动, center_valid=已校准中位基准, sample_valid=样本有效 */
static bool active, center_valid, sample_valid;
/** @brief 最近一次有效的物理电压采样值 (mV) */
static uint16_t last_x, last_y;
/** @brief 摇杆标定的中位物理基准电压 (mV), 默认理论中值 1650mV (3300/2) */
static uint16_t center_x = 1650, center_y = 1650;
/** @brief 故障重试时间戳 */
static uint32_t retry_ms;

/**
 * @brief 初始化 ADC1 硬件并开启 DMA 循环多通道转换
 */
void bsp_joystick_init(void)
{
    epoch = 0;
    failed = false;
    active = false;
    center_valid = false;
    sample_valid = false;
    retry_ms = HAL_GetTick();
    /* 设置 DMA 通道中断优先级为 2,0 (低于通信中断，高于普通应用) */
    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 2, 0);
    /* 启动 ADC 自校准 */
    if (HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK)
    {
        failed = true;
        return;
    }
    /* 启动双通道连续扫描 DMA 循环转换 (每次填满 256 个 uint16_t 样本) */
    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)(void *)dma_samples, 256) != HAL_OK)
    {
        failed = true;
        return;
    }
    active = true;
}

/**
 * @brief DMA 半传输完成中断回调 (前半区 128 样本转换完毕)
 * @param[in] h ADC 句柄
 */
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *h)
{
    if (h == &hadc1)
    {
        complete_half = 0;
        epoch++;
    }
}

/**
 * @brief DMA 全传输完成中断回调 (后半区 128 样本转换完毕)
 * @param[in] h ADC 句柄
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *h)
{
    if (h == &hadc1)
    {
        complete_half = 1;
        epoch++;
    }
}

/**
 * @brief ADC 硬件错误中断回调
 * @param[in] h ADC 句柄
 */
void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *h)
{
    if (h == &hadc1)
        failed = true;
}

/**
 * @brief 查询摇杆是否就绪且具备有效采样数据
 * @return true 就绪; false 未就绪或发生错误
 */
bool bsp_joystick_ready(void)
{
    return active && !failed && sample_valid;
}

/**
 * @brief 内部计算最新完成半区的 64 组 X/Y 采样电压均值
 * @param[out] x 输出 X 轴电压 (mV)
 * @param[out] y 输出 Y 轴电压 (mV)
 * @return true 采样计算成功且未发生 DMA 覆写竞态; false 发生竞态冲突或未初始化
 */
static bool voltage(uint16_t *x, uint16_t *y)
{
    if (!active || failed || epoch == 0)
        return false;
    /* 记录当前采样纪元编号 */
    uint32_t before = epoch, sx = 0, sy = 0;
    /* 确定当前稳定的半区基地址: complete_half=1 则取后半区 128~255，否则取前半区 0~127 */
    unsigned base = complete_half ? 128 : 0;
    /* ISR only publishes a half index; averaging runs in main context. */
    /* 累加 64 组采样样本 (X 与 Y 交叉存储) */
    for (unsigned i = base; i < base + 128; i += 2)
    {
        sx += dma_samples[i];
        sy += dma_samples[i + 1];
    }
    /* 临界一致性校验: 若计算期间 epoch 发生跳变，说明 DMA 已覆写当前半区，数据不一致丢弃 */
    if (before != epoch || failed)
        return false;
    /* 换算为物理毫伏电压: mv = (sum * 3300 + 64*4095/2) / (64 * 4095), 包含整数四舍五入 */
    *x = (uint16_t)((sx * JOYSTICK_VREF_MV + 64U * 4095U / 2U) / (64U * 4095U));
    *y = (uint16_t)((sy * JOYSTICK_VREF_MV + 64U * 4095U / 2U) / (64U * 4095U));
    last_x = *x;
    last_y = *y;
    sample_valid = true;
    return true;
}

/**
 * @brief 执行摇杆中心中位物理电压校准
 */
void bsp_joystick_calibrate_zero(void)
{
    uint16_t x, y;
    /* 仅当摇杆处于合理物理中位区间 (500mV ~ 2800mV) 时确认该校准中点有效 */
    if (voltage(&x, &y) && x > 500 && x < 2800 && y > 500 && y < 2800)
    {
        center_x = x;
        center_y = y;
        center_valid = true;
    }
}

/**
 * @brief 获取完整的摇杆采样数据结构体
 * @param[out] out 接收数据的结构体指针
 */
void bsp_joystick_get_data(joystick_data_t *out)
{
    if (!out)
        return;
    uint16_t x, y;
    /* 读取最新均值电压，若当前受 DMA 边界竞态干扰则尝试复用上一有效样本 */
    if (!voltage(&x, &y))
    {
        /* A DMA boundary invalidates this average, not the last coherent sample. */
        if (!bsp_joystick_ready())
        {
            memset(out, 0, sizeof(*out));
            return;
        }
        x = last_x;
        y = last_y;
    }
    /* 若尚未标定过中位，自动触发一次校准 */
    if (!center_valid)
        bsp_joystick_calibrate_zero();
    out->x_voltage_mv = x;
    out->y_voltage_mv = y;
    /* 调用服务层进行死区过滤与归一化映射 (-1000 ~ +1000) */
    out->x_mapped = srv_joystick_map(x, center_x);
    out->y_mapped = srv_joystick_map(y, center_y);
    out->is_centered = out->x_mapped == 0 && out->y_mapped == 0;
}

/**
 * @brief 摇杆后台自愈维护任务
 * @param[in] now 当前时间戳 (毫秒)
 */
void bsp_joystick_service(uint32_t now)
{
    /* 若处于失败或未激活状态，每隔 1000ms 尝试复位并重启 ADC DMA 采样 */
    if ((failed || !active) && (uint32_t)(now - retry_ms) >= 1000)
    {
        retry_ms = now;
        if (HAL_ADC_Stop_DMA(&hadc1) == HAL_OK)
            bsp_joystick_init();
    }
}
