# ArkLink 代码库状态分析

> 生成时间: 2026-02-11

## 1. 架构概览

ArkLink 已完成重构设计草案中规划的核心架构拆分，形成了以下模块化结构：

```
ArkLink/
├─ include/ArkLink/          # 公共接口
│  ├─ arklink.h              # 顶层 API: ArkLinkJob, arklink_link()
│  ├─ context.h              # Arena, SectionBuffer, 字符串池
│  ├─ loader.h               # ArkLinkUnit, ArkSymbolDesc, ArkRelocationDesc
│  ├─ resolver.h             # ArkResolverPlan, ArkImportBinding
│  ├─ backend.h              # ArkBackendOps 接口 (prepare/write/destroy)
│  └─ targets/{pe,elf}.h     # 后端特定配置
├─ src/
│  ├─ core/
│  │  ├─ context.c           # Arena, 字符串池, SectionBuffer 实现
│  │  ├─ job.c               # arklink_link() 主流程 (load → resolve → backend)
│  │  ├─ loader_eo.c         # EO v1 格式解析 → ArkLinkUnit
│  │  ├─ resolver.c          # 符号去重、未定义检查、导入计划、重定位收集
│  │  └─ backend.c           # 后端注册表
│  ├─ backends/
│  │  ├─ pe/pe_backend.c     # PE64 完整实现 (1083行)
│  │  └─ elf/elf_backend.c   # ELF64 基础实现 (769行)
│  └─ cli/
│     ├─ main.c              # CLI 入口, 仅依赖 arklink.h
│     ├─ config.c/h          # JSON 配置解析
└─ tests/
   ├─ loader_tests.c         # Loader 单元测试
   ├─ resolver_tests.c       # Resolver 单元测试
   └─ smoke/{pe,elf}_runtime.sh
```

### 主流程 (`job.c`)

```
arklink_link(ArkLinkJob)
  → ark_backend_register_all()
  → ark_context_create(job)
  → for each input:
      ark_loader_load_unit() → ArkLinkUnit
  → ark_resolver_resolve(units) → ArkResolverPlan
  → arklink_run_backends(plan)
      → ops->prepare() → ArkBackendState
      → ops->write_output()
      → ops->destroy_state()
  → cleanup
```

## 2. 各模块当前状态

### 2.1 EO Loader (`loader_eo.c`) — ✅ 完成

- 解析 EO v1 格式 (magic `0x454F4C46`)
- 支持任意数量的 section、symbol、relocation
- 字符串通过 `ark_context_intern()` 驻留
- 完善的错误处理和诊断信息

**⚠️ 关键问题**: ESC 编译器使用不同的 EO 格式！
- ESC `eo_writer.h`: magic = `0x424F2345`, version 2, 固定 5 个 section 槽位
- ArkLink `loader_eo.c`: magic = `0x454F4C46`, 可变 section 数量
- 两者的 section/symbol/reloc 结构也完全不同
- 需要编写 EO v2 loader 或统一格式

### 2.2 Resolver (`resolver.c`) — ✅ 基本完成

- FNV-1a 哈希符号表去重
- 多全局定义冲突检测
- 未定义符号检查（import 符号跳过）
- 导入计划构建（**硬编码 "kernel32.dll"**）
- 入口点查找（main/_start/WinMain/wmain 或 unit 指定）
- 生成 `ArkBackendInput`

**待改进**:
- `ark_resolver_build_import_plan()` 硬编码 module 名为 "kernel32.dll"
- 缺少从 EO 文件获取实际 DLL 名的能力
- 缺少弱符号支持
- 多目标文件合并时 section 索引映射不够健壮

### 2.3 PE Backend (`pe_backend.c`) — ✅ 生产可用

- 1083 行完整实现
- `#pragma pack(push, 1)` + `_Static_assert` 保证结构体大小
- DOS Header → NT Header → Section Table → .text/.data/.idata/.reloc
- IAT/ILT 导入表完整生成
- BaseReloc 表有独立的 `.reloc` 节
- 重定位应用（ADDR64 → DIR64, ADDR32 → HIGHLOW）
- 已验证：`ExitProcess(42)` 可正确执行

### 2.4 ELF Backend (`elf_backend.c`) — ⚠️ 基础骨架

- 769 行实现
- ELF64 header、program headers、section headers 基本正确
- 支持 LOAD 段 (per-section)
- **缺失功能**:
  - 没有 relocation 应用
  - 没有 GOT/PLT
  - 没有 .dynamic 段（已有数据结构但未生成）
  - 没有 .interp 段（计数了但未写入）
  - 没有符号表（.symtab / .strtab 预留但未填充）
  - 没有 syscall wrapper（直接调用）
  - 所有 section 各自一个 LOAD 段（应合并同权限段）
  - `#pragma pack` 缺失（与 PE 相同的对齐问题可能存在）

### 2.5 CLI (`main.c`) — ✅ 完成

- 参数解析（--target, -o, --runtime, --import-config, --config-json, --config-file, --verbose, --quiet）
- 配置文件加载（JSON 解析）
- Logger 回调
- 仅依赖公共 API

### 2.6 配置系统 (`config.c/h`) — ✅ 完成

- JSON 配置解析
- 支持 runtime_name, subsystem, entry_point, image_base, stack_size
- 导入列表 (module/symbol/slot)

## 3. 关键差距分析

### 3.1 🔴 ESC ↔ ArkLink EO 格式不兼容

这是**最严重的集成障碍**。两个项目使用不同的 EO 对象文件格式：

| 特性 | ESC (`eo_writer.h`) | ArkLink (`loader_eo.c`) |
|------|---------------------|------------------------|
| Magic | `0x424F2345` | `0x454F4C46` |
| Version | 2 | 无版本检查 |
| Section 布局 | 固定 5 个槽 (code/data/rodata/bss/tls) | 可变数量 |
| Section Header | `EOSectionInfo` (8B, 含 align_log2) | `ArkEOSectionHeader` (20B, 含 name_len) |
| Symbol | `EOSymbol` (位域, name_offset 到字符串表) | `ArkEOSymbolHeader` (内联 name) |
| Relocation | `EORelocation` (12B, 含 addend_shift) | `ArkEORelocHeader` (20B) |
| String Table | 全局字符串表 | 无（名称内联） |

**解决方案**: 需要在 ArkLink 中实现 EO v2 loader 来兼容 ESC 输出。

### 3.2 🔴 Resolver 导入模块硬编码

```c
// resolver.c:263
imp->module = "kernel32.dll"; /* Would be resolved from import config */
```

这意味着所有导入符号都被假定来自 kernel32.dll。实际上 ESC 编译器的程序会使用多个 DLL（kernel32, msvcrt 等）。

**解决方案**: 
1. 在 EO 格式中编码导入模块信息
2. 或通过外部配置文件（JSON）指定导入映射
3. 或利用 `ArkRuntimeConfig` 传递导入配置

### 3.3 🟡 ELF 后端不完整

缺少 relocation 应用、GOT/PLT、.dynamic、符号表等关键功能，无法生成可运行的 ELF 可执行文件。

### 3.4 🟡 多文件链接未验证

Resolver 支持多 unit，但 section 索引在合并时可能有问题（各 unit 的 section 索引是独立的，合并后需要重新映射）。

## 4. 高优先级任务实施计划

### 任务 1: EO v2 Loader（编译器集成）

**优先级**: 🔴 最高  
**依赖**: 无  
**工作量**: ~300 行

在 `loader_eo.c` 中添加 EO v2 格式支持，使 ArkLink 能加载 ESC 编译器输出的 .eo 文件。

关键步骤:
1. 读取文件头，根据 magic 区分 v1/v2 格式
2. 实现 `ark_loader_parse_eo_v2()`:
   - 解析 `EOHeader` (固定 5 section 槽)
   - 读取各 section 数据 (按 code/data/rodata/bss/tls 顺序)
   - 解析字符串表
   - 解析 `EOSymbol` 数组（将 name_offset 解析为实际名称）
   - 解析 `EORelocation` 数组
3. 将 v2 数据转换为 `ArkLinkUnit` 统一格式

### 任务 2: 导入配置外部化

**优先级**: 🔴 高  
**依赖**: 任务 1  
**工作量**: ~150 行

将 resolver 中硬编码的 "kernel32.dll" 替换为可配置的导入映射。

关键步骤:
1. 扩展 JSON 配置支持导入映射
2. 在 resolver 中使用配置的导入信息
3. 支持 EO v2 中的导入元数据（如果有的话）
4. 默认提供 E# 运行时标准导入映射

### 任务 3: ELF 后端完善

**优先级**: 🟡 中  
**依赖**: 无  
**工作量**: ~400 行

关键步骤:
1. 添加 `#pragma pack` 和 `_Static_assert`
2. 实现 relocation 应用
3. 合并同权限段为单个 LOAD 段
4. 填充 .symtab / .strtab
5. 实现 syscall 机制（Linux exit 等）

### 任务 4: 符号导出/导入控制

**优先级**: 🟡 中  
**依赖**: 任务 2  
**工作量**: ~200 行

扩展 resolver 支持:
- 符号可见性 (default/hidden)
- 导出符号表
- __declspec(dllexport) 等效标记
- PE 导出表生成

### 任务 5: 静态库支持

**优先级**: 🟡 中  
**依赖**: 任务 1  
**工作量**: ~500 行

支持 .lib/.a 归档文件:
1. COFF/AR 格式解析
2. 符号索引查询
3. 按需加载成员
4. 与 resolver 集成

## 5. 建议实施顺序

```
Phase 1 (编译器集成):
  1. EO v2 Loader          ← 使 ArkLink 能加载 ESC 输出
  2. 导入配置外部化         ← 解除 "kernel32.dll" 硬编码

Phase 2 (功能完善):
  3. ELF 后端完善           ← 支持 Linux 平台
  4. 符号导出/导入控制       ← 精细化符号管理

Phase 3 (生态扩展):
  5. 静态库支持             ← 链接第三方 .lib/.a
  6. 调试信息支持           ← DWARF/PDB
  7. 动态库生成             ← .dll/.so
```

## 6. 立即可以开始的任务

最建议立即开始的是**任务 1: EO v2 Loader**，因为：
- 这是 ESC 编译器与 ArkLink 链接器集成的关键路径
- 不依赖其他任务
- 实现相对独立，改动范围小（主要在 `loader_eo.c`）
- 完成后可以直接用 ESC 编译的 .eo 文件测试 PE 后端
