#include "sonet_headers.h"
#include "sonet_buf.h"
#include "compat/sonet_compat.h"
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

SonetHeaderMap *sonet_headermap_new(void) {
    SonetHeaderMap *m = malloc(sizeof(SonetHeaderMap));
    if (!m) return NULL;
    m->keys = NULL;
    m->values = NULL;
    m->count = 0;
    m->capacity = 0;
    return m;
}

static sonet_err_t headermap_grow(SonetHeaderMap *m) {
    if (m->count >= m->capacity) {
        size_t new_cap = m->capacity == 0 ? 8 : m->capacity * 2;
        char **new_keys = realloc(m->keys, new_cap * sizeof(char *));
        if (!new_keys) return SONET_ERR_OOM;
        char **new_values = realloc(m->values, new_cap * sizeof(char *));
        if (!new_values) {
            // Keep keys block if possible, but don't commit to the new size/capacity
            // Reallocating new_keys back to original size is optional, but leaving it grown
            // is fine as long as m->keys is updated to new_keys (otherwise it would leak or be dangling).
            // Actually, if new_values fails, we shouldn't update capacity.
            // Let's keep new_keys since it succeeded, but we must update m->keys to new_keys
            // so we don't have a dangling/lost pointer, or we could just roll back.
            // A simpler way: update m->keys to new_keys so it's not lost.
            m->keys = new_keys;
            return SONET_ERR_OOM;
        }
        m->keys = new_keys;
        m->values = new_values;
        m->capacity = new_cap;
    }
    return SONET_OK;
}

sonet_err_t sonet_headermap_set(SonetHeaderMap *m, const char *key, const char *value) {
    if (!m || !key || !value) return SONET_ERR_GENERIC;

    char *canon_key = strdup(key);
    if (!canon_key) return SONET_ERR_OOM;
    canonicalize_header_key(canon_key);

    for (size_t i = 0; i < m->count; i++) {
        if (sonet_strcasecmp(m->keys[i], canon_key) == 0) {
            free(canon_key);
            char *new_val = strdup(value);
            if (!new_val) return SONET_ERR_OOM;
            free(m->values[i]);
            m->values[i] = new_val;
            return SONET_OK;
        }
    }

    if (headermap_grow(m) != SONET_OK) {
        free(canon_key);
        return SONET_ERR_OOM;
    }

    char *val_copy = strdup(value);
    if (!val_copy) {
        free(canon_key);
        return SONET_ERR_OOM;
    }

    m->keys[m->count] = canon_key;
    m->values[m->count] = val_copy;
    m->count++;
    return SONET_OK;
}

sonet_err_t sonet_headermap_append(SonetHeaderMap *m, const char *key, const char *value) {
    if (!m || !key || !value) return SONET_ERR_GENERIC;

    char *canon_key = strdup(key);
    if (!canon_key) return SONET_ERR_OOM;
    canonicalize_header_key(canon_key);

    if (headermap_grow(m) != SONET_OK) {
        free(canon_key);
        return SONET_ERR_OOM;
    }

    char *val_copy = strdup(value);
    if (!val_copy) {
        free(canon_key);
        return SONET_ERR_OOM;
    }

    m->keys[m->count] = canon_key;
    m->values[m->count] = val_copy;
    m->count++;
    return SONET_OK;
}

bool sonet_headermap_has(const SonetHeaderMap *m, const char *key) {
    if (!m || !key) return false;
    for (size_t i = 0; i < m->count; i++) {
        if (sonet_strcasecmp(m->keys[i], key) == 0) {
            return true;
        }
    }
    return false;
}

char *sonet_headermap_serialize(const SonetHeaderMap *m) {
    if (!m) return NULL;
    SonetBuf b;
    sonet_buf_init(&b);
    for (size_t i = 0; i < m->count; i++) {
        sonet_buf_printf(&b, "%s: %s\r\n", m->keys[i], m->values[i]);
    }
    return sonet_buf_take(&b);
}

void sonet_headermap_free(SonetHeaderMap *m) {
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

sonet_err_t sonet_headermap_add_raw(SonetHeaderMap *m, const char *line) {
    if (!m || !line) return SONET_ERR_GENERIC;
    const char *colon = strchr(line, ':');
    if (!colon) return SONET_ERR_GENERIC;

    size_t key_len = colon - line;
    char *key = malloc(key_len + 1);
    if (!key) return SONET_ERR_OOM;
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
        return SONET_ERR_OOM;
    }
    memcpy(value, val, val_len);
    value[val_len] = '\0';

    sonet_err_t err = sonet_headermap_append(m, key, value);
    free(key);
    free(value);
    return err;
}
