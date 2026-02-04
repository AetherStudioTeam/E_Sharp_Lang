# E#

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

E# 是一个基于C#语法扩展的编程语言，由AetherStudio开发。项目在标准C#基础上增加了多元运算符等语言特性，并通过自研编译器toolchain实现。

## 特性

- **语法兼容**：基于C#语法，降低学习成本
- **自研编译器**：独立实现的前端、SSA形式的IR及后端代码生成

## 快速开始

### 环境要求

- Python 3.x
- 支持的C#运行时环境（用于自举或对比测试）

### 构建

```bash
# 开发构建
python build.py
```
# 发布构建
```bash
python build.py --release
```

## 项目结构

- `./ESC/`：编译器源代码
- `./ArkLinker/`：链接器
- `./sp/`：LSP(未完全实现)

### 参与贡献

## 欢迎通过以下方式参与项目：
- 提交 Issue 报告问题
- 提交 Pull Request（请参考现有代码风格）
- 改进文档或测试用例
- 提交规范
- 使用清晰的提交信息描述变更
- 确保代码通过现有测试
- 新增功能请附带测试用例

### 许可证
本项目采用 MIT License 开源。

### 相关项目
- LumenCode - 非商业音游项目
- AsticAI - AI 相关工具集

### 联系
- 邮箱：AetherStudio@qq.com
- QQ 群：791809691
- 官网：https://es.aetstudio.xyz/
- GitHub：https://github.com/AetherStudioTeam/E_Sharp_Lang

### 特别鸣谢
- 所有贡献者和用户
- 我的一些朋友
