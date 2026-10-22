#include "selection.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <wechatvoiceexport/silk.h>

#define WECHATVOICEEXPORT_SELECTION_MAX_COLLAPSE 32u

static bool pcm_equal(const wechatvoiceexport_pcm_t *left, const wechatvoiceexport_pcm_t *right) {
    size_t bytes;

    if (left->sample_rate != right->sample_rate || left->sample_count != right->sample_count) {
        return false;
    }
    bytes = left->sample_count * sizeof(int16_t);
    return bytes == 0 || memcmp(left->samples, right->samples, bytes) == 0;
}

wechatvoiceexport_status_t
wechatvoiceexport_selection_collapse_equivalent(wechatvoiceexport_candidate_list_t *candidates,
                                                size_t *out_removed) {
    wechatvoiceexport_pcm_t *pcm;
    bool *keep;
    size_t count;
    size_t removed = 0;
    wechatvoiceexport_status_t result = WECHATVOICEEXPORT_STATUS_OK;

    if (candidates == NULL) {
        return WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT;
    }
    if (out_removed != NULL) {
        *out_removed = 0;
    }
    count = candidates->count;
    if (count < 2 || count > WECHATVOICEEXPORT_SELECTION_MAX_COLLAPSE) {
        return WECHATVOICEEXPORT_STATUS_OK;
    }

    pcm = (wechatvoiceexport_pcm_t *)calloc(count, sizeof *pcm);
    keep = (bool *)malloc(count * sizeof *keep);
    if (pcm == NULL || keep == NULL) {
        free(pcm);
        free(keep);
        return WECHATVOICEEXPORT_STATUS_NOMEM;
    }
    for (size_t i = 0; i < count; ++i) {
        wechatvoiceexport_status_t decode_status;

        keep[i] = true;
        decode_status =
            wechatvoiceexport_silk_decode(candidates->items[i].payload, candidates->items[i].size,
                                          WECHATVOICEEXPORT_SILK_DEFAULT_RATE, &pcm[i]);
        if (decode_status == WECHATVOICEEXPORT_STATUS_NOMEM) {
            result = decode_status;
            break;
        }
        if (decode_status != WECHATVOICEEXPORT_STATUS_OK) {
            wechatvoiceexport_pcm_free(&pcm[i]);
        }
    }
    if (result == WECHATVOICEEXPORT_STATUS_OK) {
        for (size_t i = 0; i < count; ++i) {
            if (!keep[i] || pcm[i].samples == NULL) {
                continue;
            }
            for (size_t j = i + 1; j < count; ++j) {
                if (keep[j] && pcm[j].samples != NULL && pcm_equal(&pcm[i], &pcm[j])) {
                    keep[j] = false;
                }
            }
        }

        size_t write = 0;
        for (size_t i = 0; i < count; ++i) {
            if (keep[i]) {
                if (write != i) {
                    candidates->items[write] = candidates->items[i];
                }
                ++write;
            } else {
                free(candidates->items[i].payload);
            }
        }
        removed = count - write;
        candidates->count = write;
    }
    for (size_t i = 0; i < count; ++i) {
        wechatvoiceexport_pcm_free(&pcm[i]);
    }
    free(pcm);
    free(keep);
    if (out_removed != NULL) {
        *out_removed = removed;
    }
    return result;
}
