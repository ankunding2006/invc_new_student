# 文档导航

[项目首页](../README.md)

当前文档按使用、实现和维护任务组织；带日期的记录保存当时证据，归档保留早期推导。

当前为 C++17 应用层 + C 服务/驱动工程，构建使用用户现有 CMake/VS Code。新增入口：[C++ 迁移说明](cpp-migration.md) · [行为与资源对照](records/2026-10-04-cpp-refactor.md)。

## 推荐阅读顺序

1. [项目状态与需求对应](project-status.md)：系统做什么、软件完成到哪里、哪些还没验证。
2. [构建与复测](quick-start.md)：准备工具、运行测试、生成固件。
3. [操作与上位机](user-guide.md)：菜单按键、串口日志、JustFloat 通道。
4. [实现架构](architecture.md)：读代码前了解模块、状态机、数据流和恢复行为。
5. [硬件引脚连接](hardware-connection.md)、[硬件映射与配置](hardware-configuration.md)与[通信协议](protocol.md)：查接线、引脚、参数、线格式。
6. [开发约定](development.md)与[硬件验证清单](hardware-validation.md)：继续开发和后续上板记录。

## 当前文档各自维护什么

| 文档 | 主要内容 |
|---|---|
| [project-status.md](project-status.md) | 当前完成情况、题目需求到实现/证据的对应 |
| [quick-start.md](quick-start.md) | 构建命令、测试命令、依赖与输出目录 |
| [user-guide.md](user-guide.md) | 操作步骤、日志字段、PC 通道表与常见现象 |
| [cpp-migration.md](cpp-migration.md) | 应用对象、C ABI、文件迁移与行为等价验证 |
| [architecture.md](architecture.md) | 模块职责、入口、时序、数据所有权、故障处理 |
| [hardware-connection.md](hardware-connection.md) | 双端外设完整引脚连接表、有线直连与电气注意事项 |
| [hardware-configuration.md](hardware-configuration.md) | 引脚、器件、运行配置及参数修改位置 |
| [protocol.md](protocol.md) | 遥测/ACK 字节布局、校验、重传、统计定义 |
| [development.md](development.md) | 分步流程、代码约束、修改后应同步的文档与测试 |
| [hardware-validation.md](hardware-validation.md) | 待验证项目、观察方法和实测记录格式 |

## 验证与历史

- [2026-10-04 C++ 重构记录](records/2026-10-04-cpp-refactor.md)：当前 C 基线、原测试、行为对照及 CMake 资源变化。

- [2026-09-17 代码核查](records/2026-09-17-code-review.md)：实现前发现的问题及当时修复。
- [2026-09-18 软件实现与验证](records/2026-09-18-software-verification.md)：⑪～⑭ 实现、测试场景、资源占用与限制。
- [2026-09-18 实施文件清单](records/2026-09-18-software-files.json)：当时的路径快照，保留旧文档路径用于追溯。
- [文档迁移对照](records/2026-09-18-docs-refactor.md)：旧名称到当前入口的映射。
- [早期设计归档](archive/README.md)：需求量化、接口草案、资源预算与原开发规则。

## 阅读时如何判断结论

“当前实现”应与源码一致；“验证记录”必须区分主机模拟、交叉编译和真实硬件实测；“历史设计”不作为当前参数/API 的依据。若发现冲突，先核对源码和考核原题，再更新对应文档，不把旧的预算值解释成实测值。

考核原题及第三方资料保留在原目录。本文档集是工程交接与复测说明，个人考核报告仍需结合实际设计思考和硬件实验记录完成。
