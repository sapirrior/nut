#include "nurl_headers.h"
#include "nurl_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

NurlHeaderList *nurl_headers_new(void) {
    NurlHeaderList *h = malloc(sizeof(NurlHeaderList));
    if (!h) return NULL;
    h->entries = NULL;
    h->count = 0;
    h->capacity = 0;
    return h;
}

nurl_err_t nurl_headers_add(NurlHeaderList *h, const char *key, const char *value) {
    if (!h || !key || !value) return NURL_ERR_GENERIC;

    if (h->count >= h->capacity) {
        size_t new_cap = h->capacity == 0 ? 8 : h->capacity * 2;
        char **new_entries = realloc(h->entries, new_cap * sizeof(char *));
        if (!new_entries) return NURL_ERR_OOM;
        h->entries = new_entries;
        h->capacity = new_cap;
    }

    size_t needed = strlen(key) + strlen(value) + 5; // ": \r\n\0"
    char *entry = malloc(needed);
    if (!entry) return NURL_ERR_OOM;

    snprintf(entry, needed, "%s: %s\r\n", key, value);
    h->entries[h->count++] = entry;
    return NURL_OK;
}

nurl_err_t nurl_headers_add_raw(NurlHeaderList *h, const char *line) {
    if (!h || !line) return NURL_ERR_GENERIC;

    if (h->count >= h->capacity) {
        size_t new_cap = h->capacity == 0 ? 8 : h->capacity * 2;
        char **new_entries = realloc(h->entries, new_cap * sizeof(char *));
        if (!new_entries) return NURL_ERR_OOM;
        h->entries = new_entries;
        h->capacity = new_cap;
    }

    // Ensure ending with \r\n
    size_t len = strlen(line);
    char *entry = NULL;
    if (len >= 2 && line[len - 2] == '\r' && line[len - 1] == '\n') {
        entry = strdup(line);
    } else if (len >= 1 && line[len - 1] == '\n') {
        entry = malloc(len + 2);
        if (entry) {
            memcpy(entry, line, len - 1);
            entry[len - 1] = '\r';
            entry[len] = '\n';
            entry[len + 1] = '\0';
        }
    } else {
        entry = malloc(len + 3);
        if (entry) {
            snprintf(entry, len + 3, "%s\r\n", line);
        }
    }

    if (!entry) return NURL_ERR_OOM;
    h->entries[h->count++] = entry;
    return NURL_OK;
}

bool nurl_headers_has(const NurlHeaderList *h, const char *key) {
    if (!h || !key) return false;
    size_t key_len = strlen(key);

    for (size_t i = 0; i < h->count; i++) {
        char *colon = strchr(h->entries[i], ':');
        if (!colon) continue;
        size_t name_len = colon - h->entries[i];
        if (name_len == key_len && strncasecmp(h->entries[i], key, key_len) == 0) {
            return true;
        }
    }
    return false;
}

nurl_err_t nurl_headers_apply_auth(NurlHeaderList *h, const BaseArgs *a) {
    if (!h || !a) return NURL_ERR_GENERIC;
    if (a->no_auth) return NURL_OK;

    if (nurl_headers_has(h, "Authorization")) {
        return NURL_OK;
    }

    if (a->bearer || a->token) {
        const char *tok = a->bearer ? a->bearer : a->token;
        char auth_val[1024];
        snprintf(auth_val, sizeof(auth_val), "Bearer %s", tok);
        return nurl_headers_add(h, "Authorization", auth_val);
    } else if (a->user) {
        char *b64 = nurl_utils_base64_encode((const unsigned char *)a->user, strlen(a->user));
        if (!b64) return NURL_ERR_OOM;
        char auth_val[1024];
        snprintf(auth_val, sizeof(auth_val), "Basic %s", b64);
        free(b64);
        return nurl_headers_add(h, "Authorization", auth_val);
    }

    return NURL_OK;
}

nurl_err_t nurl_headers_apply_common(NurlHeaderList *h, const BaseArgs *a) {
    if (!h || !a) return NURL_ERR_GENERIC;

    if (a->user_agent && !nurl_headers_has(h, "User-Agent")) {
        nurl_err_t err = nurl_headers_add(h, "User-Agent", a->user_agent);
        if (err != NURL_OK) return err;
    }
    if (a->referer && !nurl_headers_has(h, "Referer")) {
        nurl_err_t err = nurl_headers_add(h, "Referer", a->referer);
        if (err != NURL_OK) return err;
    }
    if (a->cookie && !nurl_headers_has(h, "Cookie")) {
        nurl_err_t err = nurl_headers_add(h, "Cookie", a->cookie);
        if (err != NURL_OK) return err;
    }

    return NURL_OK;
}

char *nurl_headers_serialize(const NurlHeaderList *h) {
    if (!h || h->count == 0) {
        char *empty = malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }

    size_t total_len = 0;
    for (size_t i = 0; i < h->count; i++) {
        total_len += strlen(h->entries[i]);
    }

    char *buf = malloc(total_len + 1);
    if (!buf) return NULL;

    size_t offset = 0;
    for (size_t i = 0; i < h->count; i++) {
        size_t len = strlen(h->entries[i]);
        memcpy(buf + offset, h->entries[i], len);
        offset += len;
    }
    buf[total_len] = '\0';
    return buf;
}

void nurl_headers_free(NurlHeaderList *h) {
    if (!h) return;
    if (h->entries) {
        for (size_t i = 0; i < h->count; i++) {
            free(h->entries[i]);
        }
        free(h->entries);
    }
    free(h);
}
