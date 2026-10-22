#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <wechatvoiceexport/asset.h>
#include <wechatvoiceexport/capture.h>
#include <wechatvoiceexport/pipeline.h>
#include <wechatvoiceexport/version.h>

#include "wechatvoiceexport/selection.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#define WECHATVOICEEXPORT_EXIT_OK 0
#define WECHATVOICEEXPORT_EXIT_USAGE 2
#define WECHATVOICEEXPORT_EXIT_NO_ASSET 3
#define WECHATVOICEEXPORT_EXIT_CAPTURE 4
#define WECHATVOICEEXPORT_EXIT_WRITE 5
#define WECHATVOICEEXPORT_EXIT_MP3 6

#define WECHATVOICEEXPORT_POLL_INTERVAL_MS 500u
#define WECHATVOICEEXPORT_WATCH_MAX_SECONDS 3600u

static void print_usage(FILE *stream) {
    fprintf(stream, "wxvoice - 导出微信 4.x 收藏语音\n"
                    "\n"
                    "用法:\n"
                    "  wxvoice list [选项]\n"
                    "  wxvoice export <序号|短哈希> [选项]\n"
                    "  wxvoice --help\n"
                    "  wxvoice --version\n"
                    "\n"
                    "list 选项:\n"
                    "  --root <目录>       指定 xwechat_files 或账号目录\n"
                    "  --verbose           输出调试信息\n"
                    "\n"
                    "export 选项:\n"
                    "  --root <目录>       指定 xwechat_files 或账号目录\n"
                    "  --out <目录>        输出目录（默认 ./exports）\n"
                    "  --process <名称>    微信进程名（默认 Weixin.exe）\n"
                    "  --pid <PID>         仅扫描指定微信进程（默认自动扫描全部匹配进程）\n"
                    "  --watch <秒>        播放期间轮询进程，直到找到或超时（1-3600）\n"
                    "  --candidate <N>     选择第 N 个候选（从 1 开始）\n"
                    "  --name <基名>       输出文件基名（不含扩展名）\n"
                    "  --mp3               同时生成 MP3（需要 ffmpeg）\n"
                    "  --ffmpeg <路径>     ffmpeg 可执行文件（默认从 PATH 查找）\n"
                    "  --force             覆盖已存在的输出文件\n"
                    "  --verbose           输出调试信息\n"
                    "\n"
                    "退出码:\n"
                    "  0 成功；2 参数/路径/覆盖冲突；3 未找到资产；\n"
                    "  4 进程不存在/无候选/候选歧义；5 解码或写入失败；6 MP3 失败（WAV 已保留）\n");
}

static const char *option_value(int argc, char **argv, int *index, const char *option) {
    if (*index + 1 >= argc) {
        fprintf(stderr, "错误: %s 需要一个值。\n", option);
        return NULL;
    }
    ++(*index);
    return argv[*index];
}

static bool parse_unsigned(const char *text, unsigned long long *out_value) {
    char *end = NULL;
    unsigned long long value;

    if (text == NULL || text[0] == '\0' || text[0] == '-') {
        return false;
    }
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') {
        return false;
    }
    *out_value = value;
    return true;
}

static void print_asset_table(const wechatvoiceexport_asset_list_t *assets) {
    printf("序号  短哈希    密文       期望明文          来源  持久副本  路径\n");
    for (size_t i = 0; i < assets->count; ++i) {
        const wechatvoiceexport_asset_t *asset = &assets->items[i];
        uint64_t minimum = 0;
        uint64_t maximum = 0;
        printf("%4zu  %-8.8s  %8llu  ", i + 1, asset->md5_hex,
               (unsigned long long)asset->ciphertext_size);
        if (wechatvoiceexport_asset_expected_plain_range(asset->ciphertext_size, &minimum,
                                                         &maximum)) {
            printf("[%llu, %llu]", (unsigned long long)minimum, (unsigned long long)maximum);
        } else {
            printf("%-16s", "-");
        }
        printf("  %4zu  %-6s  %s\n", asset->source_count, asset->has_persistent_copy ? "是" : "否",
               asset->path);
    }
}

static wechatvoiceexport_status_t resolve_root_or_report(const char *root_argument,
                                                         char **out_root) {
    wechatvoiceexport_status_t status =
        wechatvoiceexport_data_root_resolve(root_argument, out_root);
    if (status == WECHATVOICEEXPORT_STATUS_NOT_FOUND) {
        fprintf(stderr, "错误: 微信数据根不存在或不可读（尝试过: %s）。\n",
                *out_root != NULL ? *out_root : "(默认文档目录)");
        fprintf(stderr, "请用 --root <目录> 指定 xwechat_files 或账号目录。\n");
    } else if (status != WECHATVOICEEXPORT_STATUS_OK) {
        fprintf(stderr, "错误: 无法解析微信数据根: %s\n", wechatvoiceexport_status_string(status));
    }
    return status;
}

static int command_list(int argc, char **argv) {
    const char *root_argument = NULL;
    bool verbose = false;
    char *root = NULL;
    wechatvoiceexport_asset_list_t assets;
    wechatvoiceexport_status_t status;

    for (int i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(stdout);
            return WECHATVOICEEXPORT_EXIT_OK;
        }
        if (strcmp(argv[i], "--root") == 0) {
            root_argument = option_value(argc, argv, &i, "--root");
            if (root_argument == NULL) {
                return WECHATVOICEEXPORT_EXIT_USAGE;
            }
        } else if (strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else {
            fprintf(stderr, "错误: list 不支持参数: %s\n", argv[i]);
            return WECHATVOICEEXPORT_EXIT_USAGE;
        }
    }

    status = resolve_root_or_report(root_argument, &root);
    if (status != WECHATVOICEEXPORT_STATUS_OK) {
        free(root);
        return WECHATVOICEEXPORT_EXIT_USAGE;
    }
    if (verbose) {
        fprintf(stderr, "调试: 数据根: %s\n", root);
    }

    memset(&assets, 0, sizeof assets);
    status = wechatvoiceexport_assets_scan(root, &assets);
    if (status != WECHATVOICEEXPORT_STATUS_OK) {
        fprintf(stderr, "错误: 扫描收藏语音失败: %s\n", wechatvoiceexport_status_string(status));
        free(root);
        wechatvoiceexport_assets_free(&assets);
        return status == WECHATVOICEEXPORT_STATUS_NOMEM ? WECHATVOICEEXPORT_EXIT_WRITE
                                                        : WECHATVOICEEXPORT_EXIT_USAGE;
    }
    if (assets.count == 0) {
        fprintf(stderr, "未找到收藏语音资源。请确认微信已登录且收藏过语音，"
                        "或用 --root 指定正确的数据目录。\n");
        if (verbose && assets.ignored_too_small > 0) {
            fprintf(stderr, "调试: 跳过 %zu 个小于 17 字节的文件。\n", assets.ignored_too_small);
        }
        free(root);
        wechatvoiceexport_assets_free(&assets);
        return WECHATVOICEEXPORT_EXIT_NO_ASSET;
    }

    print_asset_table(&assets);
    if (verbose && assets.ignored_too_small > 0) {
        fprintf(stderr, "调试: 跳过 %zu 个小于 17 字节的文件（不是有效收藏语音）。\n",
                assets.ignored_too_small);
    }

    free(root);
    wechatvoiceexport_assets_free(&assets);
    return WECHATVOICEEXPORT_EXIT_OK;
}

static bool selector_matches_prefix(const wechatvoiceexport_asset_t *asset, const char *selector) {
    size_t length = strlen(selector);
    for (size_t i = 0; i < length; ++i) {
        char left = asset->md5_hex[i];
        char right = selector[i];
        if (right >= 'A' && right <= 'F') {
            right = (char)(right - 'A' + 'a');
        }
        if (left != right) {
            return false;
        }
    }
    return true;
}

static void print_candidates(const wechatvoiceexport_candidate_list_t *candidates,
                             uint64_t ciphertext_size) {
    printf("找到 %zu 个候选，请用 --candidate N 指定:\n", candidates->count);
    printf("序号  载荷短哈希  字节数  填充字节  时长\n");
    for (size_t i = 0; i < candidates->count; ++i) {
        const wechatvoiceexport_candidate_t *candidate = &candidates->items[i];
        unsigned long long padding = 0;
        bool padding_known = ciphertext_size >= (uint64_t)candidate->size &&
                             ciphertext_size - (uint64_t)candidate->size <= 16;
        if (padding_known) {
            padding = (unsigned long long)(ciphertext_size - (uint64_t)candidate->size);
        }
        printf("%4zu  %-10.8s  %6zu  ", i + 1, candidate->md5_hex, candidate->size);
        if (padding_known) {
            printf("%8llu  ", padding);
        } else {
            printf("%8s  ", "-");
        }
        printf("%.2f 秒\n", (double)candidate->frame_count * 0.02);
    }
}

static const char *capture_error_hint(wechatvoiceexport_status_t status) {
    switch (status) {
    case WECHATVOICEEXPORT_STATUS_UNSUPPORTED:
        return "当前平台不支持读取微信进程内存。";
    case WECHATVOICEEXPORT_STATUS_IO:
        return "无法读取微信进程内存，可能被安全软件拦截或权限不足。";
    case WECHATVOICEEXPORT_STATUS_NOMEM:
        return "内存不足，无法完成扫描。";
    default:
        return "扫描进程内存失败。";
    }
}

static void sleep_briefly(void) {
#if defined(_WIN32)
    Sleep(WECHATVOICEEXPORT_POLL_INTERVAL_MS);
#endif
    /* Capture is Windows-only, so polling never reaches this on other platforms. */
}

typedef struct scan_round_result_t {
    bool any_success;
    bool fatal;
    bool enumeration_failed;
    wechatvoiceexport_status_t error;
} scan_round_result_t;

static bool capture_status_is_fatal(wechatvoiceexport_status_t status) {
    return status == WECHATVOICEEXPORT_STATUS_UNSUPPORTED ||
           status == WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT ||
           status == WECHATVOICEEXPORT_STATUS_NOMEM;
}

static scan_round_result_t scan_targets(const char *process_name, bool pid_given, uint32_t pid,
                                        uint64_t minimum, uint64_t maximum,
                                        wechatvoiceexport_pid_list_t *processes,
                                        wechatvoiceexport_candidate_list_t *candidates,
                                        bool verbose) {
    scan_round_result_t result;
    size_t target_count;

    memset(&result, 0, sizeof result);
    result.error = WECHATVOICEEXPORT_STATUS_OK;

    if (!pid_given) {
        wechatvoiceexport_status_t status;

        wechatvoiceexport_capture_pids_free(processes);
        status = wechatvoiceexport_capture_list_processes(process_name, processes);
        if (status != WECHATVOICEEXPORT_STATUS_OK) {
            result.fatal = true;
            result.enumeration_failed = true;
            result.error = status;
            return result;
        }
        if (processes->count == 0) {
            result.error = WECHATVOICEEXPORT_STATUS_NOT_FOUND;
            return result;
        }
        if (verbose) {
            fprintf(stderr, "调试: 本轮扫描 %zu 个 %s 进程。\n", processes->count, process_name);
        }
    }

    target_count = pid_given ? 1 : processes->count;
    for (size_t i = 0; i < target_count; ++i) {
        uint32_t target_pid = pid_given ? pid : processes->items[i];
        size_t before = candidates->count;
        wechatvoiceexport_status_t status =
            wechatvoiceexport_capture_scan_process(target_pid, minimum, maximum, candidates);
        bool added = candidates->count > before;

        if (capture_status_is_fatal(status)) {
            result.fatal = true;
            result.error = status;
            return result;
        }
        if (status == WECHATVOICEEXPORT_STATUS_OK || added) {
            result.any_success = true;
            if (status != WECHATVOICEEXPORT_STATUS_OK && verbose) {
                fprintf(stderr, "调试: PID %u 扫描中断（%s），已保留新候选。\n",
                        (unsigned)target_pid, wechatvoiceexport_status_string(status));
            }
        } else {
            if (result.error == WECHATVOICEEXPORT_STATUS_OK) {
                result.error = status;
            }
            if (verbose) {
                fprintf(stderr, "调试: PID %u 扫描失败: %s\n", (unsigned)target_pid,
                        wechatvoiceexport_status_string(status));
            }
        }
    }
    return result;
}

typedef struct export_cli_options_t {
    const char *selector;
    const char *root_argument;
    const char *output_dir;
    const char *process_name;
    const char *ffmpeg_path;
    const char *name_argument;
    bool write_mp3;
    bool force;
    bool verbose;
    bool pid_given;
    bool candidate_given;
    unsigned watch_seconds;
    uint32_t pid;
    size_t candidate_number;
} export_cli_options_t;

typedef enum parse_result_t {
    PARSE_RESULT_OK,
    PARSE_RESULT_HELP,
    PARSE_RESULT_ERROR
} parse_result_t;

static parse_result_t parse_export_options(int argc, char **argv, export_cli_options_t *options) {
    memset(options, 0, sizeof *options);
    options->output_dir = "exports";
    options->process_name = "Weixin.exe";

    for (int i = 0; i < argc; ++i) {
        const char *value = NULL;
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            return PARSE_RESULT_HELP;
        }
        if (strcmp(argv[i], "--root") == 0) {
            options->root_argument = option_value(argc, argv, &i, "--root");
            if (options->root_argument == NULL) {
                return PARSE_RESULT_ERROR;
            }
        } else if (strcmp(argv[i], "--out") == 0) {
            options->output_dir = option_value(argc, argv, &i, "--out");
            if (options->output_dir == NULL) {
                return PARSE_RESULT_ERROR;
            }
        } else if (strcmp(argv[i], "--process") == 0) {
            options->process_name = option_value(argc, argv, &i, "--process");
            if (options->process_name == NULL) {
                return PARSE_RESULT_ERROR;
            }
        } else if (strcmp(argv[i], "--ffmpeg") == 0) {
            options->ffmpeg_path = option_value(argc, argv, &i, "--ffmpeg");
            if (options->ffmpeg_path == NULL) {
                return PARSE_RESULT_ERROR;
            }
        } else if (strcmp(argv[i], "--name") == 0) {
            options->name_argument = option_value(argc, argv, &i, "--name");
            if (options->name_argument == NULL) {
                return PARSE_RESULT_ERROR;
            }
        } else if (strcmp(argv[i], "--pid") == 0) {
            unsigned long long parsed = 0;
            value = option_value(argc, argv, &i, "--pid");
            if (value == NULL) {
                return PARSE_RESULT_ERROR;
            }
            if (!parse_unsigned(value, &parsed) || parsed == 0 || parsed > UINT32_MAX) {
                fprintf(stderr, "错误: --pid 需要一个正整数。\n");
                return PARSE_RESULT_ERROR;
            }
            options->pid = (uint32_t)parsed;
            options->pid_given = true;
        } else if (strcmp(argv[i], "--watch") == 0) {
            unsigned long long parsed = 0;
            value = option_value(argc, argv, &i, "--watch");
            if (value == NULL) {
                return PARSE_RESULT_ERROR;
            }
            if (!parse_unsigned(value, &parsed) || parsed == 0 ||
                parsed > WECHATVOICEEXPORT_WATCH_MAX_SECONDS) {
                fprintf(stderr, "错误: --watch 需要 1-%u 之间的秒数。\n",
                        WECHATVOICEEXPORT_WATCH_MAX_SECONDS);
                return PARSE_RESULT_ERROR;
            }
            options->watch_seconds = (unsigned)parsed;
        } else if (strcmp(argv[i], "--candidate") == 0) {
            unsigned long long parsed = 0;
            value = option_value(argc, argv, &i, "--candidate");
            if (value == NULL) {
                return PARSE_RESULT_ERROR;
            }
            if (!parse_unsigned(value, &parsed) || parsed == 0 || parsed > SIZE_MAX) {
                fprintf(stderr, "错误: --candidate 需要一个正整数。\n");
                return PARSE_RESULT_ERROR;
            }
            options->candidate_number = (size_t)parsed;
            options->candidate_given = true;
        } else if (strcmp(argv[i], "--mp3") == 0) {
            options->write_mp3 = true;
        } else if (strcmp(argv[i], "--force") == 0) {
            options->force = true;
        } else if (strcmp(argv[i], "--verbose") == 0) {
            options->verbose = true;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "错误: export 不支持参数: %s\n", argv[i]);
            return PARSE_RESULT_ERROR;
        } else if (options->selector == NULL) {
            options->selector = argv[i];
        } else {
            fprintf(stderr, "错误: 只能指定一个资产选择器。\n");
            return PARSE_RESULT_ERROR;
        }
    }
    return PARSE_RESULT_OK;
}

static int resolve_export_asset(const export_cli_options_t *options, char **out_root,
                                wechatvoiceexport_asset_list_t *out_assets,
                                const wechatvoiceexport_asset_t **out_asset, uint64_t *out_minimum,
                                uint64_t *out_maximum) {
    wechatvoiceexport_status_t status;
    size_t asset_index = 0;
    const wechatvoiceexport_asset_t *asset;
    uint64_t minimum = 0;
    uint64_t maximum = 0;
    int exit_code = WECHATVOICEEXPORT_EXIT_OK;

    status = resolve_root_or_report(options->root_argument, out_root);
    if (status != WECHATVOICEEXPORT_STATUS_OK) {
        exit_code = WECHATVOICEEXPORT_EXIT_USAGE;
        goto cleanup;
    }
    if (options->verbose) {
        fprintf(stderr, "调试: 数据根: %s\n", *out_root);
    }

    status = wechatvoiceexport_assets_scan(*out_root, out_assets);
    if (status != WECHATVOICEEXPORT_STATUS_OK) {
        fprintf(stderr, "错误: 扫描收藏语音失败: %s\n", wechatvoiceexport_status_string(status));
        exit_code = status == WECHATVOICEEXPORT_STATUS_NOMEM ? WECHATVOICEEXPORT_EXIT_WRITE
                                                             : WECHATVOICEEXPORT_EXIT_USAGE;
        goto cleanup;
    }
    status = wechatvoiceexport_asset_find(out_assets, options->selector, &asset_index);
    if (status == WECHATVOICEEXPORT_STATUS_AMBIGUOUS) {
        fprintf(stderr, "短哈希前缀 %s 匹配多个资产，请提供更长的前缀:\n", options->selector);
        for (size_t i = 0; i < out_assets->count; ++i) {
            if (selector_matches_prefix(&out_assets->items[i], options->selector)) {
                fprintf(stderr, "  %zu  %s  %llu 字节  %s\n", i + 1, out_assets->items[i].md5_hex,
                        (unsigned long long)out_assets->items[i].ciphertext_size,
                        out_assets->items[i].path);
            }
        }
        exit_code = WECHATVOICEEXPORT_EXIT_USAGE;
        goto cleanup;
    }
    if (status != WECHATVOICEEXPORT_STATUS_OK) {
        fprintf(stderr, "未找到资产: %s\n", options->selector);
        exit_code = WECHATVOICEEXPORT_EXIT_NO_ASSET;
        goto cleanup;
    }
    asset = &out_assets->items[asset_index];
    if (!wechatvoiceexport_asset_expected_plain_range(asset->ciphertext_size, &minimum, &maximum)) {
        fprintf(stderr, "错误: 资产 %s 的密文过小（%llu 字节）。\n", asset->md5_hex,
                (unsigned long long)asset->ciphertext_size);
        exit_code = WECHATVOICEEXPORT_EXIT_NO_ASSET;
        goto cleanup;
    }
    if (options->verbose) {
        fprintf(stderr, "调试: 资产 %s，密文 %llu 字节，期望明文 [%llu, %llu]\n", asset->md5_hex,
                (unsigned long long)asset->ciphertext_size, (unsigned long long)minimum,
                (unsigned long long)maximum);
    }
    *out_asset = asset;
    *out_minimum = minimum;
    *out_maximum = maximum;

cleanup:
    if (exit_code != WECHATVOICEEXPORT_EXIT_OK) {
        free(*out_root);
        *out_root = NULL;
        wechatvoiceexport_assets_free(out_assets);
    }
    return exit_code;
}

static int collect_candidates(const export_cli_options_t *options, uint64_t minimum,
                              uint64_t maximum, wechatvoiceexport_pid_list_t *processes,
                              wechatvoiceexport_candidate_list_t *candidates) {
    time_t deadline;
    bool reported_no_process = false;
    bool reported_scan_error = false;

    if (options->pid_given) {
        fprintf(stderr,
                "提示: 本工具仅用于处理本人设备上的数据；将只读扫描 PID %u 的内存，"
                "不修改微信数据，不生成内存转储。\n",
                (unsigned)options->pid);
    } else {
        fprintf(stderr,
                "提示: 本工具仅用于处理本人设备上的数据；将只读扫描进程名为 %s 的微信进程，"
                "不修改微信数据，不生成内存转储。\n",
                options->process_name);
    }
    if (options->watch_seconds > 0) {
        fprintf(stderr, "等待明文语音载荷：请在微信中播放收藏语音（最多 %u 秒，Ctrl+C 停止）…\n",
                options->watch_seconds);
    }
    deadline = time(NULL) + (time_t)options->watch_seconds;

    for (;;) {
        scan_round_result_t round =
            scan_targets(options->process_name, options->pid_given, options->pid, minimum, maximum,
                         processes, candidates, options->verbose);
        if (round.fatal) {
            if (round.enumeration_failed) {
                fprintf(stderr, "错误: 无法查找微信进程（%s）: %s\n", options->process_name,
                        wechatvoiceexport_status_string(round.error));
            } else {
                fprintf(stderr, "错误: %s\n", capture_error_hint(round.error));
            }
            return round.error == WECHATVOICEEXPORT_STATUS_UNSUPPORTED ||
                           round.error == WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT
                       ? WECHATVOICEEXPORT_EXIT_USAGE
                       : (round.error == WECHATVOICEEXPORT_STATUS_NOMEM
                              ? WECHATVOICEEXPORT_EXIT_WRITE
                              : WECHATVOICEEXPORT_EXIT_CAPTURE);
        }
        if (candidates->count > 0) {
            return WECHATVOICEEXPORT_EXIT_OK;
        }
        if (options->watch_seconds == 0) {
            if (round.any_success) {
                fprintf(stderr, "未找到明文语音载荷。请先在微信中打开并播放该收藏语音，然后重试；"
                                "也可加 --watch 30 在播放期间自动轮询。\n");
            } else if (round.error == WECHATVOICEEXPORT_STATUS_NOT_FOUND) {
                fprintf(stderr, "未找到进程 %s。请先启动微信并登录。\n", options->process_name);
            } else {
                fprintf(stderr, "错误: %s\n", capture_error_hint(round.error));
            }
            return WECHATVOICEEXPORT_EXIT_CAPTURE;
        }
        if (round.error == WECHATVOICEEXPORT_STATUS_NOT_FOUND && !reported_no_process) {
            fprintf(stderr, "未找到进程 %s，等待微信启动...\n", options->process_name);
            reported_no_process = true;
        } else if (!round.any_success && round.error != WECHATVOICEEXPORT_STATUS_OK &&
                   !reported_scan_error) {
            fprintf(stderr, "警告: %s将继续重试。\n", capture_error_hint(round.error));
            reported_scan_error = true;
        }
        if (time(NULL) >= deadline) {
            fprintf(stderr,
                    "错误: 已轮询 %u 秒，仍未找到明文语音载荷。"
                    "请确认微信正在运行，并已在微信中打开并播放该收藏语音。\n",
                    options->watch_seconds);
            return WECHATVOICEEXPORT_EXIT_CAPTURE;
        }
        sleep_briefly();
    }
}

static int select_candidate(const export_cli_options_t *options,
                            wechatvoiceexport_candidate_list_t *candidates,
                            const wechatvoiceexport_asset_t *asset,
                            const wechatvoiceexport_candidate_t **out_candidate) {
    wechatvoiceexport_status_t status;
    size_t removed = 0;
    size_t before = candidates->count;

    if (options->verbose) {
        fprintf(stderr, "调试: 扫描得到 %zu 个候选\n", before);
    }
    status = wechatvoiceexport_selection_collapse_equivalent(candidates, &removed);
    if (status != WECHATVOICEEXPORT_STATUS_OK) {
        fprintf(stderr, "错误: 无法合并等价候选: %s\n", wechatvoiceexport_status_string(status));
        return status == WECHATVOICEEXPORT_STATUS_NOMEM ? WECHATVOICEEXPORT_EXIT_WRITE
                                                        : WECHATVOICEEXPORT_EXIT_CAPTURE;
    }
    if (options->verbose) {
        fprintf(stderr, "调试: 音频等价合并移除 %zu 个，剩 %zu 个候选\n", removed,
                candidates->count);
    }

    if (options->candidate_given) {
        if (options->candidate_number > candidates->count) {
            fprintf(stderr, "错误: --candidate %zu 越界，共 %zu 个候选。\n",
                    options->candidate_number, candidates->count);
            return WECHATVOICEEXPORT_EXIT_USAGE;
        }
        *out_candidate = &candidates->items[options->candidate_number - 1];
    } else if (candidates->count == 1) {
        *out_candidate = &candidates->items[0];
    } else {
        print_candidates(candidates, asset->ciphertext_size);
        return WECHATVOICEEXPORT_EXIT_CAPTURE;
    }
    return WECHATVOICEEXPORT_EXIT_OK;
}

static int export_selected_candidate(const export_cli_options_t *options,
                                     const wechatvoiceexport_asset_t *asset,
                                     const wechatvoiceexport_candidate_t *candidate,
                                     const char *root) {
    char base_name[64];
    wechatvoiceexport_export_options_t export_options;
    wechatvoiceexport_export_result_t result;
    wechatvoiceexport_status_t status;
    int exit_code = WECHATVOICEEXPORT_EXIT_OK;

    memset(&result, 0, sizeof result);
    if (options->name_argument != NULL) {
        size_t length = strlen(options->name_argument);
        if (length >= sizeof base_name) {
            fprintf(stderr, "错误: --name 过长。\n");
            exit_code = WECHATVOICEEXPORT_EXIT_USAGE;
            goto cleanup;
        }
        memcpy(base_name, options->name_argument, length + 1);
    } else {
        memcpy(base_name, asset->md5_hex, 8);
        base_name[8] = '\0';
        if (options->candidate_given) {
            int written =
                snprintf(base_name + 8, sizeof base_name - 8, "-c%zu", options->candidate_number);
            if (written < 0 || (size_t)written >= sizeof base_name - 8) {
                fprintf(stderr, "错误: 输出文件名过长。\n");
                exit_code = WECHATVOICEEXPORT_EXIT_USAGE;
                goto cleanup;
            }
        }
    }

    memset(&export_options, 0, sizeof export_options);
    export_options.output_dir = options->output_dir;
    export_options.base_name = base_name;
    export_options.data_root = root;
    export_options.ffmpeg_path = options->ffmpeg_path;
    export_options.write_mp3 = options->write_mp3;
    export_options.force = options->force;

    status = wechatvoiceexport_export_silk(candidate->payload, candidate->size, &export_options,
                                           &result);
    if (status == WECHATVOICEEXPORT_STATUS_OK) {
        printf("已导出 WAV: %s\n", result.wav_path);
        printf("音频: %zu 样本, %u Hz, 16-bit, 单声道, %.2f 秒\n", result.sample_count,
               (unsigned)result.sample_rate,
               result.sample_rate != 0 ? (double)result.sample_count / (double)result.sample_rate
                                       : 0.0);
        if (result.mp3_path != NULL) {
            printf("已导出 MP3: %s\n", result.mp3_path);
        }
        exit_code = WECHATVOICEEXPORT_EXIT_OK;
    } else if (status == WECHATVOICEEXPORT_STATUS_MP3) {
        fprintf(stderr, "MP3 生成失败（ffmpeg 缺失或执行出错），WAV 已保留: %s\n",
                result.wav_path != NULL ? result.wav_path : "(未知)");
        exit_code = WECHATVOICEEXPORT_EXIT_MP3;
    } else if (status == WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT) {
        fprintf(stderr,
                "错误: 输出路径不可用。请检查 --out/--name，确认输出目录不在微信数据目录内，"
                "且目标文件不存在（可用 --force 覆盖）。\n");
        exit_code = WECHATVOICEEXPORT_EXIT_USAGE;
    } else if (status == WECHATVOICEEXPORT_STATUS_DECODE) {
        fprintf(stderr, "错误: SILK 解码失败。\n");
        exit_code = WECHATVOICEEXPORT_EXIT_WRITE;
    } else {
        fprintf(stderr, "错误: 写入输出文件失败: %s\n", wechatvoiceexport_status_string(status));
        exit_code = WECHATVOICEEXPORT_EXIT_WRITE;
    }

cleanup:
    wechatvoiceexport_export_result_free(&result);
    return exit_code;
}

static int command_export(int argc, char **argv) {
    export_cli_options_t options;
    parse_result_t parsed;
    char *root = NULL;
    wechatvoiceexport_asset_list_t assets;
    wechatvoiceexport_pid_list_t processes;
    wechatvoiceexport_candidate_list_t candidates;
    const wechatvoiceexport_asset_t *asset = NULL;
    const wechatvoiceexport_candidate_t *candidate = NULL;
    uint64_t minimum = 0;
    uint64_t maximum = 0;
    int exit_code = WECHATVOICEEXPORT_EXIT_OK;

    parsed = parse_export_options(argc, argv, &options);
    if (parsed == PARSE_RESULT_HELP) {
        print_usage(stdout);
        return WECHATVOICEEXPORT_EXIT_OK;
    }
    if (parsed == PARSE_RESULT_ERROR) {
        return WECHATVOICEEXPORT_EXIT_USAGE;
    }
    if (options.selector == NULL) {
        fprintf(stderr, "错误: export 需要一个资产序号或短哈希。\n");
        return WECHATVOICEEXPORT_EXIT_USAGE;
    }

    memset(&assets, 0, sizeof assets);
    memset(&processes, 0, sizeof processes);
    memset(&candidates, 0, sizeof candidates);

    exit_code = resolve_export_asset(&options, &root, &assets, &asset, &minimum, &maximum);
    if (exit_code != WECHATVOICEEXPORT_EXIT_OK) {
        goto cleanup;
    }
    exit_code = collect_candidates(&options, minimum, maximum, &processes, &candidates);
    if (exit_code != WECHATVOICEEXPORT_EXIT_OK) {
        goto cleanup;
    }
    exit_code = select_candidate(&options, &candidates, asset, &candidate);
    if (exit_code != WECHATVOICEEXPORT_EXIT_OK) {
        goto cleanup;
    }
    exit_code = export_selected_candidate(&options, asset, candidate, root);

cleanup:
    free(root);
    wechatvoiceexport_assets_free(&assets);
    wechatvoiceexport_capture_pids_free(&processes);
    wechatvoiceexport_capture_candidates_free(&candidates);
    return exit_code;
}

int main(int argc, char **argv) {
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    if (argc < 2) {
        print_usage(stderr);
        return WECHATVOICEEXPORT_EXIT_USAGE;
    }
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_usage(stdout);
        return WECHATVOICEEXPORT_EXIT_OK;
    }
    if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-V") == 0) {
        printf("wxvoice %s\n", WECHATVOICEEXPORT_VERSION_STRING);
        return WECHATVOICEEXPORT_EXIT_OK;
    }
    if (strcmp(argv[1], "list") == 0) {
        return command_list(argc - 2, argv + 2);
    }
    if (strcmp(argv[1], "export") == 0) {
        return command_export(argc - 2, argv + 2);
    }
    fprintf(stderr, "错误: 未知命令: %s\n", argv[1]);
    print_usage(stderr);
    return WECHATVOICEEXPORT_EXIT_USAGE;
}
