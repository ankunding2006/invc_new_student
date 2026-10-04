# 应用层 C++ 重构与行为对照记录

[文档导航](../README.md) · [迁移说明](../cpp-migration.md) · [复测命令](../quick-start.md)

日期：2026-10-04。用户反馈基线几乎全部硬件功能测试正常，要求保留现有逻辑、CMake/VS Code 工具链，只在应用与高层做 C++ 封装。本次没有连接板卡、烧录或改写算法。

## 基线与变更

基线取自本次工作开始时的原项目，不使用 9 月旧副本覆盖。保存 sender/receiver、tests、tools、docs、VS Code 和根配置快照；其 IMU 校准范围为 0.8～1.2g，保持原样。

替换四个 App C 源为四组 C++ 类源/头，旧 App C 头保留 ABI；3 个头补充 C++ linkage guard。CMake 目标改用 .cpp 并指定 C++17。主机模拟脚本分开编译 C 与 C++；新增可传参考目录的行为对照脚本。没有修改原逻辑测试断言、BSP/Service、Core/Drivers、Presets、链接配置或 `.vscode/`。

文档更新 CMake 使用、对象边界和用户硬件反馈；接线文档的“6 字节 ACK”更正为现行 9 字节，仅修正文档，没有更改协议。

## 源码与构建核对

33 个原函数体在剔除注释/空白、对应本模块方法改名后，token 序列一致。检查包含逻辑表达式、常量、循环与调用顺序；静态成员归属变化另由初始化与行为测试覆盖。

两端原 C 基线与新 C++ 版本均通过原 `tests/run_tests.py`。交叉编译沿用现有 GNU 12.3.1/CMake/Ninja，C 为原 C11，App 为 C++17。新版本两端 Release/Debug 均完整链接；主机混合编译启用 `-Wall -Wextra -Werror`。

原 main.c/init/task 调用不变；C 桥接符号正常解析。新 ELF 中未发现 operator new/delete、__cxa_guard、__cxa_throw、__gxx_personality 或应用全局动态初始化符号。平凡构造/析构另有 static_assert。

## 同输入逐步对照

测试分别加载两套独立 DLL，断言库句柄不同；同一模拟输入同时交给原 C 和新 C++，逐毫秒比较状态/计数、UART 完成数据；每 20 ms 比较双端全部 1024 字节显存。不是只比较测试最后一句 PASS。

| 场景 | 模拟 ms | UART 输出条数 | 显存快照 | 状态/计数读取数 | 结果 |
|---|---:|---:|---:|---:|---|
| clean_sequence_wrap | 9000 | 1215 | 900 | 180000 | 一致 |
| faults_inputs_loss_restarts_pc_modes | 18000 | 1499 | 1800 | 360000 | 一致 |
| tick_wrap_and_faults | 14000 | 1083 | 1400 | 280000 | 一致 |

压力场景包含随机摇杆、短按/双击/长按、IMU 与 OLED 故障、丢帧/ACK、损坏/分片/重复、噪声、PC T/F 切换、串口背压、发送/接收端独立重启和断链。总计 41,000 模拟 ms、3,797 条 UART 输出、4,100 幅显存、820,000 个状态/计数值完全一致。

使用 `python tests/test_cpp_equivalence.py --reference-root <基线根目录>` 复测；JSON 结果含各场景 trace SHA256。该程序本身不修改待测业务，不访问串口硬件。

## 资源对照（字节）

| 目标/配置 | C Flash | C++ Flash | C RAM | C++ RAM |
|---|---:|---:|---:|---:|
| sender Release | 34340 | 34212 | 7272 | 7272 |
| sender Debug | 48948 | 49340 | 7328 | 7328 |
| receiver Release | 18860 | 18716 | 5672 | 5672 |
| receiver Debug | 27984 | 28540 | 5672 | 5680 |

两者均使用当前用户的同一 CMake 链接配置；不能与 9 月旧构建脚本的数据直接混为同一基线。RAM 包含链接脚本预留；Debug 接收端增加 8 B，Release RAM 均不增加。未为追求字节数改变成员含义或业务逻辑。静态资源检查不等于最坏栈深和实时执行时间实测。

## 硬件状态与后续

用户已验证的是重构前基线，本次新固件尚未重新上板。保留相同操作、协议、引脚、时序常量和故障逻辑；若需要新版硬件回归，按[清单](../hardware-validation.md)另记结果。没有将“几乎全部功能正常”扩大成所有量化指标均已验收。

旧实现与配置保存在本次基线备份；发生后续修改时应新建基线，不能拿历史实验的相同结果当作新代码已验证。

## 写回原项目后的检查

写回前逐文件比对开始时的 SHA256，确认没有并发修改；写回后确认清单外源码和配置保持不变。原项目再次通过完整主机测试、两端 Debug/Release 构建及根 Preset 构建。原构建缓存仍引用旧 `Desktop/WorkSpace` 路径，已备份并用 `cmake --fresh` 重建，未修改 Presets 或 VS Code 配置。交付目录包含重构前 C 基线 ZIP、文件变更清单、软件验证日志和 Debug/Release 固件。
