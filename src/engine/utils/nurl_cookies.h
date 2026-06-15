#ifndef NURL_COOKIES_H
#define NURL_COOKIES_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char *domain;
    bool include_subdomains;
    char *path;
    bool secure;
    unsigned long expiry;
    char *name;
    char *value;
} nurl_cookie_t;

typedef struct {
    nurl_cookie_t *cookies;
    size_t count;
    size_t capacity;
} nurl_cookie_jar_t;

/**
 * Creates an empty cookie jar.
 */
nurl_cookie_jar_t *nurl_cookie_jar_create(void);

/**
 * Loads cookies from a Netscape HTTP Cookie format file.
 * Returns a new cookie jar on success, or NULL if file doesn't exist or is invalid.
 */
nurl_cookie_jar_t *nurl_cookie_jar_load(const char *filepath);

/**
 * Saves a cookie jar to a file in Netscape HTTP Cookie format.
 * Returns 0 on success, non-zero on error.
 */
int nurl_cookie_jar_save(const nurl_cookie_jar_t *jar, const char *filepath);

/**
 * Adds or updates a cookie in the jar. If matching domain and name exists, replaces it.
 */
void nurl_cookie_jar_add(nurl_cookie_jar_t *jar, const nurl_cookie_t *cookie);

/**
 * Frees all cookie jar allocations.
 */
void nurl_cookie_jar_free(nurl_cookie_jar_t *jar);

#endif /* NURL_COOKIES_H */
