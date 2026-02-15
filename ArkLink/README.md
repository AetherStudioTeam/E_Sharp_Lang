# ArkLink 初始化架构说明

## 目标
根据 [`arklink_refactor_plan.md`](ArkLink/docs/arklink_refactor_plan.md) 建立新的模块化链接器骨架，拆分公共 API、核心上下文、Loader、Resolver、Backend 与 CLI/测试结构，后续可逐步填充真实实现。

## 目录现状
```
ArkLink/
├─ include/ArkLink/
│  ├─ arklink.h
│  ├─ context.h
│  ├─ loader.h
│  ├─ resolver.h
│  ├─ backend.h
│  └─ targets/{pe.h, elf.h}
├─ src/core/
│  ├─ context.c
│  ├─ job.c
│  ├─ loader_eo.c
│  ├─ resolver.c
│  └─ backend.c
├─ src/backends/{pe,elf}/
│  ├─ pe_backend.c
│  └─ elf_backend.c
├─ src/cli/main.c
└─ tests/
   ├─ loader_tests.c
   ├─ resolver_tests.c
   └─ smoke/{pe_runtime.sh, elf_runtime.sh}
```

## 核心组件
- `arklink.h`：扩展 `ArkLinkJob` 输入结构、错误码、Logger 回调等公共 API。
- `context.h/context.c`：实现 `ArkArena`、字符串驻留、SectionBuffer 管理、Logger 代理。
- `loader.h/loader_eo.c`：定义 `ArkLinkUnit`、Section/符号/重定位描述，提供 EO 解析骨架（文件读取、Header 校验、TODO: 详细表解析）。
- `resolver.h/resolver.c`：规划 `ArkResolverPlan`（符号、重定位、导入、入口信息）。当前返回空计划并为后续构建 `ArkBackendInput` 做准备。
- `backend.h/backend.c`：定义 `ArkBackendInput`（section/symbol/import/Reloc），实现 Backend 注册表、`ark_backend_query` 与默认 PE/ELF 注册。
- `src/backends/*`：PE/ELF Backend 桩函数返回 `ArkBackendOps`，后续填入真实 prepare/write 逻辑。
- `src/cli/main.c`：加入 `--target`、`-o` 参数解析、Logger 回调、`ArkLinkInput` 组装。

## 测试与脚本
- `tests/loader_tests.c` / `resolver_tests.c` 为单测入口。
- `tests/smoke/*.sh` 提供 PE/ELF 烟雾测试脚本占位，便于后续在 CI 中验证。

## 后续工作
- 填写 Loader section/symbol/reloc 解析细节。
- 完成 Resolver 中的符号合并、导入计划与 `ArkBackendInput` 构建。
- 实现 PE/ELF Backend 真正的 prepare/write 流程（section 排布、IAT/TLS/Reloc 等）。
- 为 CLI 添加 runtime/import 配置文件选项并接入 `ArkRuntimeConfig`。
- 扩充单元测试与 smoke 脚本逻辑，确保 EO 样本覆盖常见错误场景。
