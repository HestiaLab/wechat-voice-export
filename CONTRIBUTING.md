# 贡献指南

本仓库使用聚焦、可评审的变更。

## 本地检查

提交前必须执行完整的本地检查（格式检查 + 配置 + 构建 + 测试）。检查命令不会自动改写源码，格式不符合时会失败。

**Linux/macOS**

```sh
./scripts/bootstrap.sh
./scripts/check.sh
```

**Windows PowerShell**

```powershell
./scripts/bootstrap.ps1
./scripts/check.ps1
```

提交前运行包含 clang-tidy 的完整质量门禁：

**Linux/macOS**

```sh
./scripts/check.sh --enable-tidy
```

**Windows PowerShell**

```powershell
./scripts/check.ps1 -EnableTidy
```

需要显式修复 C 源码和头文件格式时，执行：

```sh
./scripts/check.sh --fix-format        # Linux/macOS
./scripts/check.ps1 -FixFormat         # Windows
```

只想单独检查或格式化 C 文件、不跑 configure/build/test 时，用专用的格式化脚本：

```powershell
./scripts/format.ps1 -Check             # 仅检查；发现违规时退出码 2
./scripts/format.ps1 -Format            # 就地改写
```

`-SkipFormat` / `--skip-format` 仅用于临时诊断，不满足提交前检查要求。CI 会重新执行不带跳过参数的完整质量门禁。`clang-format` 不处理 Markdown；文档改动请检查排版、相对链接和 `git diff --check` 结果。`tests/vendor/` 下的第三方 vendored 代码不参与格式与静态分析检查。

首次检出或换新机器时，先运行 bootstrap。如果尚未安装所需工具，参见 [docs/guides/environment.md](docs/guides/environment.md) 中的 Windows 开发环境配置说明。

关于配置、构建、测试、安装、预设和常见 CMake 故障的详细说明，参见 [docs/guides/cmake.md](docs/guides/cmake.md)。

## 分支

使用短分支名：

- `feature/<topic>`
- `fix/<topic>`
- `refactor/<topic>`
- `infra/<topic>`
- `release/<version>`

## 提交与 PR

尽量使用 Conventional Commit 风格：

```text
fix(parser): reject invalid delimiters
feat(cli): add version command
refactor(build): share test target helper
```

打开 PR 前：

- 运行覆盖本次改动的最小本地检查。
- 填写 PR 模板中适用的章节。
- 说明行为、兼容性或发布影响。
- 为行为变更补充测试。
- 改动影响构建、测试、安装、发布、安全或公共 API 时，链接相关文档。
- 不把无关清理混入 feature / fix PR。

使用 issue 模板提交 Bug 报告、功能提议和基础设施维护请求。

## 架构

本仓库文档只维护一种语言：中文。上游派生项目如需要英文文档，请自行翻译，不要在本仓库重新引入双语镜像。

- 公共头保持小且稳定。
- 实现细节不进入 `include/`。
- 显式 ownership 与清理路径。
- 抽象只在重复使用证明其价值后引入。
- 不引入网络请求、内存转储或对微信数据的写操作；详见
  [SECURITY.md](SECURITY.md) 与 [docs/requirements.md](docs/requirements.md)。
- 测试只使用合成 SILK 数据，不提交真实语音、账号路径或密文哈希。

### 目录结构

```text
include/wechatvoiceexport/   公共 C API
src/wechatvoiceexport/       核心实现（资产、采集、候选选择、SILK、WAV、流水线）
src/main.c                   wxvoice CLI
third_party/silk/            vendored Skype SILK SDK（仅解码所需源码）
tests/                       Unity 测试与合成 SILK fixture
cmake/                       CMake 模块
scripts/                     引导、检查、静态分析与 CLI 冒烟脚本
docs/                       需求、指南与 ADR
```

核心模块：

- `asset.c`：数据根定位、账号发现、收藏资产扫描与选择。
- `capture.c`：SILK 候选提取；Windows 进程内存只读扫描。
- `selection.c`：跨进程候选的音频等价合并。
- `silk.c`：SILK v3 容器解析与 PCM 解码。
- `wav.c`：RIFF/WAVE 写入。
- `pipeline.c`：解码、目录校验、WAV/MP3 输出与结果所有权。
- `fs.c` / `md5.c` / `error.c`：私有文件系统、哈希与状态字符串。

测试分层、静态分析与架构门禁见 [docs/guides/testing.md](docs/guides/testing.md)。
新增源文件时同步更新 [cmake/Sources.cmake](cmake/Sources.cmake) 和
[cmake/Tests.cmake](cmake/Tests.cmake)。
