# 早期调度与时序预算

[文档导航](../../README.md) · [归档索引](../README.md)

> 历史资料：保留早期设计推导与当时约定，不代表当前代码或已完成验收。继续开发请阅读[当前状态](../../project-status.md)、[实现架构](../../architecture.md)与[开发约定](../../development.md)。


> **设计依据**：  
> 依据 2026 INVC 嵌入式软件考核要求、系统行为规范（[SYSTEM_BEHAVIOR.md](state-machines.md)）、接口设计规范（[MODULE_INTERFACES.md](interfaces.md)）与数据设计规范（[DATA_DESIGN.md](data-model.md)），本文档确立手柄发送端（Sender）与接收端（Receiver）两端系统的时钟基准、协作式时间轮任务调度模型、各任务执行周期与最坏情况执行时间（WCET）预算、I2C 总线分页切片防饥饿机制以及中断优先级分配。  
> **核心铁律**：主循环与任务函数中**绝对严禁调用任何形式的阻塞延时 (`HAL_Delay()`)**（Rule 7）。所有任务调度均由非阻塞时间戳差值驱动。

---

> 核验说明（2026-09-17）：本文是设计预算，不是性能实测。80 次/秒 × 2.8ms = 22.4% CPU 阻塞占用；原表低估十倍。12.5ms 在 1ms tick 下不能直接精确表达，可在后续实现中用 12/13ms 交替截止时间。下方 NVIC 表是目标配置，当前实际为 SysTick=15、已启用 USART/DMA=0，接收端 USART2 IRQ 尚未启用。

## 一、 时钟基准与协作式时间轮调度架构

### 1.1 硬件时钟基准 (Clock Base)

* **主频时钟 (SYSCLK)**：外部 8MHz 无源晶振 (HSE) 经内部 PLL 9 倍频锁定为 **72.0 MHz**，单周期指令时间为 $T_{\text{cpu}} = \frac{1}{72\text{MHz}} \approx 13.88\text{ ns}$。
* **低速外设总线 (APB1)**：2 分频至 **36.0 MHz**（I2C1 与 USART2 挂载于此）。
* **高速外设总线 (APB2)**：不分频，保持 **72.0 MHz**（SPI1 与 USART1 挂载于此）。
* **系统滴答定时器 (SysTick)**：配置为每 $1.0\text{ ms}$（1000Hz）触发一次硬件中断，维护递增时间戳全局变量 `uwTick`（通过 `HAL_GetTick()` 读取）。

### 1.2 协作式非阻塞时间轮调度模型 (Cooperative Time-Wheel)

两端主控制循环均采用**时间戳差值无阻塞轮询**模型。每个周期性任务维护独立的“上一次执行时间戳”：
```c
/* 时间轮调度器通用模式 */
uint32_t current_tick = HAL_GetTick();

if ((current_tick - last_task_tick) >= TASK_PERIOD_MS) {
    last_task_tick = current_tick;
    task_function();
}
```
* **特点**：
  1. 避免使用重量级 RTOS，零任务切换开销，极大节省 RAM（无任务独立堆栈消耗）；
  2. 合作式调度会受到前序阻塞任务影响，2.8ms 页传输可产生毫秒级抖动，需实测；
  3. 通过原子时间差计算 `(current_tick - last_task_tick) >= PERIOD`，自动应对 `uwTick` 49.7 天单片机时钟回绕（Rollover）问题。

---

## 二、 手柄发送端 (Sender) 任务调度全景时序规范

### 2.1 发送端任务执行周期与时间预算表 (Sender Budget)

| 任务标识 | 函数入口 | 周期 (Period) | 频率 (Freq) | 最大允许耗时 (WCET) | 设计估算耗时（未实测） | CPU 时间占比 | 功能描述与时序要求 |
|---|---|---|---|---|---|---|---|
| `TASK_KEY_SCAN` | `bsp_key_tick_10ms()` | **10 ms** | 100 Hz | 100 μs | ~25 μs | 0.25% | 4路按键消抖、识别长按2s与300ms双击 |
| `TASK_JOYSTICK_SW` | `bsp_joystick_get_data()` | **20 ms** | 50 Hz | 200 μs | ~45 μs | 0.23% | 读取 ADC1 DMA 缓冲，计算归一化与物理电压 |
| `TASK_IMU_ATTITUDE`| `bsp_imu_read_raw()` + `srv_imu_filter_update()` | **20 ms** | 50 Hz | 800 μs | ~280 μs | 1.40% | SPI1 读取 MPU6500 原始数据，本地解算欧拉角 |
| `TASK_TELEMETRY_TX`| `srv_protocol_pack()` + `bsp_usart_transmit()` | **20 ms** | 50 Hz | 300 μs | ~80 μs | 0.40% | 组装 23 字节数据帧并写入 USART1 启动发送 |
| `TASK_OLED_SLICE` | `app_menu_update()` + `bsp_oled_slice_update()` | **12.5 ms** (切片) | 80 Hz | 3.5 ms | ~2.8 ms | 22.40% | 8页分页切片刷屏（每12.5ms刷1页，整屏100ms） |
| `TASK_DEBUG_LOG` | `bsp_usart_printf(USART2)` | **100 ms** | 10 Hz | 500 μs | ~160 μs | 0.16% | 本地 USART2 格式化输出比对日志 |
| **【发送端总体负载】**| — | — | — | — | — | **~ 24.84%** | **以上为估算平均负载，峰值与余量需实测** |

### 2.2 发送端核心时序相位对齐图 (Phase Alignment)

为了确保外发出的遥测包包含最新、时间差最小的物理量，`JOYSTICK` 采样、`IMU` 姿态解算与 `TELEMETRY` 封包必须保持**同周期、串行管道对齐**：

```text
时间轴 (ms)  0ms        5ms        10ms       15ms       20ms       25ms       30ms
            │          │          │          │          │          │          │
按键任务     [Key 10ms] ─────────► [Key 10ms] ─────────► [Key 10ms] ─────────► [Key 10ms]
            │                     │                     │                     │
采集解算外发 ┌─────────────────────────────────────────┐┌─────────────────────────────────────────┐
(20ms 流水线)│ 1.摇杆采集 (45μs)                        ││ 1.摇杆采集 (45μs)                        │
            │ 2.IMU读+姿态解算 (280μs)                 ││ 2.IMU读+姿态解算 (280μs)                 │
            │ 3.协议封包外发 (80μs)                    ││ 3.协议封包外发 (80μs)                    │
            └─────────────────────────────────────────┘└─────────────────────────────────────────┘
            │                                         │
OLED 切片   [Page 0]   [Page 1]   [Page 2]   [Page 3]   [Page 4]   [Page 5]   [Page 6] ...
(每12.5ms)  (2.8ms)    (2.8ms)    (2.8ms)    (2.8ms)    (2.8ms)    (2.8ms)    (2.8ms)
```

---

## 三、 接收端 (Receiver) 任务调度全景时序规范

### 3.1 接收端任务执行周期与时间预算表 (Receiver Budget)

| 任务标识 | 函数入口 / 机制 | 周期 (Period) | 调度频率 (Freq) | 最大允许耗时 (WCET) | 设计估算耗时（未实测） | CPU 时间占比 | 功能描述与时序要求 |
|---|---|---|---|---|---|---|---|
| `ISR_USART1_RX` | `USART1_IRQHandler()` | 异步中断 | 1150 Hz (50Hz包) | 5 μs / 字节 | ~2 μs | 0.23% | 硬件串口接收中断，将字节快速压入环形队列 |
| `TASK_STREAM_PARSE`| `srv_protocol_parser_feed_byte()` | **主循环轮询** | ≥200 Hz | 300 μs | ~60 μs | 1.20% | 消费环形队列，单字节有限状态机推进解包 |
| `TASK_PC_FORWARD` | `bsp_usart_forward_packet()` | 事件驱动 | 50 Hz (包触发) | 200 μs | ~50 μs | 0.25% | 成功解析有效帧后立即触发 USART2 向上位机推流 |
| `TASK_OLED_SLICE` | `app_ui_update()` + `bsp_oled_slice_update()` | **12.5 ms** (切片) | 80 Hz | 3.5 ms | ~2.8 ms | 22.40% | 接收端 OLED 8页分页切片刷屏（整屏100ms刷新） |
| `TASK_LINK_MONITOR`| `app_receiver_check_timeout()` | **100 ms** | 10 Hz | 50 μs | ~10 μs | 0.01% | 检查超时（最后有效包距今 >1000ms 判为离线） |
| `TASK_STATS_1SEC` | `srv_stats_tick_1000ms()` | **1000 ms** | 1 Hz | 50 μs | ~15 μs | 0.002% | 1.0 秒定时固定窗口结算频率 Hz 与丢包率 % |
| **【接收端总体负载】**| — | — | — | — | — | **~ 24.09%** | **以上为估算平均负载，峰值与余量需实测** |

---

## 四、 总线时序与长时操作切片防饥饿机制 (Anti-Starvation Slicing)

### 4.1 硬件总线物理传输时延分析

1. **SPI1 总线传输时延**：
   * 配置：72MHz 8 分频 = **9.0 MBits/s**，主全双工；
   * 读取 MPU6500 14 字节连续数据（Accel, Temp, Gyro）：
     $$T_{\text{spi}} = \frac{14 \times 8\text{ bits}}{9\times 10^6\text{ bps}} \approx 12.44\text{ μs}$$
   * 结论：SPI1 传输耗时仅 12.4 微秒，对系统 CPU 调度几乎零影响。
2. **USART1 无线串口传输时延**：
   * 配置：波特率 115200 8N1（每个字节 10 位物理周期）；
   * 发送 23 字节数据帧的物理传输耗时：
     $$T_{\text{uart\_tx}} = \frac{23 \times 10\text{ bits}}{115200\text{ bps}} \approx 1.996\text{ ms}$$
   * 传输机制：发送端调用 HAL 库启动外发，微控制器只需将首字节推入发送移位寄存器并开启 TXE/TC 或 DMA，**CPU 阻塞时间 $< 80\text{ μs}$**。
3. **I2C1 OLED 全屏传输冲突分析与长耗时瓶颈**：
   * 配置：400 kHz Fast Mode（每字节包含 8 位数据 + 1 位 ACK = 9 位）；
   * 0.96 寸屏幕全屏 1024 字节点阵数据，加上 8 个页寻址控制字节：
     $$T_{\text{i2c\_full}} = \frac{(1024 + 8 \times 3) \times 9\text{ bits}}{400\times 10^3\text{ bps}} \approx 23.58\text{ ms}$$
   * **【致命冲突风险】**：若在主循环中以单次阻塞函数调用一次性刷新 1024 字节显存，CPU 将被连续占用长达 $23.6\text{ ms}$！
     - 此时 10ms 扫描一次的按键消抖状态机将被强行打断，**漏掉 2 个完整采样点**；
     - 会直接导致按键双击判定失败（300ms 窗口失真）或长按时间计算误差。

### 4.2 解决方案：8 页分页切片刷屏机制 (Page Slicing Strategy)

为彻底解决 I2C 霸占 CPU 问题，设计 **分页时间片切片刷屏机制**：
* **切片原理**：SSD1306 屏幕由 Page 0 到 Page 7 共 8 页组成，每页包含 128 字节数据；
* **时序切片**：每隔 $12.5\text{ ms}$ 发送 1 个 Page（128 字节点阵数据 + 3 字节控制指令）；
* **单次切片耗时**：
  $$T_{\text{i2c\_page}} = \frac{(128 + 3) \times 9\text{ bits}}{400\times 10^3\text{ bps}} \approx 2.94\text{ ms}$$
* **效果评估**：
  1. 单次切片耗时仅 $2.9\text{ ms}$，远远小于 10ms 按键任务周期，CPU 迅速让出控制权；
  2. 经过 8 次切片（$8 \times 12.5\text{ ms} = 100\text{ ms}$），刚好完成一轮全屏刷新；
  3. 屏幕刷新率稳定在 **10 Hz**，既保证了视觉流畅度，又彻底消除了按键与传感器采样的饥饿隐患！

```c
/* 8 页切片异步更新状态机实现规范 */
void bsp_oled_slice_update(void) {
    static uint8_t current_page = 0;
    
    /* 单次只向硬件 I2C 发送当前页的 128 字节显存，耗时 < 3ms */
    /* s_oled_gram[128][8] 同页像素并不连续，先收集到页缓冲。 */
    static uint8_t page_buf[128];
    for (uint16_t x = 0; x < 128; ++x) {
        page_buf[x] = s_oled_gram[x][current_page];
    }
    bsp_oled_write_page(current_page, page_buf);
    
    current_page = (current_page + 1) % 8;
}
```

---

## 五、 中断优先级与嵌套配置规范 (NVIC Priority)

为防止中断嵌套死锁并确保串口数据零丢失，两端 NVIC 中断优先级统一定义如下（基于 STM32 `NVIC_PRIORITYGROUP_4`，4 位抢占优先级，0 位响应优先级）：

| 中断源 (Interrupt Source) | 抢占优先级 (Preempt Priority) | 触发条件 | 最大耗时 (ISR Time) | 行为描述 |
|---|---|---|---|---|
| **SysTick_IRQ** | **Priority 0 (最高)** | 每 1.0 ms 硬件溢出 | < 2 μs | 驱动全局时间基准 `HAL_IncTick()` |
| **USART1_IRQ** (无线串口接收) | **Priority 1** | 接收单字节 `RXNE` | < 3 μs | 提取 `DR` 寄存器字节并压入环形缓冲区 |
| **DMA1_Channel1_IRQ** (摇杆ADC)| **Priority 2** | 4 次循环搬运完成 | < 2 μs | 自动循环搬运，清除传输完成标志位 |
| **USART2_IRQ** (本地/PC串口) | **Priority 3** | 发送完成 / 接收 | < 3 μs | 调试日志输出或 PC 转发 |

* **安全准则**：
  1. 所有中断服务程序（ISR）执行时间严格控制在 $5\text{ μs}$ 以内；
  2. 严禁在 ISR 中调用任何 I2C 刷屏、协议打包解析或数学浮点运算。
