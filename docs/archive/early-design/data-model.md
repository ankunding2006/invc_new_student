# 早期数据模型与资源预算

[文档导航](../../README.md) · [归档索引](../README.md)

> 历史资料：保留早期设计推导与当时约定，不代表当前代码或已完成验收。继续开发请阅读[当前状态](../../project-status.md)、[实现架构](../../architecture.md)与[开发约定](../../development.md)。


> **设计依据**：  
> 依据 2026 INVC 嵌入式软件考核要求、系统行为规范（[SYSTEM_BEHAVIOR.md](state-machines.md)）与接口设计规范（[MODULE_INTERFACES.md](interfaces.md)），本文档确立手柄发送端（Sender）与接收端（Receiver）全系统的数据模型、内存预算、定点数缩放、通信数据帧终态规格及显存点阵映射。  
> **核心原则**：纯 ANSI C 类型定义、1 字节紧凑对齐、小端序存储、静态全局显存分配、零动态内存申请。

---

## 一、 通信数据帧终态规格详解 (23 字节定长帧)

系统通信采用紧凑、高抗干扰的二进制定长数据帧。取消前期所有 TBD 与候选状态，正式确立以下 23 字节帧布局：

### 1.1 帧字节排布与字段规格表

| 字节偏移 (Offset) | 字段名称 (Field) | 类型 (Type) | 字节数 (Bytes) | 约定值 / 取值范围 | 物理量纲 / 分辨率 (Resolution) | 详细描述与处理方式 |
|---|---|---|---|---|---|---|
| **Byte 0** | `Header_1` | `uint8_t` | 1 | `0xAA` | 固定常数 | 帧起始同步字 1，用于在连续噪声流中检索帧头 |
| **Byte 1** | `Header_2` | `uint8_t` | 1 | `0x55` | 固定常数 | 帧起始同步字 2，与 `0xAA` 构成交替位前导码 |
| **Byte 2** | `Seq_ID` | `uint8_t` | 1 | `0 ~ 255` | 1 (无量纲) | 帧流水号，发送端每次发包自增 1，溢出归零，接收端用于计算丢包 |
| **Byte 3** | `Length` | `uint8_t` | 1 | `16` (`0x10`) | 1 字节 | 后续载荷数据（Payload）字节长度，固定为 16 字节 |
| **Byte 4** | `CMD` | `uint8_t` | 1 | `0x01` | 固定常数 | 功能码：`0x01` 代表综合遥测数据帧 (Telemetry Frame) |
| **Byte 5~6** | `Joy_X_Raw` | `int16_t` | 2 | `-1000 ~ +1000` | 1 (归一化) | 摇杆 X 轴归一化值，小端序（Byte 5 为低字节，Byte 6 为高字节） |
| **Byte 7~8** | `Joy_Y_Raw` | `int16_t` | 2 | `-1000 ~ +1000` | 1 (归一化) | 摇杆 Y 轴归一化值，小端序（Byte 7 为低字节，Byte 8 为高字节） |
| **Byte 9~10** | `Joy_X_mV` | `uint16_t` | 2 | `0 ~ 3300` | 1 mV | 摇杆 X 轴真实物理采样电压，小端序 |
| **Byte 11~12**| `Joy_Y_mV` | `uint16_t` | 2 | `0 ~ 3300` | 1 mV | 摇杆 Y 轴真实物理采样电压，小端序 |
| **Byte 13** | `Key_Mask` | `uint8_t` | 1 | `0x00 ~ 0x0F` | 位掩码 | 4路轻触按键状态：Bit0=KEY1, Bit1=KEY2, Bit2=KEY3, Bit3=KEY4 (1=按下) |
| **Byte 14** | `Switch_Mask` | `uint8_t` | 1 | `0x00 ~ 0x03` | 位掩码 | 2位拨码开关状态：Bit0=SW1, Bit1=SW2 (1=高电平) |
| **Byte 15~16**| `Pitch_cd` | `int16_t` | 2 | `-1800 ~ +1800` | 0.1° (十分之一度) | 俯仰角定点数，分辨率 0.1°，对应物理角 $-180.0^\circ \sim +180.0^\circ$，小端序 |
| **Byte 17~18**| `Roll_cd` | `int16_t` | 2 | `-1800 ~ +1800` | 0.1° (十分之一度) | 横滚角定点数，分辨率 0.1°，对应物理角 $-180.0^\circ \sim +180.0^\circ$，小端序 |
| **Byte 19~20**| `Yaw_cd` | `int16_t` | 2 | `0 ~ 3600` | 0.1° (十分之一度) | 航向角定点数，分辨率 0.1°，对应物理角 $0.0^\circ \sim 360.0^\circ$，小端序 |
| **Byte 21** | `Checksum` | `uint8_t` | 1 | `0x00 ~ 0xFF` | 8-bit 累加和 | 校验和：从 Byte 2 (`Seq_ID`) 累加至 Byte 20 (`Yaw_cd` 高字节) 溢出取低 8 位 |
| **Byte 22** | `Tail` | `uint8_t` | 1 | `0x0D` (`\r`) | 固定常数 | 帧尾标识符，辅助帧边界校验与断帧防护 |

### 1.2 校验和数学模型与计算示例

* **计算范围**：从 `Seq_ID` (Byte 2) 开始，包含 `Length` (Byte 3)、`CMD` (Byte 4) 以及全部 16 字节载荷 (Byte 5 ~ Byte 20)，共计 19 字节。
* **算法公式**：
  $$\text{Checksum} = \left( \sum_{i=2}^{20} \text{Frame}[i] \right) \pmod{256}$$
* **C 语言标准实现**：
  ```c
  uint8_t srv_protocol_calc_checksum(const uint8_t *p_data, uint16_t len) {
      uint8_t sum = 0;
      for (uint16_t i = 0; i < len; i++) {
          sum += p_data[i];
      }
      return sum;
  }
  ```

---

## 二、 MCU 内存预算与 RAM/Flash 分配模型 (Memory Budget)

针对主控微控制器 STM32F103C8T6（LQFP48 封装）：
* **片上 SRAM 容量**：$20\text{KB} = 20480\text{ 字节}$（地址区间 `0x20000000 ~ 0x20004FFF`）；
* **片上 Flash 容量**：$64\text{KB} = 65536\text{ 字节}$（地址区间 `0x08000000 ~ 0x0800FFFF`）。

### 2.1 SRAM 静态分配全景预算表

| 内存用途分区 | 变量名称 / 模块 | 存储区分类 | 分配大小 (Bytes) | 占 SRAM 比例 | 架构安全规则与防溢出设计 |
|---|---|---|---|---|---|
| **MCU 运行栈 (Stack)** | `_Min_Stack_Size` | `.stack` | **2048 (0x800)** | **10.0%** | CubeMX 显式锁定为 2KB，保证中断嵌套与函数调用安全 |
| **堆空间 (Heap)** | `_Min_Heap_Size` | `.heap` | 512 (0x200) | 2.5% | 全系统严格禁止动态内存分配（Rule 6），基本保持闲置 |
| **OLED 全屏静态显存** | `s_oled_gram` (`bsp_oled.c`) | `.bss` (静态全局) | **1024 (0x400)** | **5.0%** | $128 \times 64 / 8$ 字节，严禁在函数栈分配局部显存 |
| **无线接收环形缓冲区** | `s_rx_ring_buf` (`bsp_usart.c`)| `.bss` (静态全局) | **256 (0x100)** | **1.25%** | 容纳 11 包以上 23 字节数据帧，防 50Hz 高频丢包 |
| **无线发送与封包缓冲** | `s_tx_frame_buf` | `.bss` (静态全局) | 128 | 0.6% | 容纳单包组帧与 HAL 传输外发 |
| **上位机推流/调试打印缓冲** | `s_pc_print_buf` | `.bss` (静态全局) | 256 | 1.25% | VOFA+ JustFloat 格式化或串口助手打印缓冲 |
| **ADC1 DMA 双通道循环缓冲** | `s_adc_dma_buf` | `.bss` (静态全局) | 16 | 0.1% | 2 通道 × 4 次连续循环采样硬件搬运 |
| **系统状态机与运行数据结构** | `s_app_state`, `s_telemetry` | `.data / .bss` | ~512 | 2.5% | 存放 FSM 状态、按键消抖计数、IMU 滤波中间量、1s 统计量 |
| **HAL 驱动句柄与底层变量** | `hadc1`, `hi2c1`, `hspi1`, `huart1/2` | `.data / .bss` | ~256 | 1.25% | CubeMX 生成的外设句柄 |
| **【系统已分配 SRAM 合计】** | — | — | **~5008 字节** | **~24.5%** | — |
| **【系统安全剩余 SRAM 空闲】** | — | — | **~15472 字节** | **~75.5%** | **留有超过 75% 的绝对裕量，坚决杜绝内存踩踏** |

### 2.2 Flash ROM 分配预算表

* **中断向量表与启动代码**：~1.0 KB；
* **STM32Cube HAL 外设驱动库**：~12.0 KB；
* **LibDriver MPU6500 basic 驱动**：~3.5 KB；
* **SSD1306 OLED 底层与字库 (`oledfont.h`)**：~4.5 KB（6×8 与 8×16 ASCII 字模）；
* **MiaoUI 裁剪版 UI 调度器与控件**：~8.0 KB；
* **Service 服务层（协议封包、流式解包、姿态滤波、统计）**：~4.0 KB；
* **App 应用层状态机与流水线**：~3.0 KB；
* **【总计 Flash 占用预计】**：**~36 KB / 64 KB**（占用率约 56.2%，留有 28KB 余量）。

---

## 三、 传感器物理量转换与数据定点化模型

### 3.1 双轴摇杆模拟量换算模型

* **硬件特性**：片上 ADC1 为 12 位逐次逼近型 ADC，量化范围 $0 \sim 4095$，参考电压 $V_{\text{ref}} = 3300\text{ mV}$。
* **物理电压换算**：
  $$V_{\text{mv}} = \frac{\text{ADC\_Raw} \times 3300}{4095}$$
* **中位死区与归一化换算**：
  - 静态中位电压标定值记为 $V_{\text{mid}}$（理想要素为 1650mV，允许实测校准误差）；
  - 死区半宽阈值设为 $V_{\text{dead}} = 80\text{ mV}$（低于此区间判定为手松开中心静止状态）；
  - 转换方程：
    $$\text{Joy\_Mapped} = \begin{cases} 
    0 & |V_{\text{mv}} - V_{\text{mid}}| \le V_{\text{dead}} \\
    \min\left(+1000, \; \frac{V_{\text{mv}} - (V_{\text{mid}} + V_{\text{dead}})}{3300 - (V_{\text{mid}} + V_{\text{dead}})} \times 1000\right) & V_{\text{mv}} > V_{\text{mid}} + V_{\text{dead}} \\
    \max\left(-1000, \; \frac{V_{\text{mv}} - (V_{\text{mid}} - V_{\text{dead}})}{(V_{\text{mid}} - V_{\text{dead}})} \times 1000\right) & V_{\text{mv}} < V_{\text{mid}} - V_{\text{dead}}
    \end{cases}$$

### 3.2 MPU6500 原始量向物理量转换

* **陀螺仪量程配置**：$\pm 2000^\circ/\text{s}$，16 位有符号补码（-32768 ~ +32767），灵敏度为 $16.4\text{ LSB}/(^\circ/\text{s})$；
  $$\omega_{\text{dps}} = \frac{\text{Gyro\_Raw}}{16.4}$$
* **加速度计量程配置**：$\pm 8g$，16 位有符号补码，灵敏度为 $4096\text{ LSB}/g$；
  $$a_g = \frac{\text{Accel\_Raw}}{4096.0}$$
* **欧拉角定点化表示 (厘度 cd)**：
  - 为避免浮点传输带来的大小端与平台差异，姿态角统一乘以 10 转化为 `int16_t` 定点数传输：
  $$\text{Pitch\_cd} = \text{round}(\text{pitch\_deg} \times 10.0)$$
  $$\text{Roll\_cd} = \text{round}(\text{roll\_deg} \times 10.0)$$
  $$\text{Yaw\_cd} = \text{round}(\text{yaw\_deg} \times 10.0)$$

---

## 四、 固定窗口与统计数据结构模型

接收端需在 OLED 屏幕与上位机上呈现 **1 秒内通信频率 (Hz)** 与 **实时丢包率 (%)**：

### 4.1 核心数据结构

```c
typedef struct {
    uint8_t  last_seq_id;           /* 上一次收到的有效帧流水号 */
    bool     is_first_packet;       /* 首包同步标志位 */
    uint32_t window_valid_count;    /* 当前 1000ms 窗口内成功解析的有效包数 */
    uint32_t window_lost_count;     /* 当前 1000ms 窗口内检测到的丢失包数 */
    float    current_freq_hz;       /* 上一个 1s 窗口结算得到的即时频率 */
    float    current_loss_rate_pct; /* 上一个 1s 窗口结算得到的丢包率 (0.0 ~ 100.0) */
    uint32_t total_received_packets;/* 累计开机至今接收总数 */
    uint32_t total_lost_packets;    /* 累计开机至今丢失总数 */
} stats_engine_t;
```

### 4.2 丢包判定算法与频率结算

1. **环形序列号差值求丢包数**：
   $$\Delta_{\text{seq}} = (\text{Seq}_{\text{new}} - \text{Seq}_{\text{last}}) \pmod{256}$$
   - 若 $\Delta_{\text{seq}} == 1$：连续正常，丢失包数 $\text{Lost} = 0$；
   - 若 $\Delta_{\text{seq}} > 1$：发生丢包，丢失包数 $\text{Lost} = \Delta_{\text{seq}} - 1$；
   - 若 $\Delta_{\text{seq}} == 0$：发生重发包或序号回绕异常，丢失包数 $\text{Lost} = 0$；
2. **1000ms 定时窗口结算触发**：
   $$\text{current\_freq\_hz} = (\text{float})\text{window\_valid\_count}$$
   $$\text{current\_loss\_rate\_pct} = \frac{\text{window\_lost\_count}}{\text{window\_valid\_count} + \text{window\_lost\_count}} \times 100.0\%$$
   结算后将 `window_valid_count` 与 `window_lost_count` 清零，开启下一个 1 秒统计窗口。

---

## 五、 OLED 显存映射与字模数据结构

### 5.1 显存组织模型

SSD1306 控制器将 $128 \times 64$ 分辨率划分为 8 个页（Page 0 ~ Page 7），每个页高度为 8 个纵向像素：
```text
           Column 0                        Column 127
          ┌───────────────────────────────────────────┐
  Page 0  │ D0(Top) ~ D7(Bottom)                      │ (8 vertical bits)
  Page 1  │ D0(Top) ~ D7(Bottom)                      │
  ...     │ ...                                       │
  Page 7  │ D0(Top) ~ D7(Bottom)                      │
          └───────────────────────────────────────────┘
```
* **静态显存缓冲区**：`static uint8_t s_oled_gram[128][8];`
* **点阵坐标与显存映射方程**：
  对于屏幕上的任意点 $(X, Y)$（$0 \le X \le 127$，$0 \le Y \le 63$）：
  - 所在页索引：$\text{Page} = Y / 8$；
  - 所在页内的位偏移：$\text{Bit} = Y \% 8$；
  - 置 1（亮）：`s_oled_gram[X][Y / 8] |= (1 << (Y % 8));`
  - 置 0（灭）：`s_oled_gram[X][Y / 8] &= ~(1 << (Y % 8));`

### 5.2 字模点阵数据结构

* **6×8 ASCII 字模**：每个字符占用 6 字节，纵向取模，低位在上：
  `const uint8_t oled_asc2_0806[][6];`
* **8×16 ASCII 字模**：每个字符占用 16 字节（前 8 字节对应上半页，后 8 字节对应下半页）：
  `const uint8_t oled_asc2_1608[][16];`

---

## 六、 数据并发与共享安全模型 (Rule 8 对齐)

1. **中断与主循环共享变量**：
   * 串口中断向 `srv_ring_buffer` 写入数据，主循环从中弹出数据；
   * `head` 与 `tail` 索引变量声明必须增加 `volatile` 修饰：
     ```c
     typedef struct {
         uint8_t          *p_buffer;
         uint16_t          size;
         volatile uint16_t head;
         volatile uint16_t tail;
     } ring_buffer_t;
     ```
2. **临界区读取防护**：
   * 在主循环读取 32 位累计包数（如 `total_received_packets`）或多字节姿态数据时，由于 32 位访问在 32 位 Cortex-M3 架构上为单周期原子操作，无需全局关中断；但复合结构体（如 `telemetry_payload_t`）拷贝时，采用双缓冲区机制或短临界区避免脏读。
