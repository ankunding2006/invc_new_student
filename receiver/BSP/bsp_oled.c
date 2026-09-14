#include "bsp_oled.h"
#include <string.h>

/* 全局静态显存缓冲区 (128 * 64 / 8 = 1024 字节)，禁止局部变量定义以防栈溢出 */
static uint8_t s_oled_gram[128][8];

void bsp_oled_init(void)
{
    /* 硬件 I2C1 SSD1306 初始化寄存器指令序列 */
    bsp_oled_clear();
}

void bsp_oled_clear(void)
{
    memset(s_oled_gram, 0x00, sizeof(s_oled_gram));
}

void bsp_oled_draw_pixel(uint8_t x, uint8_t y, uint8_t color)
{
    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) {
        return;
    }
    if (color) {
        s_oled_gram[x][y / 8] |= (1 << (y % 8));
    } else {
        s_oled_gram[x][y / 8] &= ~(1 << (y % 8));
    }
}

void bsp_oled_show_char(uint8_t x, uint8_t y, char ch, uint8_t size, uint8_t mode)
{
    (void)x; (void)y; (void)ch; (void)size; (void)mode;
}

void bsp_oled_show_string(uint8_t x, uint8_t y, const char *str, uint8_t size, uint8_t mode)
{
    (void)x; (void)y; (void)str; (void)size; (void)mode;
}

void bsp_oled_show_num(uint8_t x, uint8_t y, int32_t num, uint8_t len, uint8_t size, uint8_t mode)
{
    (void)x; (void)y; (void)num; (void)len; (void)size; (void)mode;
}

void bsp_oled_draw_line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color)
{
    (void)x1; (void)y1; (void)x2; (void)y2; (void)color;
}

void bsp_oled_update(void)
{
    /* 调用片上硬件 HAL_I2C_Mem_Write 逐页刷新至 SSD1306 */
}
