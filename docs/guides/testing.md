# 测试

使用 CTest 运行自动化测试：

```sh
ctest --preset ninja-debug --output-on-failure
```

预设与构建命令见 [cmake.md](cmake.md)。

## 测试框架

测试使用 vendored 的 [Unity](https://github.com/ThrowTheSwitch/Unity)（MIT，
位于 `tests/vendor/unity/`）。每个可执行文件在 `UNITY_BEGIN()` / `UNITY_END()`
之间用 `RUN_TEST()` 注册用例，并提供 `setUp()` / `tearDown()`。vendored 代码
不参与格式与静态分析。

## 测试清单

| CTest 名称 | 覆盖内容 |
|---|---|
| `wechatvoiceexport_decl_def_check` | `include/` 的每个公共函数声明在 `src/` 有定义 |
| `wechatvoiceexport_gate_behaviors` | 质量门禁脚本自身可失败（Python unittest） |
| `wechatvoiceexport_scalar_tests` | 整数宽度、溢出与有符号性假设 |
| `wechatvoiceexport_md5_tests` | RFC 1321 向量、流式一致性、含 NUL 二进制 |
| `wechatvoiceexport_silk_tests` | 24 kHz / 8 kHz 解码、9/10 字节头、截断与非法输入 |
| `wechatvoiceexport_capture_tests` | 载荷提取、跨扫描候选合并与去重、块边界、截断帧、非法头 |
| `wechatvoiceexport_selection_tests` | 音频等价合并、不同音频保留、解码失败候选与候选上限 |
| `wechatvoiceexport_wav_tests` | 规范 44 字节头、字段值、边界参数 |
| `wechatvoiceexport_asset_tests` | 数据根解析、账号发现、资产去重、选择器、持久副本 |
| `wechatvoiceexport_pipeline_tests` | WAV 写出、覆盖保护、输出包含检查、MP3 失败保留 WAV |
| `wechatvoiceexport_cli_smoke` | `--help`、`--version`、空资产、缺参数、`--watch` 参数校验与未知命令的退出码 |
| `wechatvoiceexport_version_consistency` | CMake 版本、生成头、CLI `--version` 与 CHANGELOG 一致 |

`ctest -N` 可查看当前注册的测试。构建目录中的测试临时文件位于
`build/<preset>/ctest-tmp`，不会写入源码树。

## 测试支持

- `tests/support/silk_fixture.c`：用 vendored SILK 编码器在内存中生成 440 Hz
  合成语音容器，支持 9 字节或 10 字节头。测试不依赖真实语音。
- `tests/support/test_util.c`：在 `TEST_TMP_ROOT` 下创建/清理临时目录，写入
  测试文件。
- 假数据根：`test_asset.c` 构造 `account_wxid/business/...` 目录树和假密文；
  `test_pipeline.c` 使用合成 SILK 载荷验证完整解码与输出路径。

## 测试策略

- 缺陷修复应附带回归测试。
- 新增公共 API 应覆盖成功、失败与边界输入。
- 数值与长度计算应覆盖溢出和截断。
- 断言可观察行为，而不是实现细节或覆盖率数字。
- 涉及微信真实数据的验证不进入自动化测试，只保留手工验收。

## 静态分析

```powershell
./scripts/check.ps1 -EnableTidy
./scripts/static_analysis.sh format-check
./scripts/static_analysis.sh tidy
./scripts/static_analysis.sh cppcheck
```

Linux/macOS：

```sh
./scripts/check.sh --enable-tidy
./scripts/static_analysis.sh format-check
./scripts/static_analysis.sh tidy
./scripts/static_analysis.sh cppcheck
```

`clang-tidy` 使用 `build/ninja-debug/compile_commands.json`；`cppcheck` 使用
`--std=c11 --enable=warning,performance,portability`。CI 以
`--require-tools` 运行，缺失工具视为失败；本地默认跳过缺失工具。

YAML 工作流可用 `yamllint .github .yamllint.yml` 检查。

## 需要运行的检查

| 变更类型 | 最低检查 |
|---|---|
| C 源码或公共头文件 | `./scripts/check.ps1` 或 `./scripts/check.sh` |
| 采集或 SILK 解析 | 上述检查 + `ninja-asan`（受支持平台） |
| CLI 行为或退出码 | 上述检查 + `scripts/test_cli_smoke.py` |
| CI/YAML | `yamllint` + `git diff --check` |
| 发布前 | `ninja-debug`、`ninja-release`、`ninja-asan` |

## 手工验收

真实微信数据无法进入 CI，以下项目必须在本机手工确认：

1. `wxvoice list` 能看到目标收藏语音，且同内容只出现一次。
2. 在微信播放目标语音后，`wxvoice export` 自动扫描全部 `Weixin.exe` 进程，
   输出完整时长与采样率的 WAV；本次目标为 46560 样本（1.94 秒）。
3. 多个进程持有字节数略有差异的容器时，音频等价的候选自动合并；只有音频
   不同才要求 `--candidate N`。
4. 未播放时加 `--watch 30` 能持续轮询；超时以退出码 4 结束。
5. `--mp3` 生成可播放的 MP3；ffmpeg 不可用时 WAV 保留、退出码为 6。
6. 导出目录不在微信数据根内；运行前后微信数据文件哈希不变。
7. 由用户亲耳确认导出的音频就是目标语音（2026-10-22 已通过）。

手工检查应记录命令、Windows 版本、微信版本、ffmpeg 版本与观察结果，不要
在记录中粘贴真实语音内容或账号路径。
