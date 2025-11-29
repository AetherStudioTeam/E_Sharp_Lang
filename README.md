# E#----编程语言
E#是一个由初中生团队独立研发的编程语言，目标是解决部分c sharp痛点
我们的文档：[esdocs.md](esdocs.md) [esdocs2.0.md](esdocs2.0.md) （当然由于一些原因，这俩文档是ai编写的，绝对不是因为我们爱偷懒）


# 快速开始
构建E#编译器(使用cradle,当然,前置是GCC或CMAKE,对这个工具有兴趣的可以去看我们的仓库:[Cradle](https://github.com/AetherStudioTeam/Cradle))
```bash
./cradle.exe
```
构建开始的时候cradle会自动读取根目录的cradle.config.json文件,你可以在这个文件中配置编译选项
当你想创建release版本的编译器时,可以通过：
```bash
./cradle.exe --release
```
进行编译
编译完成后,你可以在bin目录下找到编译好的e_sharp.exe文件，这是一个cli控制台程序
linux用户？可以通过我们的另外的构建脚本进行编译
```bash
python build.py
```
release版本同理,你可以通过：
```bash
python build.py --release
```
进行编译

# 提交pr
当你完成了某个功能或修复了某个bug,你可以提交一个pull request到我们的仓库,我们会 review 你的代码并合并到主分支中

# 贡献
我们欢迎所有形式的贡献,包括但不限于：
- 提交bug报告
- 建议新功能
- 改进文档
- 提交代码修复或新功能实现

# 许可证
GPL v3.0

# 关于我们
我们是一个初中生团队，拥有三四年的历史了
我们的成员包括但不限于：
- duoduo （可能是前后端结合）
- SteveHappy （lumen code主力开发）
- PUTSIX25565 （aether ai主力开发&e sharp主力开发）
- 部分未上榜成员，均为保密项目开发者，也欢迎您加入我们！

我们主要在维护的项目：
- E# 编译器
- LumenCode公益游戏项目
- AetherAI(一个人工智能项目,分支包含Code,Small,Large等等版本)
- 我们的官网（gh链接）：https://github.com/AetherStudioTeam/AetherStudioWebSite
- 当然你也可以访问我们在服务器上搭建的官网：[AetherStudio](https://aetstudio.xyz)

# 加入我们
如果您对我们的项目感兴趣，欢迎加入我们的团队！您可以通过以下方式加入我们：
- 加入我们的QQ群：791809691
- 发送邮件到：[aetherstudio@qq.com](mailto:aetherstudio@qq.com)

# 联系我们
如果您有任何问题、建议或合作机会，请随时联系我们。您可以通过以下方式联系我们：
- 邮箱：[aetherstudio@qq.com](mailto:aetherstudio@qq.com)
- QQ群：791809691