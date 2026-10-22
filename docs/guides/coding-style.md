# 编码规范

本指南记录本项目的 C 编码约定。编译器标志与 `.clang-format` / `.clang-tidy`
是可强制执行的基线；本文档说明工具无法自动检查的选型。

## 语言

- C11，以 CMake 目标特性 `c_std_11` 表达，关闭编译器扩展。
- 保持公共代码在 MSVC、GCC、Clang 和 AppleClang 之间可移植。
- Windows 专有代码用 `#if defined(_WIN32)` 隔离在实现文件中；公共头文件不暴露
  Windows 类型。
- Windows 头文件必须让 `windows.h` 先于 `tlhelp32.h`、`shellapi.h`、`shlobj.h`
  出现，并与其他头文件之间留空行，防止 clang-format 按字母顺序重排。

## 命名

| 对象 | 约定 | 示例 |
|---|---|---|
| 文件与目录 | snake_case | `capture.c`、`asset.h` |
| 函数与变量 | snake_case，公共符号带项目前缀 | `wechatvoiceexport_asset_find` |
| 类型 | snake_case + `_t` 后缀 | `wechatvoiceexport_asset_t` |
| 宏与常量 | UPPER_SNAKE_CASE | `WECHATVOICEEXPORT_API` |
| Include guards | `<PROJECT>_<FILE>_H` | `WECHATVOICEEXPORT_ASSET_H` |

函数式宏使用 UPPER_SNAKE_CASE，函数使用 snake_case。`scripts/check_decl_def.py`
依靠这个区别区分宏与需要定义的公共函数。

## 头文件组织

- 公共接口位于 `include/wechatvoiceexport/`。
- 每个公共头文件使用 `<PROJECT>_<FILE>_H` include guard。
- 公共头文件用 `extern "C"` guard 保持 C++ 可调用。
- 公共头文件只包含它需要的标准头；不为内部实现方便而扩大公共依赖。
- 私有头文件（如 `src/wechatvoiceexport/fs.h`、`md5.h`）只被 `src/` 使用，
  不复制到 `include/`。

## 格式与静态分析

- `clang-format`（LLVM 基础风格、4 空格缩进、100 列）是格式权威。检查脚本运行
  `clang-format --dry-run --Werror`。
- `clang-tidy` 以警告即错误运行。本地使用
  `./scripts/check.ps1 -EnableTidy` 或 `./scripts/check.sh --enable-tidy`。
- `cppcheck --std=c11 --enable=warning,performance,portability` 覆盖
  `src/` 与 `tests/`。
- `tests/vendor/` 下的 Unity 与 `third_party/silk/` 不参与格式、clang-tidy 或
  cppcheck。不要手工改写 vendored 代码。

## 注释与 API 文档

- 公共头文件用 Doxygen `/** ... */` 记录模块、类型与函数契约。
- 每个公共函数有一行 `@brief`；非 void 函数写 `@return`，覆盖所有返回状态。
- `@param` 说明 NULL 约束、有效范围、借用/拥有关系，以及失败时的输出状态。
- 结构体字段仅在单位、范围或所有权不明显时使用行尾 `/**< ... */`。
- 实现文件不重复公共头文件的契约；局部注释解释“为什么”，不解释“做了什么”。
- 只有非显然的约束、安全边界或 workaround 才值得注释。

## 所有权与错误处理

- 明确所有权：分配者负责释放，除非头文件明确说明所有权转移。
- 使用 `wechatvoiceexport_status_t` 与输出参数传递结果，不使用隐藏的全局状态。
- 失败路径必须释放已分配资源；列表类型提供 `*_free()`，可安全处理 NULL 或空结构。
- 公共 API 不终止进程、不打印到 stdout/stderr；诊断由 CLI 层负责。

## 安全边界

- 不新增网络请求、网络库或遥测。
- 不把进程内存内容写入临时文件或日志。
- 微信数据文件只读；输出路径必须经过数据根包含检查。
- 测试只使用合成 SILK 数据，不提交真实语音或账号路径。

## 文件编码

源文件为 UTF-8，使用 LF 行尾（由 `.gitattributes` 强制执行）。
