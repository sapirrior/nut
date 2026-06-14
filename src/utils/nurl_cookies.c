#include "nurl_cookies.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

nurl_cookie_jar_t *nurl_cookie_jar_create(void) {
    nurl_cookie_jar_t *jar = malloc(sizeof(nurl_cookie_jar_t));
    if (!jar) return NULL;
    jar->cookies = NULL;
    jar->count = 0;
    jar->capacity = 0;
    return jar;
}

static void free_cookie_contents(nurl_cookie_t *c) {
    free(c->domain);
    free(c->path);
    free(c->name);
    free(c->value);
}

nurl_cookie_jar_t *nurl_cookie_jar_load(const char *filepath) {
    FILE *f = fopen(filepath, "r");
    if (!f) return NULL;

    nurl_cookie_jar_t *jar = nurl_cookie_jar_create();
    if (!jar) {
        fclose(f);
        return NULL;
    }

    char line[2048];
    while (fgets(line, sizeof(line), f)) {
        // Trim newline
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n')) {
            line[len - 1] = '\0';
            len--;
        }

        // Skip comments and empty lines
        if (len == 0 || line[0] == '#') {
            continue;
        }

        // Split by tab character
        char *parts[7];
        int part_count = 0;
        char *tok = line;
        while (part_count < 7) {
            char *tab = strchr(tok, '\t');
            if (tab) {
                *tab = '\0';
                parts[part_count++] = tok;
                tok = tab + 1;
            } else {
                parts[part_count++] = tok;
                break;
            }
        }

        if (part_count >= 7) {
            nurl_cookie_t c;
            c.domain = strdup(parts[0]);
            c.include_subdomains = (strcasecmp(parts[1], "TRUE") == 0);
            c.path = strdup(parts[2]);
            c.secure = (strcasecmp(parts[3], "TRUE") == 0);
            c.expiry = strtoul(parts[4], NULL, 10);
            c.name = strdup(parts[5]);
            c.value = strdup(parts[6]);

            if (c.domain && c.path && c.name && c.value) {
                nurl_cookie_jar_add(jar, &c);
            } else {
                free(c.domain); free(c.path); free(c.name); free(c.value);
            }
        }
    }

    fclose(f);
    return jar;
}

int nurl_cookie_jar_save(const nurl_cookie_jar_t *jar, const char *filepath) {
    if (!jar) return -1;
    FILE *f = fopen(filepath, "w");
    if (!f) return -1;

    fprintf(f, "# Netscape HTTP Cookie File\n");
    for (size_t i = 0; i < jar->count; i++) {
        nurl_cookie_t *c = &jar->cookies[i];
        fprintf(f, "%s\t%s\t%s\t%s\t%lu\t%s\t%s\n",
            c->domain,
            c->include_subdomains ? "TRUE" : "FALSE",
            c->path,
            c->secure ? "TRUE" : "FALSE",
            c->expiry,
            c->name,
            c->value);
    }

    fclose(f);
    return 0;
}

void nurl_cookie_jar_add(nurl_cookie_jar_t *jar, const nurl_cookie_t *cookie) {
    if (!jar || !cookie) return;

    // Search for existing cookie with same domain & name
    for (size_t i = 0; i < jar->count; i++) {
        nurl_cookie_t *c = &jar->cookies[i];
        if (strcmp(c->domain, cookie->domain) == 0 && strcmp(c->name, cookie->name) == 0) {
            free_cookie_contents(c);
            c->domain = strdup(cookie->domain);
            c->include_subdomains = cookie->include_subdomains;
            c->path = strdup(cookie->path);
            c->secure = cookie->secure;
            c->expiry = cookie->expiry;
            c->name = strdup(cookie->name);
            c->value = strdup(cookie->value);
            return;
        }
    }

    // Allocate / expand jar
    if (jar->count >= jar->capacity) {
        jar->capacity = jar->capacity == 0 ? 8 : jar->capacity * 2;
        nurl_cookie_t *temp = realloc(jar->cookies, sizeof(nurl_cookie_t) * jar->capacity);
        if (!temp) return;
        jar->cookies = temp;
    }

    nurl_cookie_t *c = &jar->cookies[jar->count];
    c->domain = strdup(cookie->domain);
    c->include_subdomains = cookie->include_subdomains;
    c->path = strdup(cookie->path);
    c->secure = cookie->secure;
    c->expiry = cookie->expiry;
    c->name = strdup(cookie->name);
    c->value = strdup(cookie->value);
    jar->count++;
}

void nurl_cookie_jar_free(nurl_cookie_jar_t *jar) {
    if (jar) {
        if (jar->cookies) {
            for (size_t i = 0; i < jar->count; i++) {
                free_cookie_contents(&jar->cookies[i]);
            }
            free(jar->cookies);
        }
        free(jar);
    }
}
