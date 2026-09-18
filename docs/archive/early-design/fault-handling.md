# 早期异常与边界设计

[文档导航](../../README.md) · [归档索引](../README.md)

> 历史资料：保留早期设计推导与当时约定，不代表当前代码或已完成验收。继续开发请阅读[当前状态](../../project-status.md)、[实现架构](../../architecture.md)与[开发约定](../../development.md)。


> **设计依据**：  
> 依据 2026 INVC 嵌入式软件考核要求、系统行为设计（[SYSTEM_BEHAVIOR.md](state-machines.md)）、时序设计（[TIMING_DESIGN.md](timing.md)）与数据设计（[DATA_DESIGN.md](data-model.md)），本文档确立手柄发送端（Sender）与接收端（Receiver）全系统在**通信故障、总线死锁、硬件掉线、数据越界、数学除零**等极端恶劣条件下的防御机制与自愈恢复流程。  
> **核心铁律**：全系统遵循**防御性编程（Defensive Programming）**，任何异常均不得导致微控制器死锁（HardFault/死循环），任何硬件故障均需具备自动检测、功能降级运行与恢复自愈能力。

---

## 一、 通信层异常与边界防护矩阵 (Communication Faults)

| 异常标识 | 异常场景与触发条件 | 故障现象与潜在风险 | 软件防御策略与自愈恢复机制 |
|---|---|---|---|
| **E-COM-01** | **UART 硬件错误 (ORE / NE / FE / PE)**<br>空中噪声冲击或中断延迟导致硬件溢出错误（Overrun Error）置位。 | 硬件接收移位寄存器被冻结，单片机停止接收后续任何串口字节。 | **中断自动清除**：在 `USART1_IRQHandler` 入口首先检查 `USART_SR` 的 `ORE/NE/FE` 标志。一旦置位，执行“读 SR 寄存器后读 DR 寄存器”的标准清除时序，并清空接收错误状态，立即恢复接收。 |
| **E-COM-02** | **环形接收缓冲区满溢**<br>主循环因偶发耗时操作未能及时弹出消费数据，导致 `ring_buffer` 写满。 | 写指针越界破坏内存，引发 RAM 变量踩踏或系统崩溃。 | **安全截断与溢出标记**：`srv_ring_buffer_push` 在入队前强制执行 `(head + 1) % size == tail` 检查。若满则**直接拒绝写入并丢弃新字节**，置位 `overflow_flag` 错误计数器，严禁写指针向前推进。 |
| **E-COM-03** | **帧头丢失与随机噪声注入**<br>开机瞬间或信号遮挡引入大量无意义的乱码字节（Garbage Bytes）。 | 解包器卡死，无法识别后续到达的正常完整数据包。 | **逐字节滑动重同步**：流式解包状态机严格以 `0xAA 0x55` 触发。只要当前状态机在任意环节收到非法字节，**立即将状态复位回 `PARSER_WAIT_HEADER_1`**，将后续字节重新当作潜在帧头逐一比对，绝不挂死。 |
| **E-COM-04** | **载荷长度字段畸变越界**<br>传输中第 4 字节 `Length` 被噪声篡改为极大值（如 255）。 | 解包器试图读取超出有效载荷的数据，引发缓冲区读溢出。 | **定长硬约束校验**：本系统协议载荷定长为 16 字节。状态机在进入 `PARSER_READ_LEN` 时，若发现 `len != 16`，立即判定为非法畸变帧，重置状态机并累加 `frame_error_count`。 |
| **E-COM-05** | **校验和不匹配 (Checksum Error)**<br>数据帧在空中传输时发生偶发位翻转。 | 接收端使用脏数据，导致摇杆控制量剧烈跳变。 | **累加和严格比对**：计算 Byte 2 至 Byte 20 的 8 位累加和，若与 Byte 21 不符，**彻底丢弃该包**，不触发解包回调，丢包率统计引擎自动将其计入丢失包数。 |
| **E-COM-06** | **无线通信链路完全中断 (Timeout)**<br>手柄关机、电池耗尽或距离过远，接收端连续接收不到信号。 | 接收端保持上一次的摇杆指令，导致受控端失控飞车。 | **1000ms 超时进入离线安全态**：接收端以 100ms 周期检查距上一次收包的时间差。若超过 $1000\text{ ms}$，切入 `RECEIVER_STATE_OFFLINE`，控制量全部归零，屏幕提示 `"LINK LOST"`。 |
| **E-COM-07** | **流水号 255 回绕与重复包**<br>`seq_id` 计数器从 255 循环回绕到 0，或偶发收到重发包。 | 丢包率算法误判丢失 255 个包，丢包率瞬间飙升至 100%。 | **环形模 256 差值算法**：丢包数严格依据 `(new_seq - last_seq) & 0xFF` 判定。若差值等于 1 计 0 丢包；若差值大于 1 计丢包；若差值等于 0 判定为重复包（丢包计 0）。 |

---

## 二、 硬件外设与总线异常自愈模型 (Hardware & Bus Fault Recovery)

### 2.1 I2C 屏幕总线死锁与软件脉冲释放机制 (E-HW-01)

* **物理机理**：面包板杜邦线极易受到机械振动或接触不良干扰。若 MCU 在通过硬件 I2C 发送时发生抖动，或从机（SSD1306）正在拉低 SDA 返回 ACK 时 MCU 发生复位，从机会一直拉低 SDA 企图完成传输，而 MCU 硬件 I2C 检测到 SDA 为低即认为总线被占用（`HAL_BUSY` 或 `HAL_TIMEOUT`），导致两端陷入死锁。
* **自愈恢复流程 (Bus Deadlock Recovery)**：
```text
[检测到 I2C 传输返回 HAL_TIMEOUT / HAL_BUSY 连续 >= 3 次]
                 │
                 ▼
[触发 bsp_oled_bus_unlock() 应急自愈]
                 │
                 ├─► 1. 禁用片上 I2C1 外设 (I2C1->CR1 &= ~I2C_CR1_PE)
                 ├─► 2. 将 SCL(PB6) 与 SDA(PB7) 临时重配置为普通 GPIO 开漏输出
                 ├─► 3. SDA 保持高电平，MCU 驱动 SCL 连续翻转 9 个时钟脉冲 (80kHz)
                 │      (向从机发送 9 个虚拟时钟，强制从机将未发完的数据移出并释放 SDA)
                 ├─► 4. 发送模拟 I2C STOP 停止信号 (在 SCL 为高期间拉高 SDA)
                 ├─► 5. 重新将 PB6/PB7 切换为片上硬件复用开漏模式 (AF Open-Drain)
                 └─► 6. 重新使能 I2C1 外设并调用 bsp_oled_init() 重新发送初始化配置
                 │
                 ▼
[总线成功解开死锁，恢复正常 OLED 12.5ms 切片刷屏]
```

### 2.2 MPU6500 六轴掉线与降级运行模型 (E-HW-02)

* **故障判定**：开机自检时连续读取 `WHO_AM_I` (0x75) 寄存器 3 次均非 `0x70`；或运行中连续 5 次读取加速度/角速度数据返回全 0、全 0xFF 或 SPI 传输超时。
* **分级容错与功能降级**：
  1. **控制链路保全**：手柄主控状态机切入 `APP_STATE_FAULT_DEGRADED` 降级模式。**摇杆 (ADC) 与按键 (Key) 正常工作并外发**，确保核心遥控功能不瘫痪；
  2. **姿态安全归零**：姿态解算层立即输出固定零值（Pitch=0.0°, Roll=0.0°, Yaw=0.0°），防止受控设备因姿态突变而剧烈翻转；
  3. **屏幕直观告警**：在 OLED 状态栏闪烁显示 `"IMU ERR"` 告警字样；
  4. **后台低频自愈探测**：主循环以 1.0 秒低频周期尝试通过 SPI1 读取 WHO_AM_I。一旦杜邦线插好握手成功，系统**自动、无缝恢复回 `APP_STATE_NORMAL`**，无需重启单片机。

### 2.3 摇杆极端物理量越界截断 (E-HW-03)

* **物理边界**：电位器磨损或引脚虚焊可能导致 ADC 采样出现小于 0 或大于 4095 的野值；运算浮点电压可能溢出 3300mV。
* **数学硬截断模型**：
  ```c
  #define CLAMP(x, min, max)  ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

  /* 物理电压严格约束在 0 ~ 3300 mV */
  p_data->x_voltage_mv = CLAMP(v_x_mv, 0, 3300);
  p_data->y_voltage_mv = CLAMP(v_y_mv, 0, 3300);

  /* 归一化映射值严格约束在 -1000 ~ +1000 */
  p_data->x_mapped = CLAMP(mapped_x, -1000, 1000);
  p_data->y_mapped = CLAMP(mapped_y, -1000, 1000);
  ```

### 2.4 按键机械常通 / 卡死单次触发保护 (E-HW-04)

* **故障机理**：面包板按键机械触点粘连或内部上拉电阻虚焊短路，导致 GPIO 长期处于低电平。
* **保护逻辑**：
  - 按键消抖 FSM 引入 `LONG_PRESS_TRIGGERED` 状态标记；
  - 持续按下超过 2.0s 触发 `KEY_EVENT_LONG_PRESS` 瞬间，**立即锁定触发标记**；
  - 只要按键不完全抬起释放（GPIO 不变高），后续状态机**严禁重复产生长按事件或单击事件**，防止系统反复进出菜单或持续清零发包数。

---

## 三、 软件运行时与数学边界防御体系 (Software & Math Safety)

### 3.1 OLED 显存数组越界踩踏防御 (E-SW-01)

* **风险点**：绘图函数（画点、画线、写字符）若传入异常坐标（如动态波形计算失控产生 $X=150, Y=80$），会导致二维显存数组 `s_oled_gram[128][8]` 越界，直接覆盖后续的全局变量（如 UART 环形队列或状态机变量），引发极难排查的 HardFault。
* **底层入口硬拦截**：
  ```c
  void bsp_oled_draw_pixel(uint8_t x, uint8_t y, uint8_t color) {
      /* 强制安全边界检查 */
      if (x >= OLED_WIDTH || y >= OLED_HEIGHT) {
          return; /* 立即拦截，绝不越界写入 RAM */
      }
      if (color) {
          s_oled_gram[x][y / 8] |= (1 << (y % 8));
      } else {
          s_oled_gram[x][y / 8] &= ~(1 << (y % 8));
      }
  }
  ```

### 3.2 丢包率与统计除零硬防御 (E-SW-02)

* **风险点**：上电初期或通信断开期间，1 秒窗口内收到的有效包 $N_{\text{valid}} = 0$，丢失包 $N_{\text{lost}} = 0$。若直接计算 $\frac{N_{\text{lost}}}{N_{\text{valid}} + N_{\text{lost}}}$，会导致浮点除以零，产生 `NaN`（Not a Number）或 `Inf`，污染显示界面。
* **防守代码实现**：
  ```c
  void srv_stats_tick_1000ms(void) {
      uint32_t total_in_window = s_metrics.window_valid + s_metrics.window_lost;
      
      s_metrics.current_freq_hz = (float)s_metrics.window_valid;
      
      if (total_in_window == 0) {
          /* 分母为零安全保护，不除以零 */
          s_metrics.current_loss_rate_pct = 0.0f;
      } else {
          s_metrics.current_loss_rate_pct = ((float)s_metrics.window_lost / (float)total_in_window) * 100.0f;
      }
      
      /* 限制在 0.0 ~ 100.0% 合法区间 */
      s_metrics.current_loss_rate_pct = CLAMP(s_metrics.current_loss_rate_pct, 0.0f, 100.0f);
  }
  ```

### 3.3 姿态解算矢量归一化下限保护 (E-SW-03)

* **风险点**：互补滤波或 Mahony 算法在计算加速度向量单位化时需计算模长 $\sqrt{a_x^2 + a_y^2 + a_z^2}$。在自由落体、剧烈震动或传感器故障时，模长极度接近 0，直接除以模长会产生浮点溢出。
* **保护代码实现**：
  ```c
  float norm = sqrtf(ax * ax + ay * ay + az * az);
  if (norm < 1e-4f) {
      /* 模长异常过小，放弃本次加速度计修正，仅依赖陀螺仪积分 */
      return;
  }
  ax /= norm;
  ay /= norm;
  az /= norm;
  ```

### 3.4 SysTick 49.7 天时钟回绕免疫数学证明 (E-SW-04)

* **物理事实**：32 位无符号整数 `uint32_t` 最大值为 $2^{32}-1 = 4294967295\text{ ms} \approx 49.71\text{ 天}$。达到该时间后，`HAL_GetTick()` 将从最大值翻转回 0。
* **数学证明**：
  在 ANSI C 标准中，无符号整数减法定义为模 $2^{32}$ 运算。设当前时间已回绕为 $T_{\text{now}} = 5$，上次记录时间为 $T_{\text{last}} = 4294967290$，设定周期为 $P = 10\text{ ms}$：
  $$(T_{\text{now}} - T_{\text{last}}) \pmod{2^{32}} = (5 - 4294967290) \pmod{2^{32}} = 15$$
  此时运算结果精确为 $15$，判定 $15 \ge 10$ 为真，任务准时触发！
* **铁律**：时间戳变量一律使用 `uint32_t`，比较表达式一律书写为 `(uint32_t)(now - last) >= period`，绝不可书写为 `now >= (last + period)`（后者在回绕时会导致系统永久卡死）。

---

## 四、 防御性编程自检核对表 (Verification Checklist)

| 检查项 | 验证内容 | 判定合格标准 |
|---|---|---|
| **CHK-EXC-01 [I2C 防死锁]** | 人为在运行中断开或短路 OLED 的 SDA 杜邦线 | 单片机**绝不卡死**，串口调试正常输出，插回后屏幕恢复刷新 |
| **CHK-EXC-02 [IMU 热插拔]** | 拔掉手柄端 MPU6500 的 SPI 杜邦线 | 手柄切入降级态，屏幕提示 `"IMU ERR"`，摇杆按键正常遥控外发，插回后自动恢复解算 |
| **CHK-EXC-03 [通信抗噪声]** | 在接收端串口中持续注入随机十六进制乱码 | 流式解包器不卡死，一旦收到 `0xAA 0x55` 正常数据帧立即恢复正常解包 |
| **CHK-EXC-04 [通信断线检测]**| 关掉手柄电源 | 接收端在 1000ms 后准时触发 `"LINK LOST"` 提示，丢包率灵敏升高 |
| **CHK-EXC-05 [显存越界拦截]**| 强制调用 `bsp_oled_draw_pixel(150, 100, 1)` | 绘图函数静默拦截，系统全局变量不被篡改，无 HardFault |
| **CHK-EXC-06 [统计除零保护]**| 上电第 0 秒在无数据包到达时计算丢包率 | 丢包率数值稳定输出 `0.0%`，无 `NaN` 或崩溃 |
