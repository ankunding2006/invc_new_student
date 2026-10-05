/**
 * @file bsp_oled.c
 * @brief SSD1306 0.96寸 OLED 显示驱动、显存管理与 I2C 总线自愈实现
 */

#include "bsp_oled.h"
#include "oled_font.h"
#include "i2c.h"
#include <string.h>
#include <stdio.h>

/* Page-major: each hardware page is contiguous. Rendering is main-context only. */
/** @brief 软件显存 (GRAM): 8 页垂直分布，每页 128 列，每个字节 8 个垂直像素 (bit0 对应上方像素) */
static uint8_t gram[8][128], page, wire[129];
/** @brief 屏幕在线状态标志 */
static bool online;
/** @brief SSD1306 I2C 7位物理从机地址 (默认 0x78，备用 0x7A) */
static uint16_t address = 0x78;
/** @brief 重试与自愈时钟戳 */
static uint32_t retry_at, phase_at;
/** @brief 自愈状态机阶段与脉冲计数 */
static uint8_t recovery, pulses;

/**
 * @brief 内部记录显示通信失败并标记离线
 */
static void failed(void)
{
    online = false;
    retry_at = HAL_GetTick();
    page = 0;
}

/**
 * @brief 查询 OLED 屏幕是否在线就绪
 * @return true 在线就绪; false 离线
 */
bool bsp_oled_ready(void)
{
    return online;
}

/**
 * @brief 查询当前分片刷新是否处于第 0 页 (整帧起始)
 * @return true 处于第 0 页; false 处于第 1~7 页
 */
bool bsp_oled_frame_start(void)
{
    return page == 0;
}

/**
 * @brief 获取显存缓冲区首地址指针
 * @return 指向 gram[0][0] 的指针
 */
const uint8_t *bsp_oled_framebuffer(void)
{
    return &gram[0][0];
}

/**
 * @brief 清空显存缓冲区 (全黑清屏)
 */
void bsp_oled_clear(void)
{
    memset(gram, 0, sizeof(gram));
}

/**
 * @brief 初始化 OLED 显存与控制参数
 */
void bsp_oled_init(void)
{
    bsp_oled_clear();
    page = 0;
    online = false;
    recovery = 0;
    retry_at = HAL_GetTick() - 900U;
}

/**
 * @brief 通过硬件 I2C 发送字节流至 OLED
 * @param[in] p 数据指针
 * @param[in] n 发送字节数
 * @return true 传输成功; false 总线繁忙或发送出错
 */
static bool write_bytes(uint8_t *p, uint16_t n)
{
    /* 检查硬件 I2C 是否处于挂起/繁忙状态 */
    if (__HAL_I2C_GET_FLAG(&hi2c1, I2C_FLAG_BUSY))
    {
        failed();
        return false;
    }
    /* 调用 HAL 库阻塞式发送 (超时设为 4ms) */
    if (HAL_I2C_Master_Transmit(&hi2c1, address, p, n, 4) != HAL_OK)
    {
        failed();
        return false;
    }
    return true;
}

/**
 * @brief 触发 I2C 总线挂死故障恢复流程
 * 
 * 当从机因通信异常持续拉低 SDA 时，通过将 SCL/SDA 配置为 GPIO 开漏输出，
 * 产生多达 9 个时钟脉冲，迫使从机释放 SDA，随后重新复位 I2C 外设。
 */
void bsp_oled_bus_unlock(void)
{
    if (recovery)
        return;
    failed();
    if (HAL_I2C_DeInit(&hi2c1) != HAL_OK)
        return;
    GPIO_InitTypeDef cfg = {0};
    cfg.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    cfg.Mode = GPIO_MODE_OUTPUT_OD;
    cfg.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_WritePin(GPIOB, cfg.Pin, GPIO_PIN_SET);
    HAL_GPIO_Init(GPIOB, &cfg);
    recovery = 1;
    pulses = 0;
    phase_at = HAL_GetTick();
}

/**
 * @brief 总线自愈恢复状态机步进
 * @param[in] now 当前时间戳 (毫秒)
 */
static void recovery_step(uint32_t now)
{
    if ((uint32_t)(now - phase_at) < 1)
        return;
    phase_at = now;
    switch (recovery)
    {
    case 1:
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
        recovery = 2;
        break;
    case 2:
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
        recovery = 3;
        break;
    case 3:
        if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6) == GPIO_PIN_RESET)
        {
            recovery = 6;
            break;
        }
        if (++pulses >= 9 || HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7) == GPIO_PIN_SET)
        {
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_RESET);
            recovery = 4;
        }
        else
            recovery = 1;
        break;
    case 4:
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
        recovery = 5;
        break;
    case 5:
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);
        recovery = 6;
        break;
    default:
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_SET);
        __HAL_RCC_I2C1_FORCE_RESET();
        __HAL_RCC_I2C1_RELEASE_RESET();
        if (HAL_I2C_Init(&hi2c1) != HAL_OK)
            failed();
        recovery = 0;
        retry_at = now - 1000U;
        break;
    }
}

/**
 * @brief OLED 周期性后台维护任务 (包含重连、设备地址探测与屏幕初始化)
 * @param[in] now 当前时间戳 (毫秒)
 */
void bsp_oled_service(uint32_t now)
{
    if (recovery)
    {
        recovery_step(now);
        return;
    }
    if (online || (uint32_t)(now - retry_at) < 1000)
        return;
    retry_at = now;
    if (__HAL_I2C_GET_FLAG(&hi2c1, I2C_FLAG_BUSY) ||
        HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7) == GPIO_PIN_RESET)
    {
        bsp_oled_bus_unlock();
        return;
    }
    /* Probe both documented SSD1306 address selections; no assumed solder option. */
    address = 0x78;
    if (HAL_I2C_IsDeviceReady(&hi2c1, address, 1, 2) != HAL_OK)
    {
        address = 0x7A;
        if (HAL_I2C_IsDeviceReady(&hi2c1, address, 1, 2) != HAL_OK)
            return;
    }
    /* 发送 SSD1306 常用初始化配置命令序列 */
    uint8_t init[] = {0x00, 0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0,    0x40, 0x8D, 0x14, 0x20, 0x02,
                      0xA1, 0xC8, 0xDA, 0x12, 0x81, 0x7F, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0xAF};
    if (write_bytes(init, sizeof(init)))
    {
        online = true;
        page = 0;
    }
}

/**
 * @brief 在显存中绘制单个像素
 * @param[in] x     横坐标 (0~127)
 * @param[in] y     纵坐标 (0~63)
 * @param[in] color 颜色: 1 点亮, 0 熄灭
 */
void bsp_oled_draw_pixel(uint8_t x, uint8_t y, uint8_t color)
{
    if (x >= 128 || y >= 64)
        return;
    if (color)
        gram[y / 8][x] |= (uint8_t)(1U << (y % 8));
    else
        gram[y / 8][x] &= (uint8_t)~(1U << (y % 8));
}

/**
 * @brief 在显存中绘制 ASCII 字符
 * @param[in] x    X坐标
 * @param[in] y    Y坐标
 * @param[in] c    字符
 * @param[in] size 字体高度: 12 或 16
 * @param[in] mode 模式: 1 正显, 0 反显
 */
void bsp_oled_show_char(uint8_t x, uint8_t y, char c, uint8_t size, uint8_t mode)
{
    if (c < 32 || c > 126)
        c = '?';
    if (size != 12 && size != 16)
        return;
    unsigned width = size / 2;
    const uint8_t *glyph = size == 12 ? asc2_1206[(unsigned)c - 32] : asc2_1608[(unsigned)c - 32];
    for (unsigned col = 0; col < width; col++)
        for (unsigned row = 0; row < size; row++)
        {
            unsigned px = x + col, py = y + row;
            if (px >= 128 || py >= 64)
                continue;
            bool on = (glyph[col * 2 + row / 8] & (0x80U >> (row % 8))) != 0;
            bsp_oled_draw_pixel((uint8_t)px, (uint8_t)py, (uint8_t)(mode ? on : !on));
        }
}

/**
 * @brief 在显存中绘制字符串
 * @param[in] x    X坐标
 * @param[in] y    Y坐标
 * @param[in] s    字符串指针
 * @param[in] size 字体高度: 12 或 16
 * @param[in] mode 模式: 1 正显, 0 反显
 */
void bsp_oled_show_string(uint8_t x, uint8_t y, const char *s, uint8_t size, uint8_t mode)
{
    if (!s || (size != 12 && size != 16))
        return;
    unsigned cursor = x;
    while (*s && cursor + size / 2 <= 128)
    {
        bsp_oled_show_char((uint8_t)cursor, y, *s++, size, mode);
        cursor += size / 2;
    }
}

/**
 * @brief 在显存中绘制格式化数值
 * @param[in] x    X坐标
 * @param[in] y    Y坐标
 * @param[in] n    数值
 * @param[in] len  对齐字符宽度
 * @param[in] size 字体高度: 12 或 16
 * @param[in] mode 模式: 1 正显, 0 反显
 */
void bsp_oled_show_num(uint8_t x, uint8_t y, int32_t n, uint8_t len, uint8_t size, uint8_t mode)
{
    char s[16];
    if (len > 11)
        len = 11;
    (void)snprintf(s, sizeof(s), "%*ld", (int)len, (long)n);
    bsp_oled_show_string(x, y, s, size, mode);
}

/**
 * @brief 基于 Bresenham 算法画线
 * @param[in] x1    起点X
 * @param[in] y1    起点Y
 * @param[in] x2    终点X
 * @param[in] y2    终点Y
 * @param[in] color 颜色: 1 点亮, 0 熄灭
 */
void bsp_oled_draw_line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color)
{
    if (x1 >= 128 || x2 >= 128 || y1 >= 64 || y2 >= 64)
        return;
    int x = x1, y = y1, dx = x2 > x1 ? x2 - x1 : x1 - x2, dy = y2 > y1 ? y1 - y2 : y2 - y1,
        sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1, err = dx + dy;
    for (;;)
    {
        bsp_oled_draw_pixel((uint8_t)x, (uint8_t)y, color);
        if (x == x2 && y == y2)
            break;
        int e = 2 * err;
        if (e >= dy)
        {
            err += dy;
            x += sx;
        }
        if (e <= dx)
        {
            err += dx;
            y += sy;
        }
    }
}

/**
 * @brief 向屏幕硬件写入单页 (128 字节) 显存
 * 
 * 每次调用仅发送一页，将 1024 字节的大块传输拆分成 8 次分散在不同时间片完成，
 * 有效避免硬件 I2C 阻塞主循环与传感器采样。
 */
void bsp_oled_update_slice(void)
{
    if (!online || recovery)
        return;
    /* 发送设置页地址命令: 0x00(命令标识), 0xB0+page(设置页号), 0x00(列低地址), 0x10(列高地址) */
    uint8_t cmd[] = {0x00, (uint8_t)(0xB0U + page), 0x00, 0x10};
    if (!write_bytes(cmd, sizeof cmd))
        return;
    /* 发送 128 字节显存数据: 0x40 为数据标识 */
    wire[0] = 0x40;
    memcpy(wire + 1, gram[page], 128);
    if (write_bytes(wire, sizeof wire))
        page = (uint8_t)((page + 1U) % 8);
}

/* Compatibility entry: intentionally bounded to one page. */
/**
 * @brief 兼容刷新接口 (单次刷新一页)
 */
void bsp_oled_update(void)
{
    bsp_oled_update_slice();
}
