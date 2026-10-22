# CMake 指南

本指南说明本项目的 CMake 预设、常用选项、测试注册和排错方式。

## 前置要求

- CMake 3.21 或更高版本。
- 支持 C11 的编译器：Windows 上的 MSVC 或 MSYS2 GCC，Linux 上的 GCC/Clang，
  macOS 上的 AppleClang。
- 默认 `ninja-*` 预设需要 Ninja。
- 测试需要 Python 3（架构门禁和 CLI 冒烟测试）。

Windows 推荐环境见 [environment.md](environment.md)。项目通过
`target_compile_features(... c_std_11)` 要求 C11，并关闭编译器扩展。

## 预设

`CMakePresets.json` 提供 configure、build 和 test 三组同名预设。

| 预设 | 用途 |
|---|---|
| `ninja-debug` | 默认开发构建：调试符号、`compile_commands.json`、Ninja |
| `ninja-release` | 优化构建，用于发布前验证 |
| `ninja-asan` | Debug + AddressSanitizer + UBSan（工具链支持时） |
| `ninja-coverage` | Debug + 覆盖率插桩（GCC/Clang） |
| `debug` / `release` | 与 generator 无关的 Debug/Release 配置 |
| `asan` / `coverage` | 与 generator 无关的 sanitizer/覆盖率配置 |

`ninja-*` 是编辑器和 CI 的默认路径，会把 `compile_commands.json` 写入对应构建
目录。需要 Visual Studio、Xcode 或其他 generator 时，使用 generator-neutral
预设，或通过 `CMakeUserPresets.json` 派生出本地预设。该文件已被 Git 忽略，
可参考 `CMakeUserPresets.example.json`。

常用命令：

```sh
cmake --preset ninja-debug
cmake --build --preset ninja-debug
ctest --preset ninja-debug --output-on-failure
```

Release 构建：

```sh
cmake --preset ninja-release
cmake --build --preset ninja-release
ctest --preset ninja-release --output-on-failure
```

## 重要选项

```sh
cmake --preset ninja-debug -DWECHATVOICEEXPORT_BUILD_TESTING=OFF
```

| 选项 | 默认值 | 含义 |
|---|---|---|
| `WECHATVOICEEXPORT_BUILD_CLI` | 顶层构建为 ON | 构建 `wxvoice` 可执行文件 |
| `WECHATVOICEEXPORT_BUILD_TESTING` | 顶层构建为 ON | 构建并注册 CTest 测试 |
| `WECHATVOICEEXPORT_ENABLE_ASAN` | OFF | 启用 AddressSanitizer |
| `WECHATVOICEEXPORT_ENABLE_UBSAN` | OFF | 启用 UndefinedBehaviorSanitizer |
| `WECHATVOICEEXPORT_ENABLE_COVERAGE` | OFF | 启用覆盖率标志 |
| `BUILD_SHARED_LIBS` | OFF | CMake 标准选项；本项目仅验证静态链接 |

作为子项目通过 `add_subdirectory()` 引入时，CLI 与测试默认关闭，避免污染
下游构建。

ASan/UBSan 需要编译器提供对应运行库。Windows 上的 MSYS2 UCRT64 GCC 默认不带
可链接的 sanitizer 运行库；需要时请使用 Linux/macOS CI，或单独配置 CLANG64
工具链。

## 添加源文件

1. 在 `src/wechatvoiceexport/` 下创建 `.c` 文件。
2. 在 `include/wechatvoiceexport/` 下添加或更新公共头文件。
3. 把新源文件加入 [cmake/Sources.cmake](../../cmake/Sources.cmake) 的
   `WECHATVOICEEXPORT_CORE_SOURCES`。
4. 在 `tests/` 下添加测试，并在 [cmake/Tests.cmake](../../cmake/Tests.cmake)
   注册。
5. 运行 `./scripts/check.ps1` 或 `./scripts/check.sh`。

公共头文件中的每个函数声明都必须在 `src/` 中有定义，否则
`wechatvoiceexport_decl_def_check` 会失败。私有头文件放在 `src/` 下，不进入
公共 include 面。

## 添加测试

使用 `wechatvoiceexport_add_test()`：

```cmake
wechatvoiceexport_add_test(wechatvoiceexport_feature_tests
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_feature.c
    LIBS
        wechatvoiceexport_core
)
```

测试目标会自动获得 `include/`、`src/`、`tests/support`、Unity 头文件路径和
`TEST_TMP_ROOT` 编译定义。临时目录应位于构建树内，不要写入仓库根目录。

测试应断言公共行为。若测试需要 SILK 数据，使用
`tests/support/silk_fixture.c` 在内存中生成；不得加入真实语音。

## 生成的版本头文件

版本只在 `project(WeChatVoiceExport VERSION ...)` 中声明一次。CMake 生成
`wechatvoiceexport/version.h`，CLI 通过 `WECHATVOICEEXPORT_VERSION_STRING`
读取。构建树中的生成目录会自动加入核心目标的 include 路径。

## 本地质量检查

```powershell
./scripts/bootstrap.ps1
./scripts/check.ps1
./scripts/check.ps1 -EnableTidy
```

Linux/macOS：

```sh
./scripts/bootstrap.sh
./scripts/check.sh
./scripts/check.sh --enable-tidy
```

`check` 脚本执行格式检查、configure、build 和 CTest。格式检查使用
`clang-format --dry-run --Werror`；`--fix-format` / `-FixFormat` 会就地改写。
`tests/vendor/` 下的第三方代码不参与格式与静态分析。

## 故障排除

**configure 提示源目录不同**：旧的构建缓存指向了别的路径。删除对应构建目录
（例如 `build/ninja-debug`）后重新 configure。

**找不到 Ninja 或 C 编译器**：运行 bootstrap 获取诊断信息。Windows 上使用
Ninja preset 前，请在 Developer PowerShell、Developer Command Prompt 或已选择
MSVC kit 的 VS Code 中操作。

**CMake Tools 构建 `ALL_BUILD` 失败**：运行 `CMake: Set Build Target` 选择
`all`；必要时重置 CMake Tools 扩展状态并重新 configure。

**`ctest` 找不到测试可执行文件（multi-config generator）**：传入匹配的
`-C Debug` 或 `-C Release`。

**clang-tidy 报 Windows 头文件解析错误**：保持 `windows.h` 在
`tlhelp32.h`、`shellapi.h`、`shlobj.h` 之前，并与其他 Windows 头文件之间留空行，
避免 clang-format 重新排序。
