#ifndef SONET_HEADERS_H
#define SONET_HEADERS_H

#include "errors/sonet_error.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char   **keys;       /* canonical-case key strings, heap-owned */
    char   **values;     /* value strings, heap-owned */
    size_t   count;
    size_t   capacity;
} SonetHeaderMap;

SonetHeaderMap *sonet_headermap_new(void);
sonet_err_t     sonet_headermap_set(SonetHeaderMap *m, const char *key, const char *value);
sonet_err_t     sonet_headermap_append(SonetHeaderMap *m, const char *key, const char *value);
sonet_err_t     sonet_headermap_add_raw(SonetHeaderMap *m, const char *line);
bool           sonet_headermap_has(const SonetHeaderMap *m, const char *key);
char          *sonet_headermap_serialize(const SonetHeaderMap *m);
void           sonet_headermap_free(SonetHeaderMap *m);

#endif /* SONET_HEADERS_H */
