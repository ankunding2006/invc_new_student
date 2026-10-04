# CMake 构建与复测

[文档导航](README.md) · [C++ 迁移说明](cpp-migration.md) · [本次验证](records/2026-10-04-cpp-refactor.md)

## 环境与实际构建入口

当前使用 VS Code + CMake + Ninja + GNU Arm 工具链，不再以 Keil 为编译/下载/调试入口。本次沿用用户配置，未修改 `.vscode/`、Presets、工具链、启动文件或链接脚本。

| 工具 | 用途 |
|---|---|
| Python 3 | 构建包装脚本、主机测试与行为对照 |
| CMake ≥3.22、Ninja | 根工作区调度与 sender/receiver 独立构建 |
| arm-none-eabi-gcc / g++ / objcopy / size | C/C++/ASM 固件编译链接与产物转换 |
| 主机 GCC / G++ | C99 测试、C++17 App、Windows ctypes DLL |
| Pillow | 可选的 OLED 显存预览 |
| 已配置的 Cortex-Debug / OpenOCD | 用户在 VS Code 中下载调试；本次未连接设备 |

以上命令需在终端 PATH 可用。交叉工具链负责 MCU，主机 GCC/G++ 负责模拟，两者不能替代。本次交叉工具链为 STM32CubeCLT 1.17.0 自带 GNU 12.3.1。业务 C++17 关闭异常/RTTI/线程安全局部静态初始化；C 生成代码仍按原 C11 配置编译。

## 推荐：从根目录构建两端

```powershell
# 默认 Release，生成并收集两端固件
python tools/build_firmware.py

# Debug，可另选收集目录以保留 Release 产物
python tools/build_firmware.py --config Debug --out build/firmware-debug
```

[包装脚本](../tools/build_firmware.py)已由用户改为调用各端 CMake Presets，本次保持其行为。编译实际发生在 `sender/build/<配置>/`、`receiver/build/<配置>/`；`--out` 只改变 ELF/HEX/BIN/MAP 的复制目录，不会搬移 CMake 缓存。默认收集路径 `build/firmware/` 若连续构建不同配置会被后一次覆盖。

## 直接使用 CMake

从项目根目录构建两个子工程：

```powershell
cmake --preset Release
cmake --build --preset Release
```

根项目为调度层，`sender`、`receiver` 为独立目标，`all_firmware` 默认构建两端。根 Preset 不直接编译固件源码。只构建某一端时，进入对应目录执行：

```powershell
cd sender
cmake --preset Debug
cmake --build --preset Debug
```

接收端同理。应用源文件清单位于双方 `CMakeLists.txt`，CubeMX 内容位于各自 `cmake/stm32cubemx/`；新增 App 源文件需要登记，不能依赖通配发现。

如果移动过项目目录，CMake 报缓存路径与当前源目录不一致，可在报错的根目录或端目录执行 `cmake --fresh --preset Debug`（或 `Release`）重新配置，再构建；`--fresh` 需要 CMake 3.24 及以上，会重置该构建目录的缓存，手动添加的缓存选项应先记录。本次发现并备份了指向旧 `Desktop/WorkSpace` 路径的缓存，根项目及两端均已按当前位置重新生成。

## VS Code 的现有任务与调试

保留原 task 名称、target 名称及 ELF 路径：`sender/build/Debug/sender.elf`、`receiver/build/Debug/receiver.elf`，Release 对应更换目录名。现有 `preLaunchTask`、DAPLink/ST-LINK 和 SVD 配置继续使用。

首次构建或没有缓存时先 configure；CMake Tools/IntelliSense 使用生成的 compile_commands。C++ 标准在构建目标上正式设置为 17，不仅是编辑器提示设置。此说明不表示本次重新验证了下载器或硬件调试。

## 主机回归与 C/C++ 对照

```powershell
python tests/run_tests.py
python tests/render_screens.py  # 可选，需 Pillow
```

完整测试成功输出 `ALL SOFTWARE CHECKS PASSED`；HAL 模拟不等于上板实测。`tests/build_simulations.py` 按扩展名分别用 GCC 编译 C、G++ 编译 C++，最后用 G++ 链接，检查实际 C ABI 边界。现有 C 测试断言没有为重构降低标准。

| 单项入口 | 检查内容 |
|---|---|
| `python tests/run_logic.py` | 原 C 协议、输入、滤波、统计逻辑 |
| `python tests/run_drivers.py` | 原 C 驱动与 mock HAL |
| `python tests/test_applications.py` | 通过 C 入口调用 C++ App，检查菜单/故障/PC |
| `python tests/test_integration.py` | 双端丢包、重启、回绕 |
| `python tests/test_cpp_equivalence.py --reference-root <重构前完整快照目录>` | 同输入下对照 C 与 C++ 的 UART、状态和显存 |

行为对照的参考目录必须包含原 C 应用和原测试构建脚本，不是当前工程。脚本默认分别构建两套 DLL；`--skip-build` 仅用于已有且未过时的 DLL，`--out` 指定结果 JSON。参考基线内容摘要见[迁移记录](records/2026-10-04-cpp-refactor.md)。

## 产物与历史配置

HEX/BIN 是镜像，ELF 是调试文件，MAP 用于资源布局，`.su` 为编译单元栈用量。CMake Debug/Release 均保留原优化级别与链接布局。

早期的 `tools/STM32F103C8_FLASH.ld` 和 `tools/gcc_runtime.c` 留作历史文件，现有包装脚本不使用它们；当前链接脚本在各端目录。历史 Keil 文件即使仍保留也不是本次支持的构建入口，不再同步 C++ 源清单。
