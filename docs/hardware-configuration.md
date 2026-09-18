# 硬件映射与软件配置

[文档导航](README.md) · [硬件连接](hardware-connection.md) · [实现架构](architecture.md) · [硬件验证](hardware-validation.md)

配置来源为当前 Core、BSP 和 Service。表中默认值描述软件设置，不表示硬件已测量达标。详细的模块引脚物理接线、电气特性与有线直连调试说明见[硬件连接表](hardware-connection.md)。

## 引脚与器件

两端 MCU 均为 STM32F103C8T6，8 MHz HSE、PLL 到 72 MHz，64 KB Flash、20 KB SRAM。完整模块引脚连线图表可直接查阅[硬件引脚连接表](hardware-connection.md)。

| 功能 | 端 | 外设/引脚 | 当前配置 |
|---|---|---|---|
| 摇杆 X/Y | 发送端 | ADC1 PA0/PA1 | 扫描 Rank 1/2，12 位，DMA1 Channel1 循环 |
| KEY1/2/3/4 | 发送端 | PB0/PB1/PB10/PB11 | 上拉，按下接地，掩码 1 表示按下 |
| SW1/2 | 发送端 | PB12/PB13 | 上拉，高 1、低 0；变化稳定 60 ms 发布 |
| MPU6500 | 发送端 | SPI1 PA5/PA6/PA7，CS PA4 | Mode 0，运行时 /128，562.5 kHz |
| SSD1306 | 两端 | I2C1 PB6/PB7 | 硬件 I2C，400 kHz，128×64；探测 7 位地址 0x3C/0x3D |
| 无线远程串口 | 两端 | USART1 PA9 TX / PA10 RX | 115200，8N1，双向遥测/ACK |
| 调试 / PC | 两端 | USART2 PA2 TX / PA3 RX | 115200，8N1；手柄文本，接收端默认 JustFloat |
| 调试下载 | 两端 | SWD PA13/PA14 | 保留原配置 |

SSD1306 的 HAL 地址参数使用左移后的 0x78/0x7A，不要与 7 位地址混淆。MPU6500 WHO_AM_I 期望 0x70，量程 ±8g / ±2000 dps，采样 50 Hz、低通约 20 Hz，不启用 DMP。

## CubeMX 生成值与运行值

| 项目 | 生成配置/原因 | 运行时处理与源码 |
|---|---|---|
| SPI1 分频 | 原 .ioc /8=9 MHz 不适合一般寄存器访问 | [bsp_imu.c](../sender/BSP/bsp_imu.c) 首次访问前覆盖 /128 并初始化 |
| 中断优先级 | 原 CubeMX 优先级与业务恢复所需配置不同 | [bsp_usart.c](../sender/BSP/bsp_usart.c)：SysTick 0、USART1 1、USART2 3；ADC BSP 设置 DMA 2 |
| 接收端 USART2 IRQ | 原接收工程未开启此 IRQ | BSP 启用；[中断文件](../receiver/Core/Src/stm32f1xx_it.c) USER CODE 区补处理函数 |

不能只看 .ioc 推断所有运行参数。Core 自定义改动只在 USER CODE 区；修改/重新生成工程后应确认上述覆盖与中断仍存在，避免重复定义 handler。

## 可调参数

双方 [system_config.h](../sender/Service/system_config.h) 与[接收端副本](../receiver/Service/system_config.h) 应保持一致。下表是当前值，调整前先评估依赖。

| 参数 | 默认值 | 含义/调整影响 |
|---|---:|---|
| TELEMETRY_PERIOD_MS | 20 ms | 采样/尝试发新遥测；不能单靠缩短它提高停止等待吞吐 |
| LINK_ACK_TIMEOUT_MS | 80 ms | ACK 等待阈值，应结合真实往返延迟 |
| LINK_MAX_RETRIES | 2 | 最大重传次数，连同 ACK 阈值确定样本等待上限 |
| LINK_OFFLINE_MS | 1000 ms | 接收离线和统计基准失效条件 |
| LINK_STARTUP_QUIET_MS | 1500 ms | 发送启动静默，需覆盖对端离线阈值与实际缓存延迟 |
| PARSER_GAP_MS | 100 ms | 半帧消费间隔超时 |
| KEY_DEBOUNCE_MS | 20 ms | 按键电平稳定判定 |
| KEY_LONG_MS | 2000 ms | 长按阈值 |
| KEY_DOUBLE_MS | 300 ms | 第二次按下相对第一次释放的窗口 |
| IMU_CAL_SAMPLES | 100 | 静止校准合格样本数 |
| IMU_GYRO_LSB_PER_DPS | 16.4 | 必须与陀螺量程寄存器匹配 |
| IMU_ACCEL_LSB_PER_G | 4096 | 必须与加速度量程寄存器匹配 |
| IMU_CORRECTION_KP | 2.0 | 重力校正增益，应结合实物噪声与响应调节 |
| JOYSTICK_VREF_MV | 3300 mV | 电压换算参考；当前接收端合法范围仍固定为 ≤3300 mV |
| JOYSTICK_DEAD_MV | 80 mV | 中心两侧死区 |
| IMU_BODY_X/Y/Z | +1 / +2 / +3 | 带符号轴索引；编译期检查互异且为右手坐标系 |

部分数值直接由具体模块维护：ADC 256 半字循环缓冲、UART 256 B RX/TX、开关 60 ms、串口 TX 看门狗 100 ms、OLED 单事务 4 ms、SPI 单事务 2 ms。不要误以为所有阈值都在 system_config.h。

改变 IMU 量程时，应同时改寄存器、换算常量和测试；改电压范围需同步映射、接收端载荷检查、协议说明；改协议或通道需同步双方与 PC。默认参数的实测校准项目见[硬件验证清单](hardware-validation.md)。
