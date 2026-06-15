#ifndef NURL_HEADERS_H
#define NURL_HEADERS_H

#include "nurl.h"
#include "engine/utils/nurl_error.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char   **keys;       /* canonical-case key strings, heap-owned */
    char   **values;     /* value strings, heap-owned */
    size_t   count;
    size_t   capacity;
} NurlHeaderMap;

NurlHeaderMap *nurl_headermap_new(void);
nurl_err_t     nurl_headermap_set(NurlHeaderMap *m, const char *key, const char *value);
nurl_err_t     nurl_headermap_append(NurlHeaderMap *m, const char *key, const char *value);
nurl_err_t     nurl_headermap_add_raw(NurlHeaderMap *m, const char *line);
bool           nurl_headermap_has(const NurlHeaderMap *m, const char *key);
nurl_err_t     nurl_headermap_apply_auth(NurlHeaderMap *m, const CommonArgs *a);
nurl_err_t     nurl_headermap_apply_common(NurlHeaderMap *m, const CommonArgs *a);
char          *nurl_headermap_serialize(const NurlHeaderMap *m);
void           nurl_headermap_free(NurlHeaderMap *m);

#endif /* NURL_HEADERS_H */
