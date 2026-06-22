#ifndef SONET_COOKIES_H
#define SONET_COOKIES_H

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
} sonet_cookie_t;

typedef struct {
    sonet_cookie_t *cookies;
    size_t count;
    size_t capacity;
} sonet_cookie_jar_t;

/**
 * Creates an empty cookie jar.
 */
sonet_cookie_jar_t *sonet_cookie_jar_create(void);

/**
 * Loads cookies from a Netscape HTTP Cookie format file.
 * Returns a new cookie jar on success, or NULL if file doesn't exist or is invalid.
 */
sonet_cookie_jar_t *sonet_cookie_jar_load(const char *filepath);

/**
 * Saves a cookie jar to a file in Netscape HTTP Cookie format.
 * Returns 0 on success, non-zero on error.
 */
int sonet_cookie_jar_save(const sonet_cookie_jar_t *jar, const char *filepath);

/**
 * Adds or updates a cookie in the jar. If matching domain and name exists, replaces it.
 */
void sonet_cookie_jar_add(sonet_cookie_jar_t *jar, const sonet_cookie_t *cookie);

/**
 * Frees all cookie jar allocations.
 */
void sonet_cookie_jar_free(sonet_cookie_jar_t *jar);

#endif /* SONET_COOKIES_H */
