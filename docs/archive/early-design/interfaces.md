# 早期模块接口草案

[文档导航](../../README.md) · [归档索引](../README.md)

> 历史资料：保留早期设计推导与当时约定，不代表当前代码或已完成验收。继续开发请阅读[当前状态](../../project-status.md)、[实现架构](../../architecture.md)与[开发约定](../../development.md)。


> **设计依据**：  
> 依据 2026 INVC 嵌入式软件考核要求与系统四层单向依赖架构规范，本文档明确手柄发送端（Sender）与接收端（Receiver）所有功能模块对外暴露的 API 函数原型、数据类型、枚举常量、输入依赖与返回值约定。  
> **架构纯洁性铁律**：Service 层模块为纯 ANSI C 实现，严禁包含片上 HAL 外设库头文件。

---

## 一、 跨端通用协议数据结构 (`srv_protocol.h` / `srv_protocol_parser.h`)

### 1.1 核心数据结构体与常量定义

```c
#ifndef __SRV_PROTOCOL_H
#define __SRV_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#define PROTOCOL_FRAME_HEADER_1   (0xAA)
#define PROTOCOL_FRAME_HEADER_2   (0x55)
#define PROTOCOL_FRAME_TAIL       (0x0D)

#define PROTOCOL_CMD_TELEMETRY    (0x01)
#define PROTOCOL_PAYLOAD_LEN      (16)   /* 遥测数据载荷定长 16 字节 */
#define PROTOCOL_FRAME_TOTAL_LEN  (22)   /* 帧头2 + 序号1 + 长度1 + CMD1 + 载荷16 + 校验1 + 帧尾1 = 23 */

#pragma pack(push, 1)
/**
 * @brief 遥测数据结构体 (定长 16 字节载荷)
 */
typedef struct {
    int16_t  joy_x_raw;        /* 摇杆 X 归一化映射值 (-1000 ~ +1000) */
    int16_t  joy_y_raw;        /* 摇杆 Y 归一化映射值 (-1000 ~ +1000) */
    uint16_t joy_x_mv;         /* 摇杆 X 物理采样电压 (0 ~ 3300 mV) */
    uint16_t joy_y_mv;         /* 摇杆 Y 物理采样电压 (0 ~ 3300 mV) */
    uint8_t  key_mask;         /* 4路轻触按键状态位掩码 (Bit0:K1, Bit1:K2, Bit2:K3, Bit3:K4; 1=按下) */
    uint8_t  switch_mask;      /* 2位拨码开关状态位掩码 (Bit0:SW1, Bit1:SW2; 1=有效高电平) */
    int16_t  pitch_cd;         /* 俯仰角 (单位: 0.1度, 范围 -1800 ~ +1800 对应 -180.0° ~ +180.0°) */
    int16_t  roll_cd;          /* 横滚角 (单位: 0.1度, 范围 -1800 ~ +1800 对应 -180.0° ~ +180.0°) */
    int16_t  yaw_cd;           /* 航向角 (单位: 0.1度, 范围 0 ~ 3600 对应 0.0° ~ 360.0°) */
} telemetry_payload_t;

/**
 * @brief 完整通信帧模型
 */
typedef struct {
    uint8_t             header1;     /* 0xAA */
    uint8_t             header2;     /* 0x55 */
    uint8_t             seq_id;      /* 0~255 环形自增流水号 */
    uint8_t             length;      /* 载荷长度 (PROTOCOL_PAYLOAD_LEN) */
    uint8_t             cmd;         /* 功能码 (PROTOCOL_CMD_TELEMETRY) */
    telemetry_payload_t payload;     /* 16字节数据载荷 */
    uint8_t             checksum;    /* 累加和校验 (从 seq_id 累加至 payload 末尾) */
    uint8_t             tail;        /* 0x0D */
} protocol_frame_t;
#pragma pack(pop)
```

---

## 二、 手柄发送端 (Sender) 模块接口定义

### 2.1 应用主控协调器 (`App/app_sender.h`)

* **职责**：手柄系统顶层生命周期调度（上电自检、零偏校准、固定周期采集与协议外发）。
```c
typedef enum {
    APP_STATE_INIT = 0,         /* 上电初始化与硬件自检 */
    APP_STATE_CALIBRATING,      /* 传感器零偏静止校准中 */
    APP_STATE_NORMAL,           /* 正常运行遥控模式 */
    APP_STATE_FAULT_DEGRADED    /* 外设掉线或总线异常降级运行 */
} app_state_t;

void        app_sender_init(void);
void        app_sender_task(void);
app_state_t app_sender_get_state(void);
```

### 2.2 多级 OLED 菜单交互模块 (`App/app_menu.h`)

* **职责**：管理 4 大核心页面状态机，响应按键导航事件，调度底层画点/字符接口渲染。
```c
typedef enum {
    MENU_PAGE_JOYSTICK = 0,     /* Page 1: 摇杆模拟量与电压显示页 */
    MENU_PAGE_KEY_SW,           /* Page 2: 按键与拨码开关事件页 */
    MENU_PAGE_ATTITUDE,         /* Page 3: IMU 姿态角监控与现场校准页 */
    MENU_PAGE_COMM_STAT,        /* Page 4: 发包计数与通信统计页 */
    MENU_PAGE_COUNT
} menu_page_t;

void        app_menu_init(void);
void        app_menu_navigate_up(void);
void        app_menu_navigate_down(void);
void        app_menu_action_enter(void);
void        app_menu_action_back(void);
void        app_menu_render(void);
menu_page_t app_menu_get_current_page(void);
```

### 2.3 协议序列化服务 (`Service/srv_protocol.h`)

* **职责**：纯 C 逻辑组帧，序列化遥测结构体，计算累加和。
```c
void     srv_protocol_init(void);
uint16_t srv_protocol_pack(uint8_t seq_id, const telemetry_payload_t *p_payload, uint8_t *p_out_buf, uint16_t max_len);
uint8_t  srv_protocol_calc_checksum(const uint8_t *p_data, uint16_t len);
```

### 2.4 六轴姿态滤波算法服务 (`Service/srv_imu_filter.h`)

* **职责**：纯 C 数学滤波，消除陀螺仪静态漂移，互补/Mahony 滤波融合加速度与角速度，解算欧拉角。**【严禁使用 DMP】**
```c
typedef struct {
    float pitch;    /* 俯仰角 (单位: 度, -180.0° ~ +180.0°) */
    float roll;     /* 横滚角 (单位: 度, -180.0° ~ +180.0°) */
    float yaw;      /* 航向角 (单位: 度, 0.0° ~ 360.0°) */
} imu_euler_t;

typedef struct {
    int16_t ax; int16_t ay; int16_t az; /* 原始加速度计 ADC 原始值 */
    int16_t gx; int16_t gy; int16_t gz; /* 原始陀螺仪 ADC 原始值 */
} imu_raw_t;

void srv_imu_filter_init(float sample_freq_hz);
void srv_imu_filter_update(const imu_raw_t *p_raw, float dt_s, imu_euler_t *p_euler);
void srv_imu_filter_calibrate_gyro(void);
bool srv_imu_filter_is_calibrated(void);
```

### 2.5 轻量环形无锁字节队列 (`Service/srv_ring_buffer.h`)

* **职责**：通用环形队列，隔离中断接收与主循环解析。
```c
typedef struct {
    uint8_t  *p_buffer;
    uint16_t  size;
    uint16_t  head;
    uint16_t  tail;
} ring_buffer_t;

void     srv_ring_buffer_init(ring_buffer_t *rb, uint8_t *storage, uint16_t size);
bool     srv_ring_buffer_push(ring_buffer_t *rb, uint8_t byte);
bool     srv_ring_buffer_pop(ring_buffer_t *rb, uint8_t *p_byte);
bool     srv_ring_buffer_peek(const ring_buffer_t *rb, uint16_t offset, uint8_t *p_byte);
uint16_t srv_ring_buffer_get_count(const ring_buffer_t *rb);
bool     srv_ring_buffer_is_empty(const ring_buffer_t *rb);
bool     srv_ring_buffer_is_full(const ring_buffer_t *rb);
void     srv_ring_buffer_clear(ring_buffer_t *rb);
```

### 2.6 双轴摇杆驱动 (`BSP/bsp_joystick.h`)

* **职责**：读取 ADC1 DMA 搬运的双通道采样数据，扣除中位零偏，死区滤波，计算真实物理电压与 -1000~1000 归一化值。
```c
typedef struct {
    int16_t  x_mapped;          /* 归一化输出: -1000 ~ +1000 */
    int16_t  y_mapped;          /* 归一化输出: -1000 ~ +1000 */
    uint16_t x_voltage_mv;      /* 实际物理电压: 0 ~ 3300 mV */
    uint16_t y_voltage_mv;      /* 实际物理电压: 0 ~ 3300 mV */
    bool     is_centered;       /* 处于中心死区标志 */
} joystick_data_t;

void bsp_joystick_init(void);
void bsp_joystick_calibrate_zero(void);
void bsp_joystick_get_data(joystick_data_t *p_data);
```

### 2.7 独立按键状态机驱动 (`BSP/bsp_key.h`)

* **职责**：4 路独立按键非阻塞消抖，精准识别短按按下、释放、持续 $\ge 2.0\text{s}$ 长按与 300ms 快速双击。
```c
typedef enum {
    KEY_ID_1 = 0, KEY_ID_2, KEY_ID_3, KEY_ID_4, KEY_ID_COUNT
} key_id_t;

typedef enum {
    KEY_EVENT_NONE = 0,
    KEY_EVENT_PRESS,            /* 短按按下瞬间 */
    KEY_EVENT_RELEASE,          /* 按键弹起释放瞬间 */
    KEY_EVENT_LONG_PRESS,       /* 持续按下 >= 2.0s 触发 */
    KEY_EVENT_DOUBLE_CLICK      /* 300ms 窗口内连续双击 */
} key_event_t;

typedef struct {
    key_id_t    id;
    key_event_t event;
} key_msg_t;

void    bsp_key_init(void);
void    bsp_key_tick_10ms(void);
bool    bsp_key_get_event(key_msg_t *p_msg);
uint8_t bsp_key_get_mask(void);
```

### 2.8 拨码开关驱动 (`BSP/bsp_switch.h`)

* **职责**：采样 2 位拨码开关稳态高低电平，提供状态掩码。
```c
typedef enum {
    SWITCH_ID_1 = 0, SWITCH_ID_2, SWITCH_ID_COUNT
} switch_id_t;

void     bsp_switch_init(void);
bool     bsp_switch_get_state(switch_id_t sw_id);
uint8_t  bsp_switch_get_mask(void);
```

### 2.9 MPU6500 六轴驱动 (`BSP/bsp_imu.h`)

* **职责**：片上硬件 SPI1 主机驱动（PA4~PA7），基于 LibDriver basic 模式读取原始 6 轴加速度计与陀螺仪寄存器补码。**【严禁使用 DMP】**
```c
bool bsp_imu_init(void);
bool bsp_imu_check_id(void);
bool bsp_imu_read_raw(imu_raw_t *p_raw);
```

### 2.10 SSD1306 OLED 显存驱动 (`BSP/bsp_oled.h`)

* **职责**：片上硬件 I2C1 驱动（PB6/PB7，400kHz Fast Mode），管理 1024 字节静态显存，提供全套点阵绘制接口。
```c
#define OLED_WIDTH   (128)
#define OLED_HEIGHT  (64)

void bsp_oled_init(void);
void bsp_oled_clear(void);
void bsp_oled_draw_pixel(uint8_t x, uint8_t y, uint8_t color);
void bsp_oled_show_char(uint8_t x, uint8_t y, char ch, uint8_t size, uint8_t mode);
void bsp_oled_show_string(uint8_t x, uint8_t y, const char *str, uint8_t size, uint8_t mode);
void bsp_oled_show_num(uint8_t x, uint8_t y, int32_t num, uint8_t len, uint8_t size, uint8_t mode);
void bsp_oled_draw_line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color);
void bsp_oled_update(void);     /* 将 1KB 静态显存通过硬件 I2C1 刷写至屏幕 */
```

### 2.11 双串口硬件驱动 (`BSP/bsp_usart.h`)

* **职责**：USART1 对接现成无线透明通道（纯硬件外发），USART2 对接 USB 转 TTL 输出诊断调试日志。
```c
typedef enum {
    USART_PORT_WIRELESS = 0,    /* USART1: PA9/PA10 115200 8N1 */
    USART_PORT_DEBUG            /* USART2: PA2/PA3 115200 8N1 */
} usart_port_t;

void     bsp_usart_init(void);
bool     bsp_usart_transmit(usart_port_t port, const uint8_t *p_data, uint16_t len);
uint16_t bsp_usart_receive(usart_port_t port, uint8_t *p_buf, uint16_t max_len);
void     bsp_usart_printf(usart_port_t port, const char *fmt, ...);
```

---

## 三、 接收端 (Receiver) 模块接口定义

### 3.1 接收端应用主控协调器 (`App/app_receiver.h`)

* **职责**：接收端顶层运行状态机（等待同步、联机通信、超时离线），调度流式解包，触发 1s 统计与上位机转发。
```c
typedef enum {
    RECEIVER_STATE_WAIT_SYNC = 0,   /* 等待接收首次有效数据包 */
    RECEIVER_STATE_CONNECTED,       /* 正常联机接收状态 */
    RECEIVER_STATE_OFFLINE          /* 通信超时断开状态 (>1000ms 未收包) */
} receiver_state_t;

void             app_receiver_init(void);
void             app_receiver_task(void);
receiver_state_t app_receiver_get_state(void);
```

### 3.2 接收端 OLED 界面排版渲染器 (`App/app_ui.h`)

* **职责**：排版展示手柄遥测数据、姿态角、1秒固定窗口通信频率 `Freq: XX Hz` 与实时丢包率 `Loss: X.X %`。
```c
void app_ui_init(void);
void app_ui_update(const telemetry_payload_t *p_telemetry, float freq_hz, float loss_rate_pct);
```

### 3.3 单字节流式协议解包有限状态机 (`Service/srv_protocol_parser.h`)

* **职责**：纯 C 单字节流式有限状态机，逐字节消费串口缓冲区，具备抗粘包、拆包与抗噪声自动再同步能力。
```c
typedef enum {
    PARSER_WAIT_HEADER_1 = 0,   /* 等待帧头 1: 0xAA */
    PARSER_WAIT_HEADER_2,       /* 等待帧头 2: 0x55 */
    PARSER_READ_SEQ,            /* 读取包序列号 */
    PARSER_READ_LEN,            /* 读取有效长度 */
    PARSER_READ_CMD,            /* 读取功能码 */
    PARSER_READ_PAYLOAD,        /* 逐字节读取 16 字节载荷 */
    PARSER_VERIFY_CHECKSUM,     /* 比对累加和校验 */
    PARSER_WAIT_TAIL            /* 验证帧尾 0x0D */
} parser_state_t;

typedef void (*protocol_frame_cb_t)(uint8_t seq_id, const telemetry_payload_t *p_payload);

void           srv_protocol_parser_init(protocol_frame_cb_t callback);
void           srv_protocol_parser_feed_byte(uint8_t byte);
void           srv_protocol_parser_reset(void);
parser_state_t srv_protocol_parser_get_state(void);
```

### 3.4 1秒固定窗口通信频率与丢包率统计引擎 (`Service/srv_stats.h`)

* **职责**：纯数学统计引擎，维护精确的 1000ms 时间窗口，计算有效包接收频率 $N_{\text{valid}}\text{ Hz}$，基于序列号环形差值求丢包率百分比。
```c
typedef struct {
    float    freq_hz;           /* 即时接收频率 (Hz, 整数或保留1位小数) */
    float    loss_rate_pct;     /* 过去 1 秒固定窗口丢包率 (0.0 ~ 100.0 %) */
    uint32_t total_received;    /* 累计接收有效包总数 */
    uint32_t total_lost;        /* 累计丢失包总数 */
} stats_metrics_t;

void srv_stats_init(void);
void srv_stats_on_packet_received(uint8_t seq_id);
void srv_stats_tick_1000ms(void);
void srv_stats_get_metrics(stats_metrics_t *p_metrics);
```

### 3.5 接收端双串口驱动 (`BSP/bsp_usart.h`)

* **职责**：USART1 接收中断无锁压入环形缓冲区，USART2 负责向 PC 电脑（VOFA+）高速转发推流。
```c
typedef enum {
    USART_PORT_WIRELESS = 0,    /* USART1: PA9/PA10 115200 8N1 */
    USART_PORT_PC_FORWARD       /* USART2: PA2/PA3 115200 8N1 */
} rx_usart_port_t;

void     bsp_usart_init(void);
bool     bsp_usart_transmit(rx_usart_port_t port, const uint8_t *p_data, uint16_t len);
uint16_t bsp_usart_receive(rx_usart_port_t port, uint8_t *p_buf, uint16_t max_len);
void     bsp_usart_forward_packet(const uint8_t *p_data, uint16_t len);
```
