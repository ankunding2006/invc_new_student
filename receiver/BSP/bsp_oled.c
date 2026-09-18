#include "bsp_oled.h"
#include "oled_font.h"
#include "i2c.h"
#include <string.h>
#include <stdio.h>
/* Page-major: each hardware page is contiguous. Rendering is main-context only. */
static uint8_t gram[8][128], page, wire[129];
static bool online;
static uint16_t address = 0x78;
static uint32_t retry_at, phase_at;
static uint8_t recovery, pulses;
static void failed(void)
{
    online = false;
    retry_at = HAL_GetTick();
    page = 0;
}
bool bsp_oled_ready(void)
{
    return online;
}
bool bsp_oled_frame_start(void)
{
    return page == 0;
}
const uint8_t *bsp_oled_framebuffer(void)
{
    return &gram[0][0];
}
void bsp_oled_clear(void)
{
    memset(gram, 0, sizeof(gram));
}
void bsp_oled_init(void)
{
    bsp_oled_clear();
    page = 0;
    online = false;
    recovery = 0;
    retry_at = HAL_GetTick() - 900U;
}
static bool write_bytes(uint8_t *p, uint16_t n)
{
    if (__HAL_I2C_GET_FLAG(&hi2c1, I2C_FLAG_BUSY))
    {
        failed();
        return false;
    }
    if (HAL_I2C_Master_Transmit(&hi2c1, address, p, n, 4) != HAL_OK)
    {
        failed();
        return false;
    }
    return true;
}
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
    uint8_t init[] = {0x00, 0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0,    0x40, 0x8D, 0x14, 0x20, 0x02,
                      0xA1, 0xC8, 0xDA, 0x12, 0x81, 0x7F, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0xAF};
    if (write_bytes(init, sizeof(init)))
    {
        online = true;
        page = 0;
    }
}
void bsp_oled_draw_pixel(uint8_t x, uint8_t y, uint8_t color)
{
    if (x >= 128 || y >= 64)
        return;
    if (color)
        gram[y / 8][x] |= (uint8_t)(1U << (y % 8));
    else
        gram[y / 8][x] &= (uint8_t)~(1U << (y % 8));
}
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
void bsp_oled_show_num(uint8_t x, uint8_t y, int32_t n, uint8_t len, uint8_t size, uint8_t mode)
{
    char s[16];
    if (len > 11)
        len = 11;
    (void)snprintf(s, sizeof(s), "%*ld", (int)len, (long)n);
    bsp_oled_show_string(x, y, s, size, mode);
}
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
void bsp_oled_update_slice(void)
{
    if (!online || recovery)
        return;
    uint8_t cmd[] = {0x00, (uint8_t)(0xB0U + page), 0x00, 0x10};
    if (!write_bytes(cmd, sizeof cmd))
        return;
    wire[0] = 0x40;
    memcpy(wire + 1, gram[page], 128);
    if (write_bytes(wire, sizeof wire))
        page = (uint8_t)((page + 1U) % 8);
}
/* Compatibility entry: intentionally bounded to one page. */
void bsp_oled_update(void)
{
    bsp_oled_update_slice();
}
