WeChat 收藏语音导出工具（wxvoice）
================================

`wxvoice` 是一个 Windows 命令行工具，用于把微信 4.x 中**已收藏的语音**导出为
可直接播放的 WAV，可选生成 MP3。它读取本地数据目录，在微信播放后只读扫描
`Weixin.exe` 进程内存中已解密的 SILK 载荷，再解码为 PCM；全程不联网、不注入
进程、不写内存转储、不修改微信数据。

Quick Start
-----------

```powershell
cmake --preset ninja-release
cmake --build --preset ninja-release
build\ninja-release\bin\wxvoice.exe list
build\ninja-release\bin\wxvoice.exe export 1 --out "D:\微信语音导出" --mp3 --watch 30
```

在 `export` 前，请先在微信中打开并播放目标收藏语音。构建前提、完整参数、候选
规则与退出码见 [使用指南](docs/guides/usage.md)。

开始之前必读
-------------

- 仅处理本人设备上的数据；使用前确认你有权访问相关语音。
- 只读扫描进程名匹配的微信进程（默认 `Weixin.exe`），不注入、不写数据。
- 不发起网络请求，不生成内存转储或临时采集文件。
- 输出目录不得位于微信数据根内，判定包含符号链接解析后的路径。
- 采集仅支持 Windows；SILK SDK 使用 BSD 风格许可，但编解码器存在独立专利声明。

详细边界与漏洞报告见 [SECURITY.md](SECURITY.md)。

Who Are You?
============

使用者
------

- 日常操作与示例：[使用指南](docs/guides/usage.md)
- 构建、预设与排错：[CMake 指南](docs/guides/cmake.md)
- 安全边界：[SECURITY.md](SECURITY.md)

开发者
------

- 贡献流程与目录结构：[CONTRIBUTING.md](CONTRIBUTING.md)
- 开发环境：[环境配置](docs/guides/environment.md)
- 编码风格：[编码约定](docs/guides/coding-style.md)
- 测试与质量门禁：[测试指南](docs/guides/testing.md)
- 需求规格：[需求文档](docs/requirements.md)
安全与维护者
------------

- 安全政策：[SECURITY.md](SECURITY.md)
- 架构决策：[ADR 索引](docs/adr/README.md)
  - [0001 C11 与 CMake 工具链策略](docs/adr/0001-c11-cmake-toolchain-policy.md)
  - [0002 只读进程内存采集](docs/adr/0002-read-only-memory-capture.md)
- 变更记录：[CHANGELOG.md](CHANGELOG.md)
- AI 代理约束：[AGENTS.md](AGENTS.md)

许可证
------

MIT。`third_party/silk/` 与测试框架保留各自的上游许可，详见 [LICENSE](LICENSE)
与对应目录。
