#ifndef NURL_HEADERS_H
#define NURL_HEADERS_H

#include "nurl.h"
#include "engine/utils/nurl_error.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char **entries;   /* "Key: Value\r\n" strings */
    size_t count;
    size_t capacity;
} NurlHeaderList;

NurlHeaderList *nurl_headers_new(void);
nurl_err_t      nurl_headers_add(NurlHeaderList *h, const char *key, const char *value);
nurl_err_t      nurl_headers_add_raw(NurlHeaderList *h, const char *line);  /* "Key: Value" */
bool            nurl_headers_has(const NurlHeaderList *h, const char *key); /* case-insensitive */
nurl_err_t      nurl_headers_apply_auth(NurlHeaderList *h, const CommonArgs *a);
nurl_err_t      nurl_headers_apply_common(NurlHeaderList *h, const CommonArgs *a);
char           *nurl_headers_serialize(const NurlHeaderList *h);            /* heap alloc, caller frees */
void            nurl_headers_free(NurlHeaderList *h);

#endif /* NURL_HEADERS_H */
