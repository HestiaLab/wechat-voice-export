#ifndef WECHATVOICEEXPORT_SELECTION_H
#define WECHATVOICEEXPORT_SELECTION_H

#include <stddef.h>

#include <wechatvoiceexport/capture.h>
#include <wechatvoiceexport/error.h>

/*
 * Collapse candidates whose containers decode to identical PCM, keeping the
 * first of each audio group. Candidates that fail to decode stay independent.
 * Discarded payloads are freed; the item array is compacted and count reduced.
 *
 * out_removed may be NULL.
 */
wechatvoiceexport_status_t
wechatvoiceexport_selection_collapse_equivalent(wechatvoiceexport_candidate_list_t *candidates,
                                                size_t *out_removed);

#endif /* WECHATVOICEEXPORT_SELECTION_H */
