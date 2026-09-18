# 早期设计归档

[文档导航](../README.md) · [当前实现](../architecture.md)

保留步骤 ①～⑩ 的推导、初始资源配置和原开发流程，供追溯设计演进。文内代码片段、参数与完成状态可能已过时。当前 API 请直接读头文件，现行线格式以[通信协议](../protocol.md)为准。

| 归档资料 | 原文件名 |
|---|---|
| [早期需求分析与量化目标](early-design/requirements.md) | `REQUIREMENTS.md` |
| [早期分层架构与模块划分](early-design/architecture.md) | `MODULE_ARCHITECTURE.md` |
| [早期模块接口草案](early-design/interfaces.md) | `MODULE_INTERFACES.md` |
| [早期数据模型与资源预算](early-design/data-model.md) | `DATA_DESIGN.md` |
| [早期系统状态机设计](early-design/state-machines.md) | `SYSTEM_BEHAVIOR.md` |
| [早期调度与时序预算](early-design/timing.md) | `TIMING_DESIGN.md` |
| [早期异常与边界设计](early-design/fault-handling.md) | `EXCEPTION_BOUNDARY.md` |
| [CubeMX 初始化阶段记录](early-design/cubemx-initialization.md) | `资源初始化后的总结.md` |
| [早期开发流程与规则](early-design/development-rules.md) | `AI_RULES.md` |

关键变化：原单向方案已增加 ACK/超时重传；旧 SPI /8 由 BSP 在运行时覆盖为 /128；姿态单位统一 0.1°；菜单和 IMU 使用精简实现；旧时序/内存预算不代表实测结果。完整理由见[软件实施记录](../records/2026-09-18-software-verification.md)。

历史开发规则中“一次对话仅一步”、强制某个第三方库和早期步骤编号，保留作流程记录。当前协作以用户当次授权和[开发约定](../development.md)为准。
