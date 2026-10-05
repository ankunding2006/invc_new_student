#ifndef BSP_OLED_H
#define BSP_OLED_H

/**
 * @file bsp_oled.h
 * @brief SSD1306 0.96寸 128x64 OLED 屏幕硬件驱动与绘图 API
 * 
 * 驱动架构特色:
 * 1. 内存显存缓存 (GRAM): 8 页 * 128 列 = 1024 字节；
 * 2. 分片刷新机制 (Time-Sliced Refresh): 每次仅刷新一页 (128 字节)，大幅降低 I2C 阻塞；
 * 3. 硬件 I2C 挂死自愈 (Bus Recovery): 具备 SCL 9 脉冲时钟震荡解锁与 I2C 重启。
 */

#include <stdint.h>
#include <stdbool.h>

/** @brief OLED 屏幕水平物理分辨率 (像素) */
#define OLED_WIDTH 128
/** @brief OLED 屏幕垂直物理分辨率 (像素) */
#define OLED_HEIGHT 64

/**
 * @brief 初始化 OLED 显存与驱动状态
 */
void bsp_oled_init(void);

/**
 * @brief 周期性轮询维护 OLED I2C 连接与总线自愈
 * @param[in] now 当前毫秒时间戳
 */
void bsp_oled_service(uint32_t now);

/**
 * @brief 查询 OLED 屏幕当前是否在线可用
 * @return true 设备在线且初始化成功; false 离线或正在复位恢复
 */
bool bsp_oled_ready(void);

/**
 * @brief 查询当前分片刷新是否正位于整帧起始页 (Page 0)
 * @return true 位于第 0 页 (适合在此刻重绘完整显存并同步更新); false 处于中间页刷新
 */
bool bsp_oled_frame_start(void);

/**
 * @brief 清空显存缓冲区 (全黑)
 */
void bsp_oled_clear(void);

/**
 * @brief 在显存指定坐标绘制单个像素点
 * @param[in] x     横坐标 (0 ~ 127)
 * @param[in] y     纵坐标 (0 ~ 63)
 * @param[in] color 颜色: 1 点亮(白), 0 熄灭(黑)
 */
void bsp_oled_draw_pixel(uint8_t x, uint8_t y, uint8_t color);

/**
 * @brief 在显存指定位置绘制单个 ASCII 字符
 * @param[in] x    起始横坐标 (0 ~ 127)
 * @param[in] y    起始纵坐标 (0 ~ 63)
 * @param[in] c    待显示的 ASCII 字符
 * @param[in] size 字体高度: 支持 12 (6x12) 或 16 (8x16)
 * @param[in] mode 显示模式: 1 正显(白字黑底), 0 反显(黑字白底)
 */
void bsp_oled_show_char(uint8_t x, uint8_t y, char c, uint8_t size, uint8_t mode);

/**
 * @brief 在显存指定位置绘制以空字符结尾的字符串
 * @param[in] x    起始横坐标 (0 ~ 127)
 * @param[in] y    起始纵坐标 (0 ~ 63)
 * @param[in] text 字符串指针
 * @param[in] size 字体高度: 12 或 16
 * @param[in] mode 显示模式: 1 正显, 0 反显
 */
void bsp_oled_show_string(uint8_t x, uint8_t y, const char *text, uint8_t size, uint8_t mode);

/**
 * @brief 在显存指定位置绘制格式化整数
 * @param[in] x    起始横坐标
 * @param[in] y    起始纵坐标
 * @param[in] n    待显示的数值 (支持正负数)
 * @param[in] len  总字符显示宽度 (右对齐，不足补空格)
 * @param[in] size 字体高度: 12 或 16
 * @param[in] mode 显示模式: 1 正显, 0 反显
 */
void bsp_oled_show_num(uint8_t x, uint8_t y, int32_t n, uint8_t len, uint8_t size, uint8_t mode);

/**
 * @brief 基于 Bresenham 算法在显存中绘制任意两点间的线段
 * @param[in] x1    起点 X 坐标
 * @param[in] y1    起点 Y 坐标
 * @param[in] x2    终点 X 坐标
 * @param[in] y2    终点 Y 坐标
 * @param[in] color 颜色: 1 点亮, 0 熄灭
 */
void bsp_oled_draw_line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color);

/**
 * @brief 兼容性刷新接口 (内部单次刷新一页)
 */
void bsp_oled_update(void);

/**
 * @brief 刷新一页 (128 字节) 显存到硬件屏幕
 * @note 屏幕共 8 页 (0~7)。调用 8 次完成一整帧刷新。
 */
void bsp_oled_update_slice(void);

/**
 * @brief 触发 I2C 总线挂死解锁与复位流程
 */
void bsp_oled_bus_unlock(void);

/**
 * @brief 获取显存缓冲区首地址只读指针 (用于测试或显存快照对比)
 * @return 指向 1024 字节显存首地址的指针
 */
const uint8_t *bsp_oled_framebuffer(void);

#endif
