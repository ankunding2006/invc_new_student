#ifndef BSP_OLED_H
#define BSP_OLED_H
#include <stdint.h>
#include <stdbool.h>
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
void bsp_oled_init(void);
void bsp_oled_service(uint32_t now);
bool bsp_oled_ready(void);
bool bsp_oled_frame_start(void);
void bsp_oled_clear(void);
void bsp_oled_draw_pixel(uint8_t x, uint8_t y, uint8_t color);
void bsp_oled_show_char(uint8_t x, uint8_t y, char c, uint8_t size, uint8_t mode);
void bsp_oled_show_string(uint8_t x, uint8_t y, const char *text, uint8_t size, uint8_t mode);
void bsp_oled_show_num(uint8_t x, uint8_t y, int32_t n, uint8_t len, uint8_t size, uint8_t mode);
void bsp_oled_draw_line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color);
void bsp_oled_update(void);
void bsp_oled_update_slice(void);
void bsp_oled_bus_unlock(void);
const uint8_t *bsp_oled_framebuffer(void);
#endif
