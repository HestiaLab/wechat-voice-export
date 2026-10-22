# Skype SILK SDK（vendored）

本目录只保留 SILK v3 解码所需的 SDK 源码。

| 项 | 值 |
|---|---|
| 上游 | https://github.com/kn007/silk-v3-decoder |
| 上游提交 | `507be6bca8ce1fb977a061481f1d79e8c610e309`（2025-01-22） |
| 复制范围 | `silk/interface/` 与 `silk/src/`，逐字复制 |
| 未复制 | `silk/test/`、`silk/Makefile`、Windows 预编译产物 |
| 本地修改 | 无 |
| 许可 | Skype Limited 的 BSD 风格许可，见 `LICENSE`；其中明确不授予专利许可 |

`src/` 同时包含编码器与解码器源码，以保持上游文件完整；本项目只调用
`SKP_Silk_SDK_Get_Decoder_Size`、`SKP_Silk_SDK_InitDecoder` 与
`SKP_Silk_SDK_Decode`。

构建由 `cmake/ThirdParty.cmake` 中的 `silk_decoder` 静态库负责。该目标关闭
自身警告，并排除在项目的 clang-format、clang-tidy、cppcheck 扫描面之外。

上游仓库顶层另有 MIT 许可的封装脚本；本目录只使用 SILK SDK 源码，不包含
这些脚本。
