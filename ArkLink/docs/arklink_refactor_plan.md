# ArkLink 重构设计草案

## 目标概述
- 将目前集中在[`ArkLink/src/core/linker.c`](ArkLink/src/core/linker.c)的解析、符号、导入、重定位与写出逻辑拆分成可复用库。
- 让 CLI 仅负责参数解析与调用公开 API，避免直接操作内部结构（现状参考[`ArkLink/src/cli/main.c`](ArkLink/src/cli/main.c)）。
- 基于可扩展的 Backend 体系同时支持 PE / ELF，保留现有 EO 对象格式并易于扩展到 Mach-O。
- 统一 LinkContext 生命周期与内存策略，防止 `_strdup`/`malloc` 混用导致泄漏。

## 目录与接口规划
```
ArkLink/
├─ include/ArkLink/
│  ├─ arklink.h            // 顶层 API: arklink_link_job, arklink_init_job 等
│  ├─ context.h            // LinkContext/arena API
│  ├─ loader.h             // ark_loader_load_unit()
│  ├─ resolver.h           // ark_resolver_resolve()
│  ├─ backend.h            // ArkBackendOps 接口
│  ├─ targets/pe.h         // PE backend 对外配置
│  └─ targets/elf.h
├─ src/
│  ├─ core/
│  │  ├─ context.c         // arena、字符串池、SectionBuffer 等
│  │  ├─ job.c             // LinkJob -> LinkContext 构建 & 销毁
│  │  ├─ loader_eo.c       // 解析 EO -> LinkUnit
│  │  ├─ resolver.c        // 符号、别名、导入计划、重定位队列
│  │  └─ planner.c         // 将 LinkUnits 排程为 BackendInput
│  ├─ backends/
│  │  ├─ pe/
│  │  │  ├─ pe_backend.c   // 实现 ArkBackendOps
│  │  │  └─ pe_sections.c  // header/section/IAT/TLS/Reloc
│  │  └─ elf/
│  │     ├─ elf_backend.c
│  │     └─ elf_sections.c
│  └─ cli/
│     └─ main.c            // 仅依赖 include/ArkLink/arklink.h
└─ tests/
   ├─ loader_tests.c
   ├─ resolver_tests.c
   └─ smoke/
      ├─ pe_runtime.sh
      └─ elf_runtime.sh
```

## 公共 API 草案
```c
// arklink.h
typedef struct {
    const char** inputs;
    size_t input_count;
    const char* output;
    ArkLinkTarget target;
    ArkRuntimeConfig runtime;
    ArkLinkLogger logger;
} ArkLinkJob;

ArkLinkResult arklink_link(const ArkLinkJob* job);
const char* arklink_error_string(ArkLinkResult code);
```
- CLI 只需栈上填充 `ArkLinkJob`，交给库执行，日志通过回调输出。

## LinkContext 数据结构
- **Arena/Pool**：单一 `ArkArena` 负责对象与字符串，避免 `malloc/free` 分散；在[`ArkLink/src/core/mempool.c`](ArkLink/src/core/mempool.c)基础上扩展。
- **SectionBuffer**：抽象 `{ uint8_t* data; size_t size; size_t capacity; ArkSectionKind kind; ArkFlags flags; }`，存放 code/data/rodata/bss/tls，并记录对齐与特性。
- **SymbolTable**：
  - `ArkSymbol`（名称、段、偏移、大小、binding、visibility、import_id）。
  - `ArkSymbolTable` 维护哈希桶 + 动态数组，Resolver 统一管理别名、导入、未定义。
- **RelocationPlan**：
  - `ArkReloc`（section_id, offset, type, target_symbol, addend）。
  - Resolver 输出 “已绑定符号 + 重定位列表”，Backend 只需遍历。
- **ImportPlan**：
  - 抽象 Runtime/Import 配置，支持外部 JSON/INI，取消[`ArkLink/src/core/linker.c`](ArkLink/src/core/linker.c)中硬编码的 kernel32/msvcrt 列表。
- **LinkUnit**：Loader 输出 `{ sections[], symbols[], relocs[], entry }`；LinkContext 聚合多 Unit。
- **Lifecycle**：`ark_context_create(job)` -> 加载 unit -> 解析 -> Backend 写出 -> `ark_context_destroy()` 一次性释放 arena。

## 模块职责
1. **Loader (`loader_eo.c`)**
   - 解析 EO header/section/symbol/reloc，与 I/O 解耦。
   - 输出 `ArkLinkUnit`，并附带 `ArkDiagnostics`（文件/行/错误码）。
   - 单元测试覆盖：无效 magic、字符串表越界、不同 section 对齐等。

2. **Resolver (`resolver.c`)**
   - 合并所有 LinkUnit -> LinkGraph。
   - 处理别名、默认 runtime、导入匹配、未定义符号检查。
   - 构建 RelocationPlan + ImportPlan，生成入口点信息。
   - 负责 `ArkRuntimeConfig`（默认/外部配置）加载。

3. **Backend 接口 (`backend.h`)**
```c
typedef struct {
    ArkLinkTarget target;
    ArkResult (*prepare)(ArkContext* ctx, ArkBackendInput* in, ArkBackendState** out);
    ArkResult (*write_output)(ArkBackendState*, ArkBuffer* out);
    void (*destroy_state)(ArkBackendState*);
} ArkBackendOps;
```
   - Loader & Resolver 与 Backend 通过 `ArkBackendInput`（排好序的 sections/symbols/relocs/imports/entry）交互。

4. **PE Backend**
   - 拆分[`ArkLink/src/output/pe_writer.c`](ArkLink/src/output/pe_writer.c)中的 DOS/NT header、section table、IAT/TLS/BaseReloc 写出逻辑，移除 Windows API 调试分支。
   - 添加结构化日志，允许注入自定义 `ArkLinkLogger`。

5. **ELF Backend**
   - 基于现有[`ArkLink/src/output/elf_writer.c`](ArkLink/src/output/elf_writer.c)改写，满足 `ArkBackendOps`。
   - 规划未来 Mach-O backend 仅需新增目录与实现。

6. **CLI**
   - 解析参数 -> 填充 `ArkLinkJob` -> 调用 `arklink_link`。
   - 通过 Logger 回调实现 `--verbose` 与 `--quiet`。
   - 支持配置文件输入（与 future runtime/import 管理一致）。

## 内存与资源管理
- `ArkArena`：分层 chunk（例如 64 KB）链表，集中释放。
- `ArkStringPool`：驻留符号名/路径，减少重复。
- `ArkVector<T>`：统一动态数组 API，附带容量策略。
- 所有文件 I/O 通过 `ArkInput`/`ArkOutput` 抽象，易于切换到内存文件或测试桩。

## 实施步骤
1. **目录初始化**：建立 `include/ArkLink/{arklink.h, context.h, loader.h, resolver.h, backend.h}` 与 `src/core`/`src/backends` 框架，写 stub 以保持构建通过。
2. **LinkContext/Job 层**：实现 arena、字符串池、SectionBuffer、LinkJob -> LinkContext API，单测覆盖创建/销毁与字符串驻留。
3. **EO Loader 重写**：迁移旧 `read_eo_file`，输出 `LinkUnit`。编写针对 `.eo` 样例的单元测试（位于 `tests/loader_tests.c`）。
4. **Resolver + Runtime 管理**：
   - 建立符号哈希映射、别名策略。
   - 外部化 runtime/import 配置（JSON/INI），CLI 提供 `--runtime-config`。
   - 输出统一 `ArkBackendInput`。
5. **Backend 抽象 & PE/ELF 落地**：
   - 抽离 PE 写出逻辑（header、section、IAT、TLS、base reloc）。
   - 补齐 ELF 后端以生成 64-bit ELF 可执行文件。
6. **薄 CLI**：重写[`ArkLink/src/cli/main.c`](ArkLink/src/cli/main.c)使其只依赖 `arklink.h`，删除直接访问 `linker->sections` 等内部字段。
7. **测试矩阵**：
   - Loader/Resolver 单元测试。
   - Backend smoke tests：`tests/smoke/run_pe.sh`（默认 runtime + TLS + 重定位）、`tests/smoke/run_elf.sh`。
   - CLI 集成测试（借助 `build.py` 或 CI）。
8. **逐步迁移**：维持旧 `es_linker_*` API 的薄包装（调用新库）以兼容现有调用者，待全部迁移后弃用旧接口。

## 日志与错误处理
- `ArkLinkLogger`:
```c
typedef enum { ARK_LOG_ERROR, ARK_LOG_WARN, ARK_LOG_INFO, ARK_LOG_DEBUG } ArkLogLevel;
typedef void (*ArkLinkLogger)(ArkLogLevel level, const char* fmt, ...);
```
- LinkContext 统一 `ArkLinkError`，包含文件/符号/section 位置信息，CLI 仅输出。

## 后续扩展
- Mach-O Backend：沿用 `ArkBackendOps`。
- 并行 Loader：利用 LinkUnit 粒度并行读取。
- Cache：Arena/Vector 统计信息（类似旧 `stats`）用于调优。

---
该方案提供从架构拆分、数据结构到实施步骤的完整路线，便于逐模块替换现有实现并保持 EO 格式兼容。