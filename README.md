# INVC 无线串口手柄系统

基于两套 STM32F103C8T6，实现摇杆、按键、拨码和六轴姿态采集，经自定义协议通过透明无线串口发送，在接收端显示并转发至 PC。

**当前进度：⑪～⑭ 软件实现与模拟联调已完成。** 两端 GNU Arm 编译链接和主机测试已通过；硬件联调、精度与实时性验收尚未执行。详情见[项目状态](docs/project-status.md)。

## 从这里开始

| 你想做什么 | 阅读入口 |
|---|---|
| 第一次了解项目或查找资料 | [文档导航](docs/README.md) |
| 编译固件、运行软件测试 | [构建与复测](docs/quick-start.md) |
| 使用按键菜单、查看日志与上位机数据 | [操作与上位机](docs/user-guide.md) |
| 理解模块、状态机和故障处理 | [实现架构](docs/architecture.md) |
| 核对引脚、默认参数和 CubeMX 覆盖配置 | [硬件映射与配置](docs/hardware-configuration.md) |
| 对接或修改通信协议 | [通信协议](docs/protocol.md) |
| 继续开发、准备硬件验证 | [开发约定](docs/development.md) · [硬件验证清单](docs/hardware-validation.md) |

## 快速复测

在项目根目录执行；需要 Python 3、主机 GCC 和 GNU Arm 工具加入 PATH：

```powershell
python tests/run_tests.py
python tools/build_firmware.py
```

脚本不烧录硬件。测试产物位于 `build/tests/`；固件位于 `build/firmware/sender/` 与 `build/firmware/receiver/`。环境准备、Keil 状态及产物说明见[构建与复测](docs/quick-start.md)。

## 目录概览

```text
project/
├── sender/                 # 发送端：App / Service / BSP / Core / Drivers
├── receiver/               # 接收端：App / Service / BSP / Core / Drivers
├── tests/                  # 主机逻辑、HAL 模拟与双端集成测试
├── tools/                  # GNU Arm 构建与链接配置
├── docs/                   # 当前文档、验证记录、历史设计
├── question(pdf_version_and_txt_version)/  # 考核原题
└── 驱动_框架/              # 原始第三方参考资料
```

文档采用中文标题和稳定的英文文件名。原“项目级_Prompt _Rules 模板”已整理到 `docs/`，查找旧文件请看[迁移对照](docs/records/2026-09-18-docs-refactor.md)。
