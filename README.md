# **E#**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg?style=for-the-badge)](https://opensource.org/licenses/MIT)
[![Stars](https://img.shields.io/github/stars/AetherStudioTeam/E_Sharp_Lang?style=for-the-badge&logo=github&color=blue)](https://github.com/AetherStudioTeam/E_Sharp_Lang/stargazers)
[![Forks](https://img.shields.io/github/forks/AetherStudioTeam/E_Sharp_Lang?style=for-the-badge&logo=github&color=orange)](https://github.com/AetherStudioTeam/E_Sharp_Lang/network/members)
[![Issues](https://img.shields.io/github/issues/AetherStudioTeam/E_Sharp_Lang?style=for-the-badge&logo=github&color=red)](https://github.com/AetherStudioTeam/E_Sharp_Lang/issues)
[![Last Commit](https://img.shields.io/github/last-commit/AetherStudioTeam/E_Sharp_Lang?style=for-the-badge&logo=git&color=green)](https://github.com/AetherStudioTeam/E_Sharp_Lang/commits)
[![Code Size](https://img.shields.io/github/languages/code-size/AetherStudioTeam/E_Sharp_Lang?style=for-the-badge&logo=github&color=purple)](https://github.com/AetherStudioTeam/E_Sharp_Lang)
[![C#](https://img.shields.io/badge/C%23-239120?style=for-the-badge&logo=c-sharp&logoColor=white)](https://learn.microsoft.com/en-us/dotnet/csharp/)
[![Made by Aether](https://img.shields.io/badge/Made%20by-Aether%20Studio-00d4aa?style=for-the-badge)](https://www.aetstudio.xyz)

E# 是一个基于C#语法扩展的编程语言，由AetherStudio开发。项目在标准C#基础上增加了多元运算符等语言特性，并通过自研编译器toolchain实现。
> 一个初中生团队写的C#扩展语言，编译速度比你想的要快*亿*点点

## 特性

- **语法兼容**：基于C#语法，降低学习成本
- **自研编译器**：独立实现的前端、SSA形式的IR及后端代码生成

## 快速开始

### 环境要求

- Python 3.x

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
- RedstLauncher - Minecraft启动器

### 联系
- 邮箱：AetherStudio@qq.com
- QQ 群：791809691
- 官网：https://es.aetstudio.xyz/
- GitHub：https://github.com/AetherStudioTeam/E_Sharp_Lang

## 致谢

### 技术参考
- LLVM SSA 设计理念
- C# 语法
- GCC生成的asm代码
- 其他开源项目的灵感和代码片段

### 贡献者
&lt;a href="https://github.com/AetherStudioTeam/E_Sharp_Lang/graphs/contributors"&gt;
  &lt;img src="https://contrib.rocks/image?repo=AetherStudioTeam/E_Sharp_Lang" /&gt;
&lt;/a&gt;

### 以及...
&gt; BYD yukifuri！！！！！（气）
