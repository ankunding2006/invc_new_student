#ifndef __BSP_OLED_H
#define __BSP_OLED_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#define OLED_WIDTH   (128)
#define OLED_HEIGHT  (64)

/* 显存坐标安全边界防护宏 (防数组越界踩踏 RAM) */
#define OLED_CHECK_BOUNDS(x, y) ((x) < OLED_WIDTH && (y) < OLED_HEIGHT)

void bsp_oled_init(void);
void bsp_oled_clear(void);
void bsp_oled_draw_pixel(uint8_t x, uint8_t y, uint8_t color);
void bsp_oled_show_char(uint8_t x, uint8_t y, char ch, uint8_t size, uint8_t mode);
void bsp_oled_show_string(uint8_t x, uint8_t y, const char *str, uint8_t size, uint8_t mode);
void bsp_oled_show_num(uint8_t x, uint8_t y, int32_t num, uint8_t len, uint8_t size, uint8_t mode);
void bsp_oled_draw_line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color);
void bsp_oled_update(void); /* 将 1024 字节显存通过硬件 I2C1 刷写至屏幕 (全屏阻塞) */
void bsp_oled_update_slice(void); /* 8页非阻塞切片刷新 (每页128字节, 10Hz帧率防任务饥饿) */

/* I2C1 总线防死锁自愈机制 (9个时钟脉冲强制释放从机SDA并恢复总线) */
void bsp_oled_bus_unlock(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_OLED_H */
