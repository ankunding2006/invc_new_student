# INVC 无线串口手柄系统

基于两套 STM32F103C8T6，实现摇杆、按键、拨码和六轴姿态采集，经自定义协议通过透明无线串口发送，在接收端显示并转发至 PC。

**当前进度（2026-10-04）：已完成应用层 C++17 封装，底层 C 实现保留。** 用户反馈重构前的基线已完成几乎全部硬件功能测试且正常；本次完成主机行为对照与双端 CMake Debug/Release 构建，没有重新烧录或进行硬件测试。详情见[项目状态](docs/project-status.md)与[C++ 迁移说明](docs/cpp-migration.md)。

## 从这里开始

| 你想做什么 | 阅读入口 |
|---|---|
| 第一次了解项目或查找资料 | [文档导航](docs/README.md) · [代码详细导读](docs/codebase_guide.md) |
| 编译固件、运行软件测试 | [构建与复测](docs/quick-start.md) |
| 使用按键菜单、查看日志与上位机数据 | [操作与上位机](docs/user-guide.md) |
| 了解 C++ 封装与 C 接口边界 | [C++ 迁移说明](docs/cpp-migration.md) |
| 理解模块、状态机和故障处理 | [实现架构](docs/architecture.md) · [代码详细导读](docs/codebase_guide.md) |
| 核对接线、引脚、默认参数和 CubeMX 覆盖配置 | [硬件引脚连接](docs/hardware-connection.md) · [硬件映射与配置](docs/hardware-configuration.md) |
| 对接或修改通信协议 | [通信协议](docs/protocol.md) |
| 继续开发、准备硬件验证 | [开发约定](docs/development.md) · [硬件验证清单](docs/hardware-validation.md) |

## 快速复测

在项目根目录执行；需要 Python 3、CMake、Ninja、主机 GCC/G++ 和 GNU Arm GCC/G++ 加入 PATH：

```powershell
python tests/run_tests.py
python tools/build_firmware.py
```

脚本不烧录硬件。测试产物位于 `build/tests/`；固件位于 `build/firmware/sender/` 与 `build/firmware/receiver/`。CMake/VS Code 使用及产物说明见[构建与复测](docs/quick-start.md)。

## 目录概览

```text
project/
├── sender/                 # 发送端：C++ App / C Service、BSP、Core、Drivers
├── receiver/               # 接收端：C++ App / C Service、BSP、Core、Drivers
├── tests/                  # 主机逻辑、HAL 模拟与双端集成测试
├── tools/                  # GNU Arm 构建与链接配置
├── docs/                   # 当前文档、验证记录、历史设计
├── question(pdf_version_and_txt_version)/  # 考核原题
└── 驱动_框架/              # 原始第三方参考资料
```

文档采用中文标题和稳定的英文文件名。原“项目级_Prompt _Rules 模板”已整理到 `docs/`，查找旧文件请看[迁移对照](docs/records/2026-09-18-docs-refactor.md)。
