# 使用指南

`wxvoice` 从本机微信 4.x 数据中定位**已收藏的语音**，在微信播放后只读扫描
`Weixin.exe` 进程内存中已解密的 SILK 载荷，解码并导出为 WAV，可选生成 MP3。
全程不联网、不注入进程、不写内存转储、不修改微信数据。

> **仅限本人设备。** 只应处理你本人账号在本机产生的数据；使用前请确认你有权
> 访问相关语音。工具会在扫描内存前打印一次提示。

## 适用范围

- 微信 4.x（进程名 `Weixin.exe`），Windows x64。
- 收藏语音在磁盘上的 `.silk` / `.silk_temp` 加密副本。
- 输出 WAV；安装 ffmpeg 后可同时输出 MP3。
- 不解密微信数据库，不导出文字、图片、视频或聊天记录。
- 不支持微信 3.x、macOS/Linux 版微信，也不提供批量导出或监听模式。

## 工作方式

1. `wxvoice list` 扫描数据根下的收藏临时文件，按密文 MD5 去重，显示短哈希、
   密文长度和预期明文区间。
2. 在微信中打开并播放目标收藏语音，使明文 SILK 载荷进入 `Weixin.exe` 内存。
3. `wxvoice export <序号|短哈希>` 自动扫描全部匹配的微信进程，按 SILK 帧结构
   校验候选，解码并写出 WAV；加 `--mp3` 时调用外部 ffmpeg 生成 MP3。

微信通常同时运行多个 `Weixin.exe` 进程。工具会自动扫描全部匹配进程并合并
候选，不需要手动指定 `--pid`。明文只在播放期间短暂驻留内存；未命中时可加
`--watch` 在播放期间持续轮询。

## 构建

需要 CMake 3.21+、支持 C11 的编译器、Ninja 和 Python 3（测试门禁）。
Windows 推荐 VS Code + MSYS2 UCRT64 + GCC，完整环境见
[environment.md](environment.md)。

```powershell
./scripts/bootstrap.ps1
cmake --preset ninja-release
cmake --build --preset ninja-release
```

CLI 位于 `build/ninja-release/bin/wxvoice.exe`。预设、Debug/Release/ASAN 与
常见 CMake 问题见 [cmake.md](cmake.md)。

## 1. 列出收藏语音

```powershell
./build/ninja-release/bin/wxvoice.exe list
```

默认依次尝试 Windows“文档”目录下的 `xwechat_files` 和
`%USERPROFILE%\Documents\xwechat_files`。也可以显式指定数据根或账号目录：

```powershell
wxvoice list --root "D:\WeChat\xwechat_files"
```

输出列包括序号、短哈希、密文字节数、预期明文区间、来源数量、持久副本标记和
一个代表性路径。若没有任何资产，退出码为 3。

## 2. 在微信中播放目标语音

解密后的 SILK 载荷只在播放或打开语音后出现在进程内存中。若工具提示
“未找到明文语音载荷”，请在微信中重新打开并播放该收藏语音；也可以在播放时
加 `--watch 30`，让工具在 30 秒内持续轮询全部匹配进程。

## 3. 导出

```powershell
wxvoice export 1 --out "D:\微信语音导出"
wxvoice export a1b2c3d4 --out "D:\微信语音导出" --mp3 --watch 30
```

常用选项：

| 选项 | 说明 |
|---|---|
| `--root <目录>` | 指定 `xwechat_files` 或账号目录 |
| `--out <目录>` | 输出目录，默认当前目录下的 `exports`；不得位于微信数据根内 |
| `--name <基名>` | 输出文件基名，不含扩展名 |
| `--mp3` | 同时生成 MP3，需要 ffmpeg |
| `--ffmpeg <路径>` | 指定 ffmpeg 可执行文件 |
| `--process <名称>` | 微信进程名，默认 `Weixin.exe` |
| `--pid <PID>` | 仅扫描指定进程；省略时自动扫描全部匹配进程 |
| `--watch <秒>` | 未命中时持续轮询进程，直到找到或超时（1-3600） |
| `--candidate <N>` | 存在多个音频不同的候选时选择第 N 个 |
| `--force` | 覆盖已存在的输出文件 |
| `--verbose` | 输出调试信息 |

默认文件名是 `<短哈希>.wav`；使用 `--candidate N` 时为
`<短哈希>-c<N>.wav`。目标文件已存在且未加 `--force` 时会拒绝覆盖。

WAV 为规范 RIFF/WAVE：单声道、16-bit PCM，采样率取自 SILK 流（微信语音通常
为 24 kHz）。MP3 使用 `libmp3lame -q:a 2`；ffmpeg 失败时 WAV 会保留，退出码为 6。

## 多个候选

不同进程或不同内存副本可能持有字节数略有差异、但解码后完全相同的 SILK 容器。
工具会先解码并合并音频等价的候选；只有剩余候选的音频确实不同时，才列出短哈希、
字节数、推测填充字节和时长，并要求用 `--candidate N` 明确选择。
`--candidate` 越界时不会写文件。

## 退出码

| 退出码 | 含义 |
|---|---|
| 0 | 成功 |
| 2 | 参数、路径、覆盖冲突或输出目录位于数据根内 |
| 3 | 未找到收藏语音资产 |
| 4 | 微信进程不存在、无候选、轮询超时或候选歧义 |
| 5 | SILK 解码或文件写入失败 |
| 6 | MP3 编码失败，WAV 已保留 |

错误信息写标准错误；列表与成功信息写标准输出。

## 限制

- 依赖微信 4.x 当前的内存布局；微信升级后扫描可能失效，但不会修改数据。
- 必须在微信播放过目标语音后才能取得明文；工具不会自动操作微信。
- 进程内存读取可能被安全软件拦截；工具不会尝试提权。
- 仅验证 Windows x64；其他平台的构建用于保证核心逻辑可移植。
- SILK SDK 使用 BSD 风格许可，但 SILK 编解码器存在独立专利声明。分发前请
  自行复核许可与专利约束。

## 安全与隐私

- 只处理本人设备上的数据。
- 只读扫描进程名匹配的微信进程；不联网、不生成内存转储、不修改微信数据。
- 输出目录不得位于微信数据根内，包含符号链接解析后的路径。
- 采集仅支持 Windows；不请求管理员权限，不注入进程。

完整边界与漏洞报告方式见 [SECURITY.md](../../SECURITY.md)。
