#include "nurl_headers.h"
#include "nurl_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>



static void canonicalize_header_key(char *key) {
    bool cap_next = true;
    for (char *p = key; *p; p++) {
        if (*p == '-') {
            cap_next = true;
        } else if (cap_next) {
            *p = toupper((unsigned char)*p);
            cap_next = false;
        } else {
            *p = tolower((unsigned char)*p);
        }
    }
}

NurlHeaderMap *nurl_headermap_new(void) {
    NurlHeaderMap *m = malloc(sizeof(NurlHeaderMap));
    if (!m) return NULL;
    m->keys = NULL;
    m->values = NULL;
    m->count = 0;
    m->capacity = 0;
    return m;
}

static nurl_err_t headermap_grow(NurlHeaderMap *m) {
    if (m->count >= m->capacity) {
        size_t new_cap = m->capacity == 0 ? 8 : m->capacity * 2;
        char **new_keys = realloc(m->keys, new_cap * sizeof(char *));
        char **new_values = realloc(m->values, new_cap * sizeof(char *));
        if (!new_keys || !new_values) {
            if (new_keys) m->keys = new_keys;
            if (new_values) m->values = new_values;
            return NURL_ERR_OOM;
        }
        m->keys = new_keys;
        m->values = new_values;
        m->capacity = new_cap;
    }
    return NURL_OK;
}

nurl_err_t nurl_headermap_set(NurlHeaderMap *m, const char *key, const char *value) {
    if (!m || !key || !value) return NURL_ERR_GENERIC;

    char *canon_key = strdup(key);
    if (!canon_key) return NURL_ERR_OOM;
    canonicalize_header_key(canon_key);

    for (size_t i = 0; i < m->count; i++) {
        if (strcasecmp(m->keys[i], canon_key) == 0) {
            free(canon_key);
            char *new_val = strdup(value);
            if (!new_val) return NURL_ERR_OOM;
            free(m->values[i]);
            m->values[i] = new_val;
            return NURL_OK;
        }
    }

    if (headermap_grow(m) != NURL_OK) {
        free(canon_key);
        return NURL_ERR_OOM;
    }

    char *val_copy = strdup(value);
    if (!val_copy) {
        free(canon_key);
        return NURL_ERR_OOM;
    }

    m->keys[m->count] = canon_key;
    m->values[m->count] = val_copy;
    m->count++;
    return NURL_OK;
}

nurl_err_t nurl_headermap_append(NurlHeaderMap *m, const char *key, const char *value) {
    if (!m || !key || !value) return NURL_ERR_GENERIC;

    char *canon_key = strdup(key);
    if (!canon_key) return NURL_ERR_OOM;
    canonicalize_header_key(canon_key);

    if (headermap_grow(m) != NURL_OK) {
        free(canon_key);
        return NURL_ERR_OOM;
    }

    char *val_copy = strdup(value);
    if (!val_copy) {
        free(canon_key);
        return NURL_ERR_OOM;
    }

    m->keys[m->count] = canon_key;
    m->values[m->count] = val_copy;
    m->count++;
    return NURL_OK;
}

bool nurl_headermap_has(const NurlHeaderMap *m, const char *key) {
    if (!m || !key) return false;
    for (size_t i = 0; i < m->count; i++) {
        if (strcasecmp(m->keys[i], key) == 0) {
            return true;
        }
    }
    return false;
}

char *nurl_headermap_serialize(const NurlHeaderMap *m) {
    if (!m || m->count == 0) {
        char *empty = malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }

    size_t total_len = 0;
    for (size_t i = 0; i < m->count; i++) {
        total_len += strlen(m->keys[i]) + strlen(m->values[i]) + 4;
    }

    char *buf = malloc(total_len + 1);
    if (!buf) return NULL;

    size_t offset = 0;
    for (size_t i = 0; i < m->count; i++) {
        size_t len = sprintf(buf + offset, "%s: %s\r\n", m->keys[i], m->values[i]);
        offset += len;
    }
    buf[total_len] = '\0';
    return buf;
}

void nurl_headermap_free(NurlHeaderMap *m) {
    if (!m) return;
    if (m->keys) {
        for (size_t i = 0; i < m->count; i++) {
            free(m->keys[i]);
            free(m->values[i]);
        }
        free(m->keys);
        free(m->values);
    }
    free(m);
}

nurl_err_t nurl_headermap_add_raw(NurlHeaderMap *m, const char *line) {
    if (!m || !line) return NURL_ERR_GENERIC;
    const char *colon = strchr(line, ':');
    if (!colon) return NURL_ERR_GENERIC;

    size_t key_len = colon - line;
    char *key = malloc(key_len + 1);
    if (!key) return NURL_ERR_OOM;
    memcpy(key, line, key_len);
    key[key_len] = '\0';

    const char *val = colon + 1;
    while (*val && isspace((unsigned char)*val)) val++;
    
    size_t val_len = strlen(val);
    while (val_len > 0 && isspace((unsigned char)val[val_len - 1])) {
        val_len--;
    }
    char *value = malloc(val_len + 1);
    if (!value) {
        free(key);
        return NURL_ERR_OOM;
    }
    memcpy(value, val, val_len);
    value[val_len] = '\0';

    nurl_err_t err = nurl_headermap_append(m, key, value);
    free(key);
    free(value);
    return err;
}

nurl_err_t nurl_headermap_apply_auth(NurlHeaderMap *m, const CommonArgs *a) {
    if (!m || !a) return NURL_ERR_GENERIC;
    if (a->no_auth) return NURL_OK;

    if (nurl_headermap_has(m, "Authorization")) {
        return NURL_OK;
    }

    if (a->bearer || a->token) {
        const char *tok = a->bearer ? a->bearer : a->token;
        char auth_val[1024];
        snprintf(auth_val, sizeof(auth_val), "Bearer %s", tok);
        return nurl_headermap_set(m, "Authorization", auth_val);
    } else if (a->user) {
        char *b64 = nurl_utils_base64_encode((const unsigned char *)a->user, strlen(a->user));
        if (!b64) return NURL_ERR_OOM;
        char auth_val[1024];
        snprintf(auth_val, sizeof(auth_val), "Basic %s", b64);
        free(b64);
        return nurl_headermap_set(m, "Authorization", auth_val);
    }

    return NURL_OK;
}

nurl_err_t nurl_headermap_apply_common(NurlHeaderMap *m, const CommonArgs *a) {
    if (!m || !a) return NURL_ERR_GENERIC;

    if (a->user_agent && !nurl_headermap_has(m, "User-Agent")) {
        nurl_err_t err = nurl_headermap_set(m, "User-Agent", a->user_agent);
        if (err != NURL_OK) return err;
    }
    if (a->referer && !nurl_headermap_has(m, "Referer")) {
        nurl_err_t err = nurl_headermap_set(m, "Referer", a->referer);
        if (err != NURL_OK) return err;
    }
    if (a->cookie && !nurl_headermap_has(m, "Cookie")) {
        nurl_err_t err = nurl_headermap_set(m, "Cookie", a->cookie);
        if (err != NURL_OK) return err;
    }

    return NURL_OK;
}
