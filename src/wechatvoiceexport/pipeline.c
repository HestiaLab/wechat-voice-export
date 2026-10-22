#if !defined(_WIN32)
// NOLINTNEXTLINE(bugprone-reserved-identifier)
#define _POSIX_C_SOURCE 200809L
#endif

#include <wechatvoiceexport/pipeline.h>

#include <stdlib.h>
#include <string.h>

#include <wechatvoiceexport/silk.h>
#include <wechatvoiceexport/wav.h>

#include "fs.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

static bool wechatvoiceexport_pipeline_valid_base_name(const char *name) {
    if (name == NULL || name[0] == '\0') {
        return false;
    }
    for (const char *cursor = name; *cursor != '\0'; ++cursor) {
        unsigned char value = (unsigned char)*cursor;
        if (value < 0x20 || value == '/' || value == '\\' || value == '<' || value == '>' ||
            value == ':' || value == '"' || value == '|' || value == '?' || value == '*') {
            return false;
        }
    }
    return strcmp(name, ".") != 0 && strcmp(name, "..") != 0;
}

static wechatvoiceexport_status_t wechatvoiceexport_pipeline_join_name(const char *directory,
                                                                       const char *base_name,
                                                                       const char *extension,
                                                                       char **out_path) {
    size_t base_length = strlen(base_name);
    size_t extension_length = strlen(extension);
    char *file_name;
    wechatvoiceexport_status_t status;

    file_name = (char *)malloc(base_length + extension_length + 1);
    if (file_name == NULL) {
        return WECHATVOICEEXPORT_STATUS_NOMEM;
    }
    memcpy(file_name, base_name, base_length);
    memcpy(file_name + base_length, extension, extension_length + 1);
    status = wechatvoiceexport_fs_join(directory, file_name, out_path);
    free(file_name);
    return status;
}

#if defined(_WIN32)
static bool wechatvoiceexport_pipeline_append_quoted(char **cursor, size_t *remaining,
                                                     const char *argument) {
    size_t length = strlen(argument);
    size_t needed = length * 2 + 4;
    size_t backslashes = 0;
    char *target = *cursor;

    if (needed > *remaining) {
        return false;
    }
    *target++ = '"';
    for (size_t i = 0; i < length; ++i) {
        char value = argument[i];
        if (value == '\\') {
            *target++ = '\\';
            ++backslashes;
        } else if (value == '"') {
            for (size_t j = 0; j < backslashes; ++j) {
                *target++ = '\\';
            }
            *target++ = '\\';
            *target++ = '"';
            backslashes = 0;
        } else {
            *target++ = value;
            backslashes = 0;
        }
    }
    for (size_t j = 0; j < backslashes; ++j) {
        *target++ = '\\';
    }
    *target++ = '"';
    *remaining -= (size_t)(target - *cursor);
    *cursor = target;
    return true;
}

static wchar_t *wechatvoiceexport_pipeline_utf8_to_wide(const char *value) {
    int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1, NULL, 0);
    wchar_t *wide;

    if (length <= 0) {
        return NULL;
    }
    wide = (wchar_t *)malloc((size_t)length * sizeof(wchar_t));
    if (wide == NULL) {
        return NULL;
    }
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1, wide, length) <= 0) {
        free(wide);
        return NULL;
    }
    return wide;
}

static wechatvoiceexport_status_t wechatvoiceexport_pipeline_run_ffmpeg(const char *executable,
                                                                        const char *wav_path,
                                                                        const char *mp3_path) {
    const char *arguments[11];
    size_t capacity = 1;
    char *command_line;
    char *cursor;
    size_t remaining;
    wchar_t *wide_command;
    STARTUPINFOW startup;
    PROCESS_INFORMATION process;
    BOOL created;
    DWORD exit_code = 1;
    wechatvoiceexport_status_t status = WECHATVOICEEXPORT_STATUS_MP3;

    arguments[0] = executable;
    arguments[1] = "-y";
    arguments[2] = "-v";
    arguments[3] = "error";
    arguments[4] = "-i";
    arguments[5] = wav_path;
    arguments[6] = "-codec:a";
    arguments[7] = "libmp3lame";
    arguments[8] = "-q:a";
    arguments[9] = "2";
    arguments[10] = mp3_path;

    for (size_t i = 0; i < 11; ++i) {
        size_t length = strlen(arguments[i]);
        if (length > (SIZE_MAX - capacity - 8) / 2) {
            return WECHATVOICEEXPORT_STATUS_NOMEM;
        }
        capacity += length * 2 + 4;
    }
    command_line = (char *)malloc(capacity);
    if (command_line == NULL) {
        return WECHATVOICEEXPORT_STATUS_NOMEM;
    }
    cursor = command_line;
    remaining = capacity;
    for (size_t i = 0; i < 11; ++i) {
        if (i > 0) {
            *cursor++ = ' ';
            --remaining;
        }
        if (!wechatvoiceexport_pipeline_append_quoted(&cursor, &remaining, arguments[i])) {
            free(command_line);
            return WECHATVOICEEXPORT_STATUS_NOMEM;
        }
    }
    *cursor = '\0';

    wide_command = wechatvoiceexport_pipeline_utf8_to_wide(command_line);
    free(command_line);
    if (wide_command == NULL) {
        return WECHATVOICEEXPORT_STATUS_MP3;
    }

    memset(&startup, 0, sizeof startup);
    startup.cb = sizeof startup;
    memset(&process, 0, sizeof process);
    created = CreateProcessW(NULL, wide_command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL,
                             &startup, &process);
    free(wide_command);
    if (!created) {
        return WECHATVOICEEXPORT_STATUS_MP3;
    }
    WaitForSingleObject(process.hProcess, INFINITE);
    if (GetExitCodeProcess(process.hProcess, &exit_code)) {
        status = exit_code == 0 ? WECHATVOICEEXPORT_STATUS_OK : WECHATVOICEEXPORT_STATUS_MP3;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return status;
}
#else
static wechatvoiceexport_status_t wechatvoiceexport_pipeline_run_ffmpeg(const char *executable,
                                                                        const char *wav_path,
                                                                        const char *mp3_path) {
    pid_t pid;
    int wait_status = 0;

    pid = fork();
    if (pid < 0) {
        return WECHATVOICEEXPORT_STATUS_MP3;
    }
    if (pid == 0) {
        char *const arguments[] = {(char *)executable, (char *)"-y",         (char *)"-v",
                                   (char *)"error",    (char *)"-i",         (char *)wav_path,
                                   (char *)"-codec:a", (char *)"libmp3lame", (char *)"-q:a",
                                   (char *)"2",        (char *)mp3_path,     NULL};
        execvp(executable, arguments);
        _exit(127);
    }
    while (waitpid(pid, &wait_status, 0) < 0) {
        if (errno != EINTR) {
            return WECHATVOICEEXPORT_STATUS_MP3;
        }
    }
    if (!WIFEXITED(wait_status) || WEXITSTATUS(wait_status) != 0) {
        return WECHATVOICEEXPORT_STATUS_MP3;
    }
    return WECHATVOICEEXPORT_STATUS_OK;
}
#endif

static void wechatvoiceexport_pipeline_fill_result(wechatvoiceexport_export_result_t *result,
                                                   const wechatvoiceexport_pcm_t *pcm,
                                                   char *wav_path) {
    result->wav_path = wav_path;
    result->sample_count = pcm->sample_count;
    result->frame_count = pcm->frame_count;
    result->sample_rate = pcm->sample_rate;
}

wechatvoiceexport_status_t
wechatvoiceexport_export_silk(const uint8_t *payload, size_t payload_size,
                              const wechatvoiceexport_export_options_t *options,
                              wechatvoiceexport_export_result_t *out_result) {
    wechatvoiceexport_pcm_t pcm;
    wechatvoiceexport_status_t status;
    char *wav_path = NULL;
    char *mp3_path = NULL;
    bool within_root = false;

    if (payload == NULL || options == NULL || out_result == NULL || options->output_dir == NULL ||
        options->output_dir[0] == '\0' ||
        !wechatvoiceexport_pipeline_valid_base_name(options->base_name)) {
        return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
    }
    memset(out_result, 0, sizeof *out_result);
    memset(&pcm, 0, sizeof pcm);

    status = wechatvoiceexport_silk_decode(payload, payload_size,
                                           WECHATVOICEEXPORT_SILK_DEFAULT_RATE, &pcm);
    if (status != WECHATVOICEEXPORT_STATUS_OK) {
        return status;
    }

    if (options->data_root != NULL && options->data_root[0] != '\0') {
        status = wechatvoiceexport_fs_check_within(options->output_dir, options->data_root,
                                                   &within_root);
        if (status != WECHATVOICEEXPORT_STATUS_OK) {
            wechatvoiceexport_pcm_free(&pcm);
            return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
        }
        if (within_root) {
            wechatvoiceexport_pcm_free(&pcm);
            return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
        }
    }

    status = wechatvoiceexport_fs_create_dir(options->output_dir);
    if (status != WECHATVOICEEXPORT_STATUS_OK) {
        wechatvoiceexport_pcm_free(&pcm);
        return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
    }

    status = wechatvoiceexport_pipeline_join_name(options->output_dir, options->base_name, ".wav",
                                                  &wav_path);
    if (status != WECHATVOICEEXPORT_STATUS_OK) {
        wechatvoiceexport_pcm_free(&pcm);
        return status;
    }
    if (options->write_mp3) {
        status = wechatvoiceexport_pipeline_join_name(options->output_dir, options->base_name,
                                                      ".mp3", &mp3_path);
        if (status != WECHATVOICEEXPORT_STATUS_OK) {
            free(wav_path);
            wechatvoiceexport_pcm_free(&pcm);
            return status;
        }
    }

    if (!options->force && (wechatvoiceexport_fs_file_exists(wav_path) ||
                            (mp3_path != NULL && wechatvoiceexport_fs_file_exists(mp3_path)))) {
        free(wav_path);
        free(mp3_path);
        wechatvoiceexport_pcm_free(&pcm);
        return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
    }

    status = wechatvoiceexport_wav_write(wav_path, pcm.samples, pcm.sample_count, pcm.sample_rate);
    if (status != WECHATVOICEEXPORT_STATUS_OK) {
        free(wav_path);
        free(mp3_path);
        wechatvoiceexport_pcm_free(&pcm);
        return status;
    }

    wechatvoiceexport_pipeline_fill_result(out_result, &pcm, wav_path);
    wav_path = NULL;
    wechatvoiceexport_pcm_free(&pcm);

    if (mp3_path != NULL) {
        const char *executable = options->ffmpeg_path != NULL && options->ffmpeg_path[0] != '\0'
                                     ? options->ffmpeg_path
                                     : "ffmpeg";
        status = wechatvoiceexport_pipeline_run_ffmpeg(executable, out_result->wav_path, mp3_path);
        if (status != WECHATVOICEEXPORT_STATUS_OK) {
            free(mp3_path);
            return WECHATVOICEEXPORT_STATUS_MP3;
        }
        out_result->mp3_path = mp3_path;
    }

    return WECHATVOICEEXPORT_STATUS_OK;
}

void wechatvoiceexport_export_result_free(wechatvoiceexport_export_result_t *result) {
    if (result == NULL) {
        return;
    }
    free(result->wav_path);
    free(result->mp3_path);
    memset(result, 0, sizeof *result);
}
