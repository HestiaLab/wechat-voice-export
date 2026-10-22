# AGENTS.md

本文件是 AI 代理和自动化贡献者的入口。开始修改前先阅读 README 与需求规格。

## 项目

`wechat-voice-export` 是一个 Windows C11 命令行工具 `wxvoice`，用于把微信 4.x
中已收藏的语音导出为 WAV/MP3。核心流程是：发现加密收藏资产 → 只读扫描
`Weixin.exe` 内存中的明文 SILK 载荷 → 解码 → 写 WAV（可选 MP3）。

需求基线：[docs/requirements.md](docs/requirements.md)。

## 不可违反的安全边界

- 不引入网络请求、网络库或遥测。
- 不写内存转储、临时采集文件或包含载荷的日志。
- 微信数据文件只读，绝不修改。
- 只扫描进程名匹配的微信进程（默认 `Weixin.exe`）或 `--pid` 指定的进程，
  不扫描其他进程，不注入、不提权。
- 输出目录不得位于微信数据根内（解析符号链接后判定）。
- 不把真实语音、账号路径、wxid、密文哈希或个人数据写入测试、文档或提交。
- 测试使用 `tests/support/silk_fixture.c` 生成合成 SILK 数据。

## 构建与检查

```powershell
./scripts/bootstrap.ps1
./scripts/check.ps1                 # 格式 + configure + build + ctest
./scripts/check.ps1 -EnableTidy     # 额外 clang-tidy
```

```sh
ctest --preset ninja-debug --output-on-failure
./scripts/static_analysis.sh format-check
./scripts/static_analysis.sh tidy
./scripts/static_analysis.sh cppcheck
```

提交前至少运行 `scripts/check.ps1`（Windows）或 `scripts/check.sh`。
`ninja-release` 用于发布前验证；`tests/vendor/` 与 `third_party/silk/` 不参与
格式与静态分析。

## 代码约定

- C11，`-std=c11`，无编译器扩展；公共符号前缀 `wechatvoiceexport_`。
- 公共头文件在 `include/wechatvoiceexport/`，私有头文件留在 `src/`。
- 公共函数必须有 Doxygen 契约；每个公共声明必须在 `src/` 中有定义。
- 新增 `.c` 时同步更新 `cmake/Sources.cmake`；新增测试时同步更新
  `cmake/Tests.cmake`。
- Windows 头文件顺序：`windows.h` 先于 `tlhelp32.h` / `shellapi.h` /
  `shlobj.h`，并在 `windows.h` 后留空行，避免 clang-format 重排。
- 不要手工修改 vendored 代码或生成文件。
- 注释只解释“为什么”；不自解释代码，不加任务编号或变更日志式注释。

## 修改检查

- 采集或 SILK 解析改动：运行完整 CTest，优先在支持 ASan 的平台跑
  `ninja-asan`。
- CLI/退出码改动：更新并运行 `scripts/test_cli_smoke.py` 对应的 CTest。
- 路径/输出改动：保持 `test_pipeline` 的数据根包含检查通过。
- 安全边界改动：必须同步更新 README、`docs/requirements.md` 与 SECURITY.md。

## 文档

文档语言为中文。文档索引在 README.md；相对链接必须可达。不要创建模板性的
占位文档或未实现功能的指南。
