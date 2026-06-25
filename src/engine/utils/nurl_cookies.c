#include "nurl_cookies.h"
#include "errors/nurl_diag.h"
#include "compat/nurl_compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

nurl_cookie_jar_t *nurl_cookie_jar_create(void) {
    nurl_cookie_jar_t *jar = calloc(1, sizeof(nurl_cookie_jar_t));
    return jar;
}

void nurl_cookie_jar_free(nurl_cookie_jar_t *jar) {
    if (!jar) return;
    for (size_t i = 0; i < jar->count; i++) {
        free(jar->cookies[i].domain);
        free(jar->cookies[i].path);
        free(jar->cookies[i].name);
        free(jar->cookies[i].value);
    }
    free(jar->cookies);
    free(jar);
}

nurl_cookie_jar_t *nurl_cookie_jar_load(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        if (errno != ENOENT) {
            nurl_diag_err("could not open cookie jar '%s' for reading: %s", path, strerror(errno));
        }
        return NULL;
    }

    nurl_cookie_jar_t *jar = nurl_cookie_jar_create();
    if (!jar) { fclose(f); return NULL; }

    char line[2048];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;

        char *parts[7];
        int part_count = 0;
        char *tok = strtok(line, "\t\n\r");
        while (tok && part_count < 7) {
            parts[part_count++] = tok;
            tok = strtok(NULL, "\t\n\r");
        }

        if (part_count >= 7) {
            nurl_cookie_t c;
            c.domain = strdup(parts[0]);
            c.include_subdomains = (nurl_strcasecmp(parts[1], "TRUE") == 0);
            c.path = strdup(parts[2]);
            c.secure = (nurl_strcasecmp(parts[3], "TRUE") == 0);
            c.expiry = strtoul(parts[4], NULL, 10);
            c.name = strdup(parts[5]);
            c.value = strdup(parts[6]);

            if (c.domain && c.path && c.name && c.value) {
                nurl_cookie_jar_add(jar, &c);
            }
            free(c.domain); free(c.path); free(c.name); free(c.value);
        }
    }

    fclose(f);
    return jar;
}

int nurl_cookie_jar_save(const nurl_cookie_jar_t *jar, const char *path) {
    if (!jar) return -1;
    FILE *f = fopen(path, "w");
    if (!f) {
        nurl_diag_err("could not open cookie jar '%s' for writing: %s", path, strerror(errno));
        return -1;
    }

    fprintf(f, "# Netscape HTTP Cookie File\n");
    for (size_t i = 0; i < jar->count; i++) {
        nurl_cookie_t *c = &jar->cookies[i];
        fprintf(f, "%s\t%s\t%s\t%s\t%lu\t%s\t%s\n",
            c->domain,
            c->include_subdomains ? "TRUE" : "FALSE",
            c->path,
            c->secure ? "TRUE" : "FALSE",
            (unsigned long)c->expiry,
            c->name,
            c->value);
    }
    fclose(f);
    return 0;
}

void nurl_cookie_jar_add(nurl_cookie_jar_t *jar, const nurl_cookie_t *cookie) {
    if (!jar || !cookie) return;

    // Check for existing cookie (same domain, path, name)
    for (size_t i = 0; i < jar->count; i++) {
        if (strcmp(jar->cookies[i].domain, cookie->domain) == 0 &&
            strcmp(jar->cookies[i].path, cookie->path) == 0 &&
            strcmp(jar->cookies[i].name, cookie->name) == 0) {
            
            free(jar->cookies[i].value);
            jar->cookies[i].value = strdup(cookie->value);
            jar->cookies[i].expiry = cookie->expiry;
            jar->cookies[i].secure = cookie->secure;
            return;
        }
    }

    nurl_cookie_t *temp = realloc(jar->cookies, sizeof(nurl_cookie_t) * (jar->count + 1));
    if (temp) {
        jar->cookies = temp;
        jar->cookies[jar->count].domain = strdup(cookie->domain);
        jar->cookies[jar->count].path = strdup(cookie->path);
        jar->cookies[jar->count].name = strdup(cookie->name);
        jar->cookies[jar->count].value = strdup(cookie->value);
        jar->cookies[jar->count].include_subdomains = cookie->include_subdomains;
        jar->cookies[jar->count].secure = cookie->secure;
        jar->cookies[jar->count].expiry = cookie->expiry;
        jar->count++;
    }
}
