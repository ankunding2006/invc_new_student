# 文档重构与重命名记录

[文档导航](../README.md)

日期：2026-09-18。范围为项目文档，未修改固件源码、测试脚本、构建配置、考核原题或第三方资料。

## 调整内容

将“项目级_Prompt _Rules 模板”整理为 `docs/`。项目首页缩短为介绍与导航；构建、使用、架构、配置、协议和开发约定分别维护；当前状态、带日期证据和历史设计分开存放。文件名采用小写英文与连字符，文内标题与说明使用中文。

保留早期设计正文供追溯，添加历史标识并修复文档引用；旧的阶段规则原文归档，开发流程按最近用户约定整理。已有 JSON 文件清单和审查记录中的历史路径保留，不伪装成现在的文件清单。

## 原目录文件迁移

下列旧名称均位于原 `项目级_Prompt _Rules 模板/`。`AI_RULES.md` 原文归档，现行摘要另见[开发约定](../development.md)。

| 原文件名 | 新位置 |
|---|---|
| `AI_CONTEXT.md` | [project-status.md](../project-status.md) |
| `AI_RULES.md` | [archive/early-design/development-rules.md](../archive/early-design/development-rules.md) |
| `PROTOCOL.md` | [protocol.md](../protocol.md) |
| `REQUIREMENTS.md` | [archive/early-design/requirements.md](../archive/early-design/requirements.md) |
| `MODULE_ARCHITECTURE.md` | [archive/early-design/architecture.md](../archive/early-design/architecture.md) |
| `MODULE_INTERFACES.md` | [archive/early-design/interfaces.md](../archive/early-design/interfaces.md) |
| `DATA_DESIGN.md` | [archive/early-design/data-model.md](../archive/early-design/data-model.md) |
| `SYSTEM_BEHAVIOR.md` | [archive/early-design/state-machines.md](../archive/early-design/state-machines.md) |
| `TIMING_DESIGN.md` | [archive/early-design/timing.md](../archive/early-design/timing.md) |
| `EXCEPTION_BOUNDARY.md` | [archive/early-design/fault-handling.md](../archive/early-design/fault-handling.md) |
| `资源初始化后的总结.md` | [archive/early-design/cubemx-initialization.md](../archive/early-design/cubemx-initialization.md) |
| `REVIEW_2026-09-17.md` | [records/2026-09-17-code-review.md](2026-09-17-code-review.md) |
| `SOFTWARE_VERIFICATION.md` | [records/2026-09-18-software-verification.md](2026-09-18-software-verification.md) |
| `SOFTWARE_CHANGED_FILES.json` | [records/2026-09-18-software-files.json](2026-09-18-software-files.json) |

## 新增阅读入口

[文档首页](../README.md)、[构建与复测](../quick-start.md)、[操作与上位机](../user-guide.md)、[实现架构](../architecture.md)、[硬件映射与配置](../hardware-configuration.md)、[开发约定](../development.md)、[硬件验证清单](../hardware-validation.md)。根 README 保留快速运行命令，将详细说明链接到对应主题。

## 核对范围

文档内部链接与源码链接、相对路径可移植性、标题层级和代码块配对；参数/状态/API 与现有代码对应；重构前后固件源码、测试脚本和工程配置哈希一致。该改动不改变程序行为，不将上一轮软件测试记录写成新增硬件测试结果。
