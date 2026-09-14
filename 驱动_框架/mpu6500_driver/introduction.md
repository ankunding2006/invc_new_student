Listed directory mpu6500
Viewed README_zh-Hans.md:1-150
Listed directory src
Listed directory interface
Listed directory example
Listed directory test
Listed directory project
Listed directory datasheet
Listed directory stm32f407
Viewed driver_mpu6500_interface.h:1-160
Viewed driver_mpu6500.h:50-200
Searched for "@defgroup|@addtogroup"
Searched for "group mpu6500_"
Searched for "addtogroup"
Searched for "addtogroup"
Viewed driver_mpu6500.h:1-60
Searched for "@(defgroup|addtogroup)"
Searched for "@(defgroup|addtogroup)"
Searched for "@defgroup"
Viewed driver_mpu6500.h:440-540
Viewed driver_mpu6500_basic.h:1-137
Viewed driver_mpu6500_dmp.h:1-180
Viewed driver_mpu6500_fifo.h:1-140
Viewed driver_mpu6500_code.h:1-70
Listed directory misra
Listed directory doc
Listed directory raspberrypi4b
Viewed README.md:1-120

该项目是 **LibDriver MPU6500**，是由开源驱动生态 [LibDriver](https://www.libdriver.com) 维护的一个**工业级、跨平台、全功能、符合 MISRA 规范的 MPU6500 六轴惯性传感器 C 语言驱动库**。

---

### 一、 项目的核心目的与设计理念

1. **核心目的**：
   - 为 InvenSense / TDK 的 **MPU6500** 芯片（包含 3 轴加速度计、3 轴陀螺仪以及内置硬件数字运动处理器 DMP）提供一个**开箱即用、工业标准、全功能覆盖**的底层驱动实现。
   - 彻底解决传统嵌入式驱动与特定 MCU 硬件强耦合的问题，让开发者能够在不同的单片机（如 STM32、ESP32、GD32、AVR）或 Linux 开发板（如树莓派）之间**零成本无缝迁移**。

2. **设计理念**：
   - **硬件抽象解耦（HAL 隔离）**：驱动核心逻辑只依赖纯 C 语言和标准的抽象接口函数指针，完全不绑定具体硬件外设库。
   - **高可靠性与代码规范**：代码严格遵循 **MISRA C** 规范，并通过 Polyspace 静态分析报告验证，适用于汽车电子、无人机、机器人等高可靠性场景。
   - **分层设计**：从底层硬件读写、中间完整寄存器封装，到上层常用业务抽象（Basic/FIFO/DMP）、自动化测试，层级分明。

---

### 二、 项目目录结构详解

项目的目录组织非常标准和清晰，呈现清晰的嵌入式驱动工程规范：

```text
mpu6500/
├── datasheet/                  # 官方芯片数据手册与寄存器映射表
│   ├── mpu6500_product_specification.pdf
│   └── mpu6500_register_map.pdf
├── doc/                        # 文档与 API 说明（支持 Doxygen 生成）
├── interface/                  # 硬件平台抽象接口层（移植对接层）
│   ├── driver_mpu6500_interface.h          # 硬件抽象接口声明
│   └── driver_mpu6500_interface_template.c # 平台移植空模板
├── src/                        # 驱动核心源码（纯 C 语言，平台无关）
│   ├── driver_mpu6500.h        # 核心头文件（寄存器、枚举、handle、全功能 API）
│   ├── driver_mpu6500.c        # 核心逻辑实现（驱动主体，超 1.5 万行）
│   └── driver_mpu6500_code.h   # 片上 DMP 固件微码数组（3062 字节二进制固件）
├── example/                    # 上层业务封装范例（开箱即用 API）
│   ├── driver_mpu6500_basic.c/.h   # 基础采集范例（加速度/角速度/温度）
│   ├── driver_mpu6500_fifo.c/.h    # FIFO 批量突发读取范例
│   └── driver_mpu6500_dmp.c/.h     # DMP 姿态解算/计步/轻敲/翻转范例
├── test/                       # 驱动单元测试与功能验证套件
│   ├── driver_mpu6500_register_test.c/.h       # 寄存器读写边界测试
│   ├── driver_mpu6500_read_test.c/.h           # 基础读数功能测试
│   ├── driver_mpu6500_fifo_test.c/.h           # FIFO 缓存模式测试
│   ├── driver_mpu6500_dmp_read_test.c/.h       # DMP 姿态角四元数解算测试
│   ├── driver_mpu6500_dmp_pedometer_test.c/.h  # DMP 计步器测试
│   └── driver_mpu6500_dmp_tap_orient_motion_test.c/.h # 敲击/朝向/运动检测测试
├── project/                    # 具体硬件平台的实际参考工程（包含 Shell 命令行交互）
│   ├── stm32f407/              # STM32F407 开发板工程（支持 Keil MDK 与 IAR EWARM）
│   └── raspberrypi4b/          # 树莓派 4B Linux 工程（支持 CMake / Makefile 编译）
├── misra/                      # MISRA 代码合规性检查结果及扫描报告
│   └── mpu6500_polyspace_report.pdf
├── Doxyfile                    # Doxygen 文档构建配置
└── README_zh-Hans.md / README.md # 多语言说明自述文件
```

#### 各目录关键作用说明：
- **`src/`**：驱动内核。开发者一般不需要修改此目录的代码。[driver_mpu6500.c](file:///c:/Users/34118/Desktop/WorkSpace/mpu6500/src/driver_mpu6500.c) 实现了对 MPU6500 硬件寄存器的全部配置与数据转换，[driver_mpu6500_code.h](file:///c:/Users/34118/Desktop/WorkSpace/mpu6500/src/driver_mpu6500_code.h) 内嵌了加载进传感器内部执行姿态融合的 DMP 微码。
- **`interface/`**：移植层。包含 [driver_mpu6500_interface_template.c](file:///c:/Users/34118/Desktop/WorkSpace/mpu6500/interface/driver_mpu6500_interface_template.c)，用户在实际项目中只需在此文件中填入自己平台的硬件 IIC/SPI 读写、毫秒延时、打印输出等函数。
- **`example/`**：高阶应用封装。将繁琐的寄存器配置流程简化为 `mpu6500_basic_init()`、`mpu6500_dmp_init()` 等一键初始化和读取函数，极大地降低了二次开发门槛。
- **`test/`**：用于自动化校验或出厂校准，验证移植后的硬件通信时序和芯片各项硬件特性是否正常。
- **`project/`**：可以直接编译运行的示范工程，自带基于串口的 CLI 调试 Shell，可通过串口发送指令（如 `mpu6500 -t dmp`、`mpu6500 -e read`）快速验证芯片。

---

### 三、 驱动提供的核心功能

该驱动实现了 MPU6500 芯片的**全功能覆盖**，主要包括以下模块：

#### 1. 基础数据采集与单位换算 (Basic Sensing)
- **3 轴加速度读取**：支持 $\pm 2g, \pm 4g, \pm 8g, \pm 16g$ 量程配置，自动将 16 位 ADC 原始计数值（Raw Data）换算为标准重力加速度工程单位 $g$。
- **3 轴角速度读取**：支持 $\pm 250dps, \pm 500dps, \pm 1000dps, \pm 2000dps$ 量程配置，自动换算为角速度单位 $dps$（度/秒）。
- **片上温度采集**：内置 16 位 ADC 温度传感器读取与线性摄氏度（$^\circ\text{C}$）换算。

#### 2. 数字运动处理器 (DMP - Digital Motion Processor) 硬件姿态融合
这是该驱动最强大的特性之一，无需主控 MCU 运行繁重的卡尔曼滤波或 Mahony/Madgwick 姿态解算算法：
- **片上四元数融合输出**：DMP 硬件引擎直接输出 6 轴融合四元数（Quaternion），驱动层支持直接计算出物体的**俯仰角（Pitch）、横滚角（Roll）和偏航角（Yaw）**。
- **硬件计步器 (Pedometer)**：由 DMP 在传感器内部进行步数统计和运动步频/行走时间计算，支持步数中断上报。
- **敲击/轻触检测 (Tap Detection)**：支持 X/Y/Z 轴阈值配置，识别单击、双击动作及撞击方向。
- **空间朝向识别 (Orientation)**：自动检测设备的放置方向（类似手机屏幕在肖像/风景等 8 个空间朝向的旋转事件）。
- **抖动过滤与震颤抑制 (Shake Reject)**：过滤机械振动带来的误触发。

#### 3. 硬件 FIFO 突发缓存机制
- 支持 512 字节硬件 FIFO 缓冲机制。
- 可将加速度、陀螺仪、温度、从机传感器数据按需推入 FIFO。
- 主控 MCU 可以低频睡眠，在 FIFO 达到水位线时通过中断突发（Burst Read）批量读取数据，极大降低系统功耗和总线占用。

#### 4. 电源管理与运动唤醒 (Wake-on-Motion / Low Power)
- **低功耗加速度计模式**：支持 0.24Hz 至 500Hz 低频循环采样。
- **运动唤醒 (WOM / Motion Interrupt)**：静止时芯片与 MCU 同时进入极低功耗休眠，当检测到振动或位移超过设定阈值时产生硬件中断信号唤醒 MCU。
- 支持陀螺仪待机（Standby）模式与芯片全断电休眠模式。

#### 5. 信号调理、自检与校准
- **低通滤波器 (DLPF)**：针对加速度计与陀螺仪分别支持独立的数字低通滤波频率配置（1kHz/8kHz/32kHz 基础采样下的滤波档位）。
- **硬件自检 (Self-Test)**：提供内置自检模式，自动比对电气输出波动，判断传感器硬件结构是否损坏。
- **偏置校准 (Offset Calibration)**：支持工厂/静态零偏写入与补偿寄存器调节。

#### 6. 接口协议与扩展支持
- **双总线兼容**：一套 API 同时支持 **I2C 总线**（最高 400kHz，支持 AD0 引脚选择高低地址 `0xD0`/`0xD2`）和 **SPI 总线**（最高 20MHz）。
- **辅助 I2C 主机模式 (Auxiliary I2C Master)**：MPU6500 可作为二级 I2C 主机挂载外部 3 轴磁力计（如 AK8963 等组合为 9 轴）或气压计，支持直通旁路（Bypass）模式与从机自动轮询模式。
- **完善的中断管理**：支持数据就绪中断、FIFO 溢出中断、运动检测中断、DMP 中断，中断引脚电平、推挽/开漏均可灵活配置。

---

### 四、 驱动架构的设计模式

该驱动采用了清晰的 **C 语言面向对象设计（句柄模式 Handle Pattern）**：

1. **句柄结构体 (`mpu6500_handle_t`)**：
   在 [driver_mpu6500.h](file:///c:/Users/34118/Desktop/WorkSpace/mpu6500/src/driver_mpu6500.h#L451-L473) 中定义了 `mpu6500_handle_t`，其中包含了所有总线通信、延时、调试输出、中断回调的函数指针以及运行状态变量。
2. **连接宏 (`DRIVER_MPU6500_LINK_*`)**：
   通过类似于宏注入的方式将平台实现与核心驱动绑定：
   ```c
   DRIVER_MPU6500_LINK_INIT(&gs_handle, mpu6500_handle_t);
   DRIVER_MPU6500_LINK_IIC_INIT(&gs_handle, mpu6500_interface_iic_init);
   DRIVER_MPU6500_LINK_IIC_READ(&gs_handle, mpu6500_interface_iic_read);
   DRIVER_MPU6500_LINK_IIC_WRITE(&gs_handle, mpu6500_interface_iic_write);
   DRIVER_MPU6500_LINK_DELAY_MS(&gs_handle, mpu6500_interface_delay_ms);
   DRIVER_MPU6500_LINK_DEBUG_PRINT(&gs_handle, mpu6500_interface_debug_print);
   ```
3. **极简的使用流程**：
   若使用封装好的 [example](file:///c:/Users/34118/Desktop/WorkSpace/mpu6500/example)，只需三步即可获取姿态：
   - 步骤 1：填充 [driver_mpu6500_interface.c](file:///c:/Users/34118/Desktop/WorkSpace/mpu6500/interface/driver_mpu6500_interface_template.c) 中的总线读写接口。
   - 步骤 2：调用 `mpu6500_dmp_init(...)` 完成芯片初始化和 DMP 固件载入。
   - 步骤 3：周期调用 `mpu6500_dmp_read_all(...)` 直接获取四元数与 Pitch/Roll/Yaw 角度。