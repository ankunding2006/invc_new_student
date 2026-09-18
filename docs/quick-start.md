# 构建与复测

[文档导航](README.md) · [当前验证记录](records/2026-09-18-software-verification.md)

## 环境准备

| 工具 | 用途 | 是否必需 |
|---|---|---|
| Python 3 | 运行测试和构建脚本 | 是 |
| 主机 GCC（Windows MSYS2 UCRT64） | 编译主机测试和 ctypes 模拟 DLL | 软件测试需要 |
| GNU Arm Embedded 工具链 | `arm-none-eabi-gcc`、`objcopy`、`size` | 固件构建需要 |
| Pillow | 从应用显存生成 OLED 图片 | 可选 |
| Keil MDK | 使用原 `.uvprojx` 工程 | 可选，本次未实编译验证 |

主机测试目前使用 Windows DLL，不应当作已经验证过 Linux/macOS 兼容性。先在 PowerShell 检查工具：

```powershell
python --version
gcc --version
arm-none-eabi-gcc --version
arm-none-eabi-objcopy --version
arm-none-eabi-size --version
```

若提示找不到命令，将对应工具的 `bin` 目录加入 PATH 后重新打开终端。主机 GCC 和交叉 GCC 用途不同，不能相互代替。

## 执行完整检查

在项目根目录执行（Python 3，主机 GCC 与 GNU Arm 工具需在 PATH）：

```powershell
python tests/run_tests.py
python tools/build_firmware.py
# 可选：需 Pillow，从真实应用绘制的显存生成预览
python tests/render_screens.py
```

测试不依赖串口设备，也不会烧录。默认测试输出在 `build/tests/`，固件输出在 `build/firmware/sender/` 和 `build/firmware/receiver/`。

2026-09-18 软件验证使用：主机 MSYS2 UCRT64 GCC、STM32CubeCLT 1.17.0 的 arm-none-eabi-gcc。两种编译均开启 `-Wall -Wextra -Werror`。Keil 工程已更新源文件清单，当前机器未运行 Keil 编译器；不要将 GNU 编译通过理解为 Keil 已实编译通过。

## 单项检查与输出

| 命令（项目根目录） | 内容 | 默认产物 |
|---|---|---|
| `python tests/run_logic.py` | 协议、重传、输入、姿态与统计 | `build/tests/` |
| `python tests/run_drivers.py` | HAL 模拟驱动回归 | `build/tests/` |
| `python tests/test_applications.py` | 双端业务、菜单、PC 输出 | `build/tests/` |
| `python tests/test_integration.py` | 两端链路、丢包、重启与回绕 | `build/tests/integration_results.json` |
| `python tests/render_screens.py` | 菜单和接收端显存布局 | `build/tests/oled_screens.png` |
| `python tools/build_firmware.py` | 两端 Cortex-M3 完整编译链接 | `build/firmware/` |

测试脚本以断言或非零返回码报告失败。完整成功时输出 `ALL SOFTWARE CHECKS PASSED`。固件目录中 HEX/BIN 为镜像，ELF 用于符号/调试分析，MAP 为链接布局，`.su` 为编译单元栈用量信息；这些产物不表示已烧录或完成硬件验收。

指定输出目录的示例：

```powershell
python tests/run_tests.py --build-dir build/tests-custom
python tools/build_firmware.py --out build/firmware-custom
```

## 使用原 Keil 工程

分别打开 [sender.uvprojx](../sender/MDK-ARM/sender.uvprojx) 和 [receiver.uvprojx](../receiver/MDK-ARM/receiver.uvprojx)。App/BSP/Service 已加入工程；GNU Arm 脚本使用独立的链接配置。Keil 版本、设备包及其实际编译结果需在对应环境确认。

新增源文件后也要更新 Keil 文件清单，不能只依赖 GNU 脚本自动发现业务目录。CubeMX 重生成后的核对要求见[开发约定](development.md)。
