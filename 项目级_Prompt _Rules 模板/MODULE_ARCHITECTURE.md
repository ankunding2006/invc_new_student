# MODULE_ARCHITECTURE.md — 软件分层架构与模块划分设计说明书 (Step 5 & 6)

> **工序定位**：15步引导式开发流程的 **⑤ 软件架构设计 (BSP / Driver / Service / App)** 与 **⑥ 模块划分 (功能模块与职责解耦)**。  
> **核心使命**：在已确立的工程需求（REQUIREMENTS.md）与系统行为状态机（SYSTEM_BEHAVIOR.md）指导下，完成系统的纵向分层架构规划与横向功能模块解耦，明确每个模块的层级归属、职责边界与依赖关系。  
> **重要规则**：**当前阶段仅在系统提示词模板中确立设计理念与模块边界，严禁在工程中私自提前创建源文件或编写代码**。

---

## 一、 软件分层架构设计理念 (Layered Architectural Principles)

为满足考核对“代码工程规范（硬件抽象层/应用层分离）、高可移植性与模块化”的核心要求，两端软件系统统一采用经典嵌入式 **四层分层架构 (Four-Layer Architecture)**：

```text
┌──────────────────────────────────────────────────────────────────────────┐
│                         Layer 4: 应用层 (Application)                    │
│      负责系统顶层业务逻辑编排、系统生命周期状态机、人机交互与协同分发    │
├──────────────────────────────────────────────────────────────────────────┤
│                     Layer 3: 核心服务/组件层 (Service / Middleware)       │
│      与硬件芯片 100% 解耦的纯逻辑算法、数据结构、协议编解码与统计算法     │
├──────────────────────────────────────────────────────────────────────────┤
│                    Layer 2: 板级硬件抽象驱动层 (BSP / Driver)            │
│      屏蔽具体器件电气特性与寄存器细节，向上提供器件级标准控制抽象        │
├──────────────────────────────────────────────────────────────────────────┤
│                       Layer 1: 芯片底层与HAL库层 (MCU / HAL)             │
│      STM32CubeMX 自动生成的芯片初始化、寄存器配置与底层硬件中断入口      │
└──────────────────────────────────────────────────────────────────────────┘
```

### 分层设计的核心工程价值：
1. **极高的跨平台可移植性**：
   * **Layer 3 (Service)** 采用纯 ANSI C 编写，**严禁包含 `stm32f1xx_hal.h`**。即使未来主控芯片从 STM32F103 更换为 STM32F407、H743 或其他厂商芯片，Service 层（协议引擎、姿态滤波、环形缓冲、丢包统计算法）**代码一行不用改**。
   * **Layer 2 (BSP)** 专职适配芯片外设接口，换芯片时仅需重新实现 BSP 层与 HAL 库的对接，保护上层投资。
2. **单向依赖与解耦红线 (Strict Dependency Rules)**：
   * **上层可调用下层，下层绝不允许依赖上层**（禁止下层 `#include` 上层头文件）；
   * 严禁跳层破坏封装（如应用层直接操作芯片底层寄存器）；
   * 下层若需向上层传递异步事件，必须通过“状态机轮询返回”或“回调函数指针 (Callback)”实现松耦合。
3. **数据流与控制流分离**：
   * 中断只做数据中转，通过环形缓冲区解耦；
   * 业务调度基于非阻塞主循环时间戳驱动，避免阻塞等待蔓延。

---

## 二、 手柄发送端 (Transmitter) 模块划分与职责定义

手柄端按纵向分层拆解为以下功能明确、高内聚低耦合的独立模块：

```text
【手柄发送端 Transmitter】

[Layer 4: Application]
├── app_sender          (手柄系统主控协调器：生命周期状态机、外发调度、故障降级)
└── app_menu            (多级 OLED 菜单交互器：参考 MiaoUI 框架二次开发，管理 4 大核心页面状态机、按键导航、Q 弹平滑动画、屏幕渲染分发)

[Layer 3: Service / Middleware] (纯 C，无硬件依赖)
├── srv_protocol        (协议组帧引擎：遥测数据结构序列化、打包校验)
├── srv_imu_filter      (姿态滤波解算引擎：陀螺仪零偏校准、互补/Mahony 滤波、欧拉角解算；【严禁使用 DMP】)
└── srv_ring_buffer     (通用环形缓冲区：无锁字节队列，用于串口收发缓冲)

[Layer 2: BSP / Driver]
├── bsp_joystick        (双轴摇杆驱动：ADC DMA 缓冲读取、零偏校准、死区过滤、物理电压与映射计算)
├── bsp_key             (按键驱动：4 路独立按键消抖、四事件 [按下/释放/长按2s/双击] 状态机)
├── bsp_switch          (拨码开关驱动：2 位拨码稳态高低电平读取)
├── bsp_imu             (六轴 IMU 硬件驱动：基于 LibDriver MPU6500 basic 模式，SPI1 PA4~PA7 通信，读取原始 16 位 Accel/Gyro 数据)
├── bsp_oled            (OLED 显示驱动：硬件 I2C1 PB6/PB7 通信，参考 0.96 OLED 例程寄存器时序与字库，1KB 显存管理与点阵绘制)
└── bsp_usart           (双串口驱动抽象：USART1 无线非阻塞发送接口、USART2 PC 调试日志接口)

[Layer 1: MCU / HAL]
└── CubeMX Core         (main.c, gpio.c, adc.c, spi.c, i2c.c, usart.c, dma.c, stm32f1xx_it.c)
```

### 手柄端各模块职责矩阵表：

| 模块名称 | 所属层级 | 核心职责 (Responsibilities) | 参考框架/驱动 (`驱动_框架/`) | 输入依赖 (Inputs) | 输出提供 (Outputs) |
|---|---|---|---|---|---|
| **`app_sender`** | Application | 手柄生命周期管理；周期调用采集与滤波；组装遥测包；触发无线发送与串口日志。 | 自主设计调度引擎 | 各服务与驱动状态 | 顶层协同控制流 |
| **`app_menu`** | Application | 管理 4 大核心页面状态机；响应按键导航事件；调度 OLED 绘制当前界面内容。 | 参考 `OLED_display/MiaoUI`（双向链表、页面树、非线性平滑动画、超长标题跑马灯） | 按键事件、传感器数据 | OLED 显存渲染请求 |
| **`srv_protocol`** | Service | 纯逻辑数据帧组包；填充帧头、自增流水号、CMD、载荷与校验和/CRC。 | 自主协议引擎 | 遥测原始结构体 | 连续二进制字节帧 |
| **`srv_imu_filter`** | Service | 纯数学姿态解算算法；处理零偏校准、加速度与角速度数据融合；输出欧拉角。**【红线：纯软件解算，严禁使用 DMP】** | 纯数学算法（互补滤波/Mahony） | 原始三轴 Accel/Gyro | Pitch, Roll, Yaw 欧拉角 |
| **`srv_ring_buffer`**| Service | 通用轻量环形字节队列；提供 push、pop、peek、数据长度查询。 | 自主通用环形队列 | 原始字节流 | 缓冲数据块 |
| **`bsp_joystick`** | BSP | 操作 ADC1 DMA 内存；执行中位校准、死区滤波；计算归一化值 (-1000~1000) 与真实物理电压。 | 自主 ADC-DMA 驱动 | ADC1 DMA 数组 | 摇杆物理电压与映射值 |
| **`bsp_key`** | BSP | 运行 4 路按键非阻塞有限状态机；滤除机械抖动；识别单击、释放、长按 2s 与双击。 | 自主按键 FSM 引擎 | GPIO 实时电平、Tick | 按键事件枚举 |
| **`bsp_switch`** | BSP | 采样并滤波 2 位拨码开关；输出稳定的高低电平布尔状态。 | 自主 GPIO 驱动 | GPIO 实时电平 | 开关稳态电平值 |
| **`bsp_imu`** | BSP | 硬件 SPI1 主机驱动；配置芯片寄存器；安全读取原始六轴传感器字节。**采用 basic 模式读取原始数据**。 | 参考 `mpu6500_driver/mpu6500`（LibDriver MPU6500 basic 驱动 + interface SPI 适配） | SPI1 硬件外设 | 原始 16 位整型数据 |
| **`bsp_oled`** | BSP | 硬件 I2C1 驱动；管理 1024 字节静态显存；提供清屏、画点、画线、字符串与图标绘制接口。 | 参考 `01-0.96OLED例程`（SSD1306 初始化时序 + `oledfont.h` 点阵字库），改造为硬件 I2C1 发送 | I2C1 硬件外设 | 屏幕显存刷写 |
| **`bsp_usart`** | BSP | 封装 USART1 与 USART2 硬件外设；提供非阻塞外发与环形缓冲接收接口。无线透传模块为现成硬件纯透明通道，无需无线底层驱动或 AT 指令，直接按通用硬件串口操作。 | 自主 HAL 串口封装 | USART1/2 外设 | 串口收发数据流 |

---

## 三、 接收端 (Receiver) 模块划分与职责定义

接收端紧紧围绕**“流式数据解包 $\rightarrow$ 1秒滑动窗口统计 $\rightarrow$ 双端同步呈现”**展开模块拆解：

```text
【接收端 Receiver】

[Layer 4: Application]
├── app_receiver        (接收端主控协调器：联机/失联状态机、解包调度、统计触发、转发管理)
└── app_ui              (接收端界面渲染器：负责 OLED 遥测数据、通信频率与丢包率动态排版)

[Layer 3: Service / Middleware] (纯 C，无硬件依赖)
├── srv_protocol_parser (流式解包有限状态机：逐字节消费、抗粘包/断包、抗噪声再同步、校验比对)
├── srv_stats           (通信统计引擎：维护 1s 滑动窗口，计算通信频率 Hz 与序列号差值丢包率 %)
└── srv_ring_buffer     (通用环形缓冲区：暂存串口中断收到的字节流)

[Layer 2: BSP / Driver]
├── bsp_oled            (OLED 显示驱动：硬件 I2C1 通信、1KB 显存管理、字符绘制，同手柄端通用)
└── bsp_usart           (双串口驱动抽象：USART1 无线接收中断环形入队、USART2 PC 转发推流)

[Layer 1: MCU / HAL]
└── CubeMX Core         (main.c, gpio.c, i2c.c, usart.c, stm32f1xx_it.c)
```

### 接收端各模块职责矩阵表：

| 模块名称 | 所属层级 | 核心职责 (Responsibilities) | 输入依赖 (Inputs) | 输出提供 (Outputs) |
|---|---|---|---|---|
| **`app_receiver`** | Application | 接收端生命周期状态机（等待/联机/掉线）；调度流式解包；触发 1s 统计；触发 PC 转发。 | 各服务与驱动状态 | 顶层协同控制流 |
| **`app_ui`** | Application | 接收端 OLED 显示布局；排版显示摇杆值、按键状态、姿态角；显式标注 `Freq: XX Hz` 与 `Loss: X.X %`。 | 遥测数据、统计指标 | OLED 显存渲染请求 |
| **`srv_protocol_parser`**| Service | 纯逻辑单字节流式有限状态机；逐字节提取帧头、长度、CMD、载荷、校验和、帧尾；分发合法数据包。 | 环形缓冲区字节流 | 有效遥测数据包事件 |
| **`srv_stats`** | Service | 纯数学统计引擎；维护 1000ms 定时窗口；统计有效包计数得到通信频率 Hz；流水号环形差值求丢包率 %。 | 接收数据包序列号 | 频率 Hz 与丢包率 % |
| **`srv_ring_buffer`**| Service | 串口接收无锁字节环形队列；隔离高频串口中断与主循环解包。 | 串口中断接收字节 | 连续字节弹出 |
| **`bsp_oled`** | BSP | 硬件 I2C1 驱动；管理 1024 字节静态显存；提供文本与图元绘制。参考 `01-0.96OLED例程` 寄存器时序与字库，底层调用 HAL 硬件 I2C1。 | I2C1 硬件外设 | 屏幕显存刷写 |
| **`bsp_usart`** | BSP | 封装 USART1 与 USART2 硬件外设；USART1 接收中断压入环形队列，USART2 向上位机推流转发。无线透传模块为现成纯硬件通道，MCU 无需任何额外无线驱动。 | USART1/2 外设 | 转发数据流 |

---

## 四、 跨层数据交互与模块协作架构 (Collaboration & Dataflow)

### 4.1 手柄端数据流动闭环
```text
[ADC/GPIO/SPI 中断或DMA]
        │ (硬件层 Layer 1)
        ▼
[bsp_joystick / bsp_key / bsp_imu] (驱动层 Layer 2)
        │
        ▼ (提取物理量与离散事件)
[srv_imu_filter] (服务层 Layer 3: 滤波解算姿态)
        │
        ▼ (物理数据汇总)
[app_sender] (应用层 Layer 4: 顶层调度业务)
        │
        ├─────────────────────────────┐
        ▼                             ▼
[srv_protocol] (打包为二进制帧)    [app_menu] (更新菜单页面数据)
        │                             │
        ▼                             ▼
[bsp_usart (USART1/2)]        [bsp_oled] (I2C 显存刷屏)
```

### 4.2 接收端数据流动闭环
```text
[USART1 接收中断] (Layer 1)
        │
        ▼ (字节直接存入)
[srv_ring_buffer] (Layer 3: 环形缓冲)
        │
        ▼ (主循环流式弹出字节)
[srv_protocol_parser] (Layer 3: 流式状态机拆包与校验)
        │ (产出有效帧)
        ▼
[app_receiver] (Layer 4: 主控分发)
        │
        ├─────────────────────────────┬─────────────────────────────┐
        ▼                             ▼                             ▼
[srv_stats] (1秒统计)          [bsp_usart (USART2)]          [app_ui] (界面排版)
(更新 Freq Hz 与 丢包率 %)     (即收即转推流至电脑上位机)     │
        │                                                   ▼
        └─────────────────────────────────────────────► [bsp_oled] (刷新屏幕)
```

---

## 五、 模块接口规范与跨层协议设计

* **状态标记**：**已完成模块接口设计 (Step 7 已完成)**。
* **详细接口设计规范书**：所有模块的详细数据结构体、枚举、宏定义与对外暴露的 API 函数原型已落地沉淀于系统级规范文件 [`MODULE_INTERFACES.md`](file:///c:/Users/34118/Desktop/invc_exam/project/项目级_Prompt%20_Rules%20模板/MODULE_INTERFACES.md)。
* **执行准则**：代码实现必须严格遵循 `MODULE_INTERFACES.md` 中的参数类型与返回值约束，严禁随意篡改函数签名破坏跨模块契约。

---

## 六、 第三方驱动与开源框架对接架构映射表 (Driver & Framework Mapping)

项目根目录下 `驱动_框架/` 中的开源资源与系统架构的对应与落地规则如下：

| 开源框架 / 参考资产 | 存放路径 | 归属架构层级 | 目标模块 | 适配与落地策略 | 禁令与排他红线 |
|---|---|---|---|---|---|
| **LibDriver MPU6500** | `驱动_框架/mpu6500_driver/mpu6500/` | **Layer 2 (BSP)** | `bsp_imu` | 1. 保留核心驱动文件 `driver_mpu6500.c/.h`；<br>2. 采用 `driver_mpu6500_basic.c/.h` 作为业务接入；<br>3. 在 `driver_mpu6500_interface.c` 中绑定硬件 SPI1 (PA4~PA7) 读写及 SysTick 延时。 | **【绝对禁令】严禁使用 DMP！** 禁止编译或调用 `driver_mpu6500_dmp.c/.h`，姿态必须交由 `srv_imu_filter` 纯软件滤波解算。 |
| **0.96寸 OLED 例程** | `驱动_框架/OLED_display/底层驱动源码/.../01-0.96OLED...例程/` | **Layer 2 (BSP)** | `bsp_oled` | 1. 提取 SSD1306 官方初始化命令列表；<br>2. 提取 `oledfont.h` (6×8, 8×16 ASCII 字模)；<br>3. 底层通信从软件模拟 I2C 重构为 `HAL_I2C_Mem_Write()` 硬件 I2C1 (PB6/PB7 400kHz)。 | **【禁止降级】** 严禁使用软件模拟 I2C 破坏硬件开漏复用配置；显存静态分配禁止局部变量导致栈溢出。 |
| **MiaoUI 菜单框架** | `驱动_框架/OLED_display/MiaoUI/` | **Layer 4 (App)** | `app_menu` | 1. 复用双向链表菜单树组织 4 大核心页面；<br>2. 复用 Q 弹非线性插值动画与超长文字跑马灯算法；<br>3. `dispDriver` 桥接 `bsp_oled`，`indevDriver` 桥接 `bsp_key`。 | **【裸机约束】** 严禁引入原工程附带的 FreeRTOS、FatFS 等重量级库，仅保留纯 C 核心 UI 调度器与基础控件。 |
