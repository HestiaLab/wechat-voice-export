#include <wechatvoiceexport/error.h>

const char *wechatvoiceexport_status_string(wechatvoiceexport_status_t status) {
    switch (status) {
    case WECHATVOICEEXPORT_STATUS_OK:
        return "ok";
    case WECHATVOICEEXPORT_STATUS_INVALID_ARGUMENT:
        return "invalid argument";
    case WECHATVOICEEXPORT_STATUS_IO:
        return "i/o error";
    case WECHATVOICEEXPORT_STATUS_NOT_FOUND:
        return "not found";
    case WECHATVOICEEXPORT_STATUS_NOMEM:
        return "out of memory";
    case WECHATVOICEEXPORT_STATUS_UNSUPPORTED:
        return "unsupported";
    case WECHATVOICEEXPORT_STATUS_DECODE:
        return "decode error";
    case WECHATVOICEEXPORT_STATUS_AMBIGUOUS:
        return "ambiguous selector";
    case WECHATVOICEEXPORT_STATUS_MP3:
        return "mp3 transcode failed";
    }
    return "unknown status";
}
