# 硬件连接与引脚分配

[文档导航](README.md) · [参数配置](hardware-configuration.md) · [实现架构](architecture.md) · [硬件验证](hardware-validation.md)

本文档整理手柄发送端与接收端的完整硬件引脚连接表、电气特性要求及有线直连调试接线。引脚分配与当前 Core、BSP 驱动及 CubeMX 配置完全一致。

---

## 一、 发送端（手柄端）硬件连接表

发送端 MCU 为 **STM32F103C8T6**（8 MHz HSE，PLL 72 MHz，3.3V 逻辑电平）。负责摇杆模拟量、独立按键、拨码开关、六轴 IMU 的采集与本地 OLED 显示，并通过透明串口与接收端进行可靠无线通信。

| 硬件模块 | 模块引脚 | MCU 引脚 | 外设 / 模式配置 | 电气特性与接线说明 | 对应驱动源码 |
|---|---|---|---|---|---|
| **双轴模拟摇杆** | VCC | 3.3V | 供电电源 | **必须接 3.3V**，与 STM32 ADC 参考基准保持一致，严禁接 5V | [bsp_joystick.c](../sender/BSP/bsp_joystick.c) |
| | GND | GND | 电源地 | 与主控板可靠共地 | |
| | VRx (X轴) | **PA0** | ADC1_IN0 (模拟输入) | 12 位 ADC，Rank 1，DMA1 Channel1 循环采集，中位电压约 1650 mV | [adc.c](../sender/Core/Src/adc.c) |
| | VRy (Y轴) | **PA1** | ADC1_IN1 (模拟输入) | 12 位 ADC，Rank 2，DMA1 Channel1 循环采集，中位电压约 1650 mV | |
| **独立轻触按键**<br>(4个独立按键) | KEY1 | **PB0** | GPIO_Input (内部上拉) | 菜单“上移/切换”。按键另一脚接 **GND**，按下为低电平（掩码 bit 0） | [bsp_key.c](../sender/BSP/bsp_key.c)<br>[gpio.c](../sender/Core/Src/gpio.c) |
| | KEY2 | **PB1** | GPIO_Input (内部上拉) | 菜单“下移/切换”。按键另一脚接 **GND**，按下为低电平（掩码 bit 1） | |
| | KEY3 | **PB10** | GPIO_Input (内部上拉) | 菜单“确认/校准”。按键另一脚接 **GND**，按下为低电平（掩码 bit 2） | |
| | KEY4 | **PB11** | GPIO_Input (内部上拉) | 菜单“返回目录”。按键另一脚接 **GND**，按下为低电平（掩码 bit 3） | |
| **拨码开关**<br>(2位开关) | SW1 | **PB12** | GPIO_Input (内部上拉) | 开关另一脚接 **GND**。悬空/断开为高电平(1)，接通闭合为低电平(0)，掩码 bit 0 | [bsp_switch.c](../sender/BSP/bsp_switch.c) |
| | SW2 | **PB13** | GPIO_Input (内部上拉) | 开关另一脚接 **GND**。悬空/断开为高电平(1)，接通闭合为低电平(0)，掩码 bit 1 | |
| **六轴 IMU 模块**<br>(MPU6500) | VCC | 3.3V | 供电电源 | 3.3V 供电 | [bsp_imu.c](../sender/BSP/bsp_imu.c)<br>[spi.c](../sender/Core/Src/spi.c) |
| | GND | GND | 电源地 | 与主控板共地 | |
| | CS / NSS | **PA4** | GPIO_Output (推挽输出) | 软件片选控制，空闲高电平，片选低电平 | |
| | SCL / SCK | **PA5** | SPI1_SCK (复用推挽) | SPI Mode 0 (CPOL=0, CPHA=0)，运行时由 BSP 设为 /128 分频 (562.5 kHz) | |
| | SDO / MISO | **PA6** | SPI1_MISO (浮空输入) | 主机读入数据。部分模块标注为 AD0/SDO | |
| | SDA / MOSI | **PA7** | SPI1_MOSI (复用推挽) | 主机写入数据。部分模块标注为 SDA/SDI | |
| **0.96寸 OLED**<br>(SSD1306, 128×64) | VCC | 3.3V / 5V | 供电电源 | 推荐接 3.3V（视模块板载稳压芯片特性而定） | [bsp_oled.c](../sender/BSP/bsp_oled.c)<br>[i2c.c](../sender/Core/Src/i2c.c) |
| | GND | GND | 电源地 | 与主控板共地 | |
| | SCL | **PB6** | I2C1_SCL (复用开漏) | 硬件 I2C1 Fast Mode 400 kHz，7 位地址 0x3C (HAL 0x78) | |
| | SDA | **PB7** | I2C1_SDA (复用开漏) | 硬件 I2C1 Fast Mode 400 kHz | |
| **无线串口透传模块**<br>(双向遥测/ACK) | VCC | 3.3V / 5V | 供电电源 | 依无线透传模块规格供电 | [bsp_usart.c](../sender/BSP/bsp_usart.c)<br>[usart.c](../sender/Core/Src/usart.c) |
| | GND | GND | 电源地 | 与主控板共地 | |
| | RXD | **PA9** | USART1_TX (复用推挽) | 115200 8N1，向无线模块发送 23 字节遥测数据帧 | |
| | TXD | **PA10** | USART1_RX (浮空输入) | 115200 8N1，接收来自对端的 6 字节 ACK 响应帧 | |
| **USB 转 TTL 模块**<br>(手柄端调试串口) | GND | GND | 电源地 | 必须共地 | [usart.c](../sender/Core/Src/usart.c) |
| | RXD | **PA2** | USART2_TX (复用推挽) | 115200 8N1，向 PC 输出约 10 Hz 文本采样日志与按键事件 | |
| | TXD | **PA3** | USART2_RX (浮空输入) | 115200 8N1，调试接收口（保留备用） | |
| **SWD 调试下载** | SWDIO | **PA13** | SYS_JTMS-SWDIO | 烧录与调试口（考核正式评测期间禁止连线） | |
| | SWCLK | **PA14** | SYS_JTCK-SWCLK | 烧录与调试口（考核正式评测期间禁止连线） | |

---

## 二、 接收端硬件连接表

接收端 MCU 同样为 **STM32F103C8T6**。负责接收无线遥测包并回复 ACK、驱动 OLED 显示实时姿态与通信统计指标（频率/丢包率），并通过独立串口向上位机转发数据。

| 硬件模块 | 模块引脚 | MCU 引脚 | 外设 / 模式配置 | 电气特性与接线说明 | 对应驱动源码 |
|---|---|---|---|---|---|
| **0.96寸 OLED**<br>(SSD1306, 128×64) | VCC | 3.3V / 5V | 供电电源 | 推荐接 3.3V | [bsp_oled.c](../receiver/BSP/bsp_oled.c)<br>[i2c.c](../receiver/Core/Src/i2c.c) |
| | GND | GND | 电源地 | 与主控板共地 | |
| | SCL | **PB6** | I2C1_SCL (复用开漏) | 硬件 I2C1 Fast Mode 400 kHz，7 位地址 0x3C (HAL 0x78) | |
| | SDA | **PB7** | I2C1_SDA (复用开漏) | 硬件 I2C1 Fast Mode 400 kHz | |
| **无线串口透传模块**<br>(双向遥测/ACK) | VCC | 3.3V / 5V | 供电电源 | 依无线透传模块规格供电 | [bsp_usart.c](../receiver/BSP/bsp_usart.c)<br>[usart.c](../receiver/Core/Src/usart.c) |
| | GND | GND | 电源地 | 与主控板共地 | |
| | RXD | **PA9** | USART1_TX (复用推挽) | 115200 8N1，向无线模块发送 6 字节 ACK 响应帧 | |
| | TXD | **PA10** | USART1_RX (浮空输入) | 115200 8N1，接收来自手柄端的 23 字节遥测数据帧 | |
| **USB 转 TTL 模块**<br>(上位机 / VOFA+) | GND | GND | 电源地 | 必须共地 | [srv_pc.h](../receiver/Service/srv_pc.h)<br>[usart.c](../receiver/Core/Src/usart.c) |
| | RXD | **PA2** | USART2_TX (复用推挽) | 115200 8N1，默认输出 14 通道 JustFloat（可切为文本 R/STAT） | |
| | TXD | **PA3** | USART2_RX (浮空输入) | 115200 8N1，接收模式切换指令（ASCII `T` 文本 / `F` JustFloat） | |
| **SWD 调试下载** | SWDIO | **PA13** | SYS_JTMS-SWDIO | 烧录与调试口（考核正式评测期间禁止连线） | |
| | SWCLK | **PA14** | SYS_JTCK-SWCLK | 烧录与调试口（考核正式评测期间禁止连线） | |

---

## 三、 双端有线直连调试接线（备用验证通道）

评测或日常联调中，若无线透传模块信号异常、参数未配置或受干扰，考核规则允许临时改用杜邦线直连两端 MCU 的串口进行辅助验证。接线必须交叉并严格共地：

| 发送端 (Sender) 引脚 | 信号流向 | 接收端 (Receiver) 引脚 | 功能说明 |
|---|:---:|---|---|
| **PA9** (USART1_TX) | ───→ | **PA10** (USART1_RX) | 手柄遥测数据帧传输至接收端 |
| **PA10** (USART1_RX) | ←─── | **PA9** (USART1_TX) | 接收端 ACK 确认应答帧回传给手柄端 |
| **GND** | ═════ | **GND** | **两端必须共地**（消除地电位差，防止通信乱码与丢包） |

---

## 四、 关键电气特性与接线注意事项

1. **严格全系统共地（Common Ground）**：
   - 面包板搭接时，电池降压输出地、STM32 最小系统板地、传感器模块地、OLED 地以及 USB-TTL 调试器地线必须全通路互通。
2. **摇杆供电基准与 ADC 测量匹配**：
   - 摇杆电位器两端接 3.3V 与 GND。若误接 5V，会导致电位器滑臂输出电压超出 STM32 模拟输入口（PA0/PA1）的 3.3V 额定电压，不仅会造成 ADC 读数长时间顶部饱和（截断在 4095/3300 mV），更可能损坏芯片引脚。
3. **按键与拨码开关输入极性**：
   - 内部 GPIO 均已配置为带弱上拉输入（`GPIO_PULLUP`）。
   - 按键（KEY1~KEY4）与拨码开关（SW1~SW2）的另一端引脚必须统一连至 **GND**。
   - 按下/导通时引脚被拉低至 0V（低电平）；松开/断开时被内部上拉至 3.3V（高电平）。
4. **MPU6500 SPI 总线合规性**：
   - 考核规则明确禁止使用软件模拟 SPI/I2C，必须使用硬件外设。
   - 驱动要求必须为 SPI 接口通信（Mode 0，CPOL=0，CPHA=0），当前 BSP 已在运行时自动重设波特率分频为 `/128`（562.5 kHz，符合寄存器访问限制）。
   - 若模块排针带 AD0/SDO，必须接 PA6 (MISO)；CS 必须接 PA4。
5. **I2C OLED 屏幕驱动**：
   - 使用 STM32 硬件 I2C1 快速模式（400 kHz），引脚配置为复用开漏（AF Open-Drain）。
   - 模块底板通常已自带 4.7 kΩ 上拉电阻至 VCC；若波形上升沿过缓导致偶发 I2C 锁死，需检查上拉电压是否稳定。
6. **串口信号线交叉连接（TX ↔ RX）**：
   - 单片机的 TXD 接外接设备（无线模块或 USB-TTL）的 RXD；
   - 单片机的 RXD 接外接设备的 TXD。
   - USART1 专用于机间通信（双端双向带 ACK），USART2 专用于上位机/PC 观察。两组串口物理完全独立，评测时需同时接出。
