# 更新日志

> 新增段落用中文书写；已发布段落保留发布时的原文，不重写。

## 0.1.0 — 2026-10-22

- 首个可用版本：`wxvoice list` 与 `wxvoice export`。
- 发现微信 4.x 收藏语音的 `.silk` / `.silk_temp` 资产，按密文 MD5 去重。
- Windows 下只读扫描 `Weixin.exe` 内存，按 SILK 帧结构校验并去重候选。
- 存在多个 `Weixin.exe` 进程时自动扫描全部匹配进程，无需手动指定 `--pid`。
- 新增 `--watch <秒>`：在微信播放语音期间持续轮询进程，直到命中或超时。
- 候选按解码 PCM 合并音频等价的副本；只有音频不同才要求 `--candidate N`。
- 实测基线校正为 46560 样本（1.94 秒）；先前 1.92 秒版本少一个 20 ms 尾帧。
- 集成 vendored Skype SILK SDK，解码 24 kHz 单声道 16-bit PCM。
- 写出规范 RIFF/WAVE；可选调用 ffmpeg 生成 MP3。
- 输出目录包含检查、覆盖保护、候选歧义处理与明确退出码。
- 合成 SILK fixture、Unity 单元测试、CLI 冒烟测试与架构门禁。
- CMake 预设、clang-format/clang-tidy/cppcheck 检查与 GitHub Actions。
- 中文需求规格、构建指南、安全边界与 ADR。
