# INVC 项目结构核查与修复记录

[文档导航](../README.md) · [当前状态](../project-status.md)

> 本文为日期快照；“当前”“待实现”等措辞指记录当天。历史文件清单保留当时路径；迁移后的入口见[重命名对照](2026-09-18-docs-refactor.md)。

核查日期：2026-09-17。项目根目录：`C:/Users/34118/Desktop/invc_exam/project/`。
本记录为代码审查与修改说明，硬件表现均未实测。

## 项目结构与实际进度

| 目录 | 职责 | 代码核验结果 |
|---|---|---|
| sender/Core、receiver/Core | CubeMX 时钟、引脚、外设初始化与中断入口 | 已生成；两个 main 仅初始化外设，业务循环为空 |
| sender/BSP | 摇杆、按键、拨码、IMU、OLED、串口适配 | 主要为占位实现；OLED 清屏与画点已有内存操作 |
| sender/Service | 遥测封包、环形队列、姿态滤波 | 封包与队列有实现；滤波、校准未实现 |
| sender/App | 发送任务与菜单状态 | 页面切换有实现；调度、渲染与确认动作未实现 |
| receiver/BSP | OLED 与双串口 | 主要为占位实现 |
| receiver/Service | 流式解包、队列、通信统计 | 队列与基础统计有实现；feed_byte 为空 |
| receiver/App | 接收、转发、掉线监测与界面 | 主任务与界面为空；回调有统计和原始载荷转发调用 |
| 两端 MDK-ARM | Keil 工程、启动文件 | Stack=0x800，Heap=0x200；本次补入已有业务源码 |
| 驱动_框架 | 第三方参考资料 | 尚未集成到当前业务实现；不能用参考工程完成度代表本工程 |
| 项目级_Prompt _Rules 模板 | Step 1～10 设计与开发流程 | 设计文档已形成，部分历史完成描述过时，本次已校正 |

预期数据流：发送端 BSP 采集 → Service 滤波/封包 → USART1 → 接收端队列/解析 → 统计、OLED、PC 转发。当前这条链路尚未接通。
按现有十五步流程，当前处于“编码实现”；本次为已有代码审查、缺陷修复与局部回归，不代表 Step 11～15 全部完成。

## 本次修复

1. 两端 `Service/srv_ring_buffer.c/.h`：零容量及未初始化零值对象调用 is_full 时会取模除零，非法重新初始化还会残留旧队列。现在容量小于 2 或存储为空时明确置为禁用状态，查询与读写安全返回。采用保留一个空槽的实现，可用容量为 size-1。
2. 同一队列缺少 ISR/主循环共享可见性约束。为共享索引、溢出计数、字节访问增加 volatile，计数使用索引快照；clear 只将消费者 tail 推到 head 快照，不再回写生产者 head。适用单核 STM32 上单 ISR 生产者、单主循环消费者；init 须在启用中断前完成，非通用多线程队列。
3. 两端 `BSP/bsp_usart.c`：未调用 HAL 却返回成功，改为返回失败，避免业务误记已发送。真实 UART 收发仍待实现。
4. `sender/BSP/bsp_imu.c`：未访问硬件却返回设备存在、读取成功及伪造 1g；改为失败并清零输出，补齐已声明的 get_status/recover，明确 NOT_INITIALIZED。
5. `sender/Service/srv_imu_filter.c`、`sender/App/app_sender.c`：未采样即宣称校准完成、未检查 IMU 初始化结果即进入正常状态。现保持未校准，初始化失败进入 FAULT_DEGRADED；成功分支进入 CALIBRATING。这只是状态可信性修复，校准状态机仍未实现。
6. 两端 `.uvprojx`：加入既有 App/BSP/Service 编译组和头文件路径，使这些源文件进入工程检查范围；未将未完成任务接入 main，也未修改 CubeMX 生成逻辑。
7. 文档：更新 AI_CONTEXT/PROTOCOL 中“尚未设计、无协议代码”等过时事实；标明早期 18 字节协议属于历史方案。现行协议仍是 23 字节、16 字节载荷、0x0D 帧尾、角度 ×10，不改线格式。
8. 文档：0.1° 应称十分之一度，不是厘度；每秒结算并清零计数的算法统一称固定窗口。
9. TIMING_DESIGN：80 页/秒 ×2.8ms =22.4% CPU 阻塞占用，修正原来的 2.8%；表内估算合计分别约 24.84%/24.09%，不是实测或峰值保证。修复示例把 `[128][8]` 显存的一页误当成连续内存的问题，先收集 128 个同页字节再传输。

## 仍待实现或重新核实的事项

- **完整硬件业务尚未实现**：ADC DMA 启动、实际按键/开关读取、SPI 采集、滤波与零偏校准、OLED 初始化/字体/分页、UART 收发与错误恢复、流式解包、定时任务和掉线处理。
- **菜单尚不构成层次化多级菜单**：目前只有四个平级页枚举和循环切换，enter/render 为空，须按题目补全层级与动作。
- **重传要求存在设计缺口**：原题明确要求“丢包重传/超时重发”，当前单向周期最新值推送不等同于重发。应在通信设计阶段明确机制，再同步两端及统计规则；本次未擅自新增 ACK/控制协议。
- **PC 格式未落地**：接收回调转发的是混合整数结构体，不是 JustFloat 编码；未来异步发送必须复制到生命周期足够长的发送缓冲，不能直接持有解析器临时载荷地址。
- **统计边界尚待设计**：现有 8 位差值只适用于顺序数据与有限丢包间隔，无法唯一识别发送端重启、乱序以及跨过一整轮 256 个序号的丢包。无数据时 freq/loss 为 0 不代表链路健康，须结合超时状态。现有接口假设调用方每 1000ms 结算，延迟结算时不能仍把计数直接当真实 Hz。
- **NVIC 文档与实际不同**：当前 SysTick=15，已启用 USART/DMA=0，接收端 USART2 IRQ 未启用；设计表的 0/1/2/3 只是目标。未来采用 USART2 中断发送前须通过 CubeMX 补齐配置。ISR 不应调用依赖 tick 的阻塞等待。
- **调度预算需实测**：1ms tick 不能直接表示 12.5ms；后续可使用 12/13ms 交替截止时间。分页阻塞 I2C 会带来毫秒级抖动；不能声称当前微秒级确定性。连续 ADC 配合极短 DMA 缓冲的中断频率也应在实现时计算。
- **接口仍有声明未定义**：两端 `bsp_oled_update_slice`、`bsp_oled_bus_unlock` 尚无实现，调用后会产生链接错误。没有补空函数掩盖未完成的硬件功能。
- **依赖方向存在局部欠账**：`bsp_imu.h` 为共享 imu_raw_t 引用 Service 头文件，和架构文档的严格向下依赖不一致，后续可将公共数据类型独立；本次未为此重构。
- **协议移植边界**：封包使用 packed 结构体 memcpy，在当前小端 STM32 目标上与规定一致；未来移植大端平台应改为显式小端编码。

## 验证

- 两端主机 C99 回归：GCC `-O2 -Wall -Wextra -Werror` 编译并运行通过。
- 回归覆盖：空/满/回绕队列、10000 次交替读写、越界 peek、空参数、非法重初始化、最小容量、溢出计数与 clear；23 字节已知协议向量、短缓冲保护、校验和；序列号 255→0、跳号与重复包；空统计窗口；未实现硬件不得返回成功以及发送端降级状态。
- ARM Cortex-M3 交叉编译语法检查：发送端 21 个、接收端 15 个 C 文件通过，包括现有 Core 与全部业务源码；无 warning/error。
- Keil XML 编译组逐文件检查通过；两端 ring buffer 的源文件与头文件保持一致。
- 未完成 Keil 固件完整链接、Flash/RAM map 测量、下载上板、总线电气或实时性测试；主机测试不能替代硬件验证。

复测命令（项目根目录）：`python tests/run_tests.py`，依赖 Python 3 与 GCC；产物默认写入系统临时目录。可用 `--build-dir` 指定输出位置。
后续按模块测试 → 功能联调 → 系统测试推进：优先打通 UART/封包/解包，再逐项接入 ADC、按键、IMU、OLED，分别保留两端调试日志与实测结果。

## 修改文件清单

以下均相对上述项目根目录；原有 `.vscode/` 未跟踪文件保持不变。

- `receiver/BSP/bsp_usart.c`
- `receiver/MDK-ARM/receiver.uvprojx`
- `receiver/Service/srv_ring_buffer.c`
- `receiver/Service/srv_ring_buffer.h`
- `receiver/Service/srv_stats.h`
- `sender/App/app_sender.c`
- `sender/BSP/bsp_imu.c`
- `sender/BSP/bsp_usart.c`
- `sender/MDK-ARM/sender.uvprojx`
- `sender/Service/srv_imu_filter.c`
- `sender/Service/srv_ring_buffer.c`
- `sender/Service/srv_ring_buffer.h`
- `tests/run_tests.py`
- `tests/test_services.c`
- `项目级_Prompt _Rules 模板/AI_CONTEXT.md`
- `项目级_Prompt _Rules 模板/DATA_DESIGN.md`
- `项目级_Prompt _Rules 模板/MODULE_ARCHITECTURE.md`
- `项目级_Prompt _Rules 模板/MODULE_INTERFACES.md`
- `项目级_Prompt _Rules 模板/PROTOCOL.md`
- `项目级_Prompt _Rules 模板/REQUIREMENTS.md`
- `项目级_Prompt _Rules 模板/REVIEW_2026-09-17.md`
- `项目级_Prompt _Rules 模板/SYSTEM_BEHAVIOR.md`
- `项目级_Prompt _Rules 模板/TIMING_DESIGN.md`
- `项目级_Prompt _Rules 模板/资源初始化后的总结.md`
