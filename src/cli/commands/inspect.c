#include "commands.h"
#include "nurl_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

static bool has_header(char **headers, size_t count, const char *key) {
    size_t key_len = strlen(key);
    for (size_t i = 0; i < count; i++) {
        if (strncasecmp(headers[i], key, key_len) == 0 && headers[i][key_len] == ':') {
            return true;
        }
    }
    return false;
}

int nurl_cmd_inspect(const char *url, const CommonArgs *common) {
    char *scheme = NULL;
    char *host = NULL;
    char *path = NULL;
    int port = 0;

    if (nurl_utils_parse_url(url, &scheme, &host, &port, &path) != 0) {
        fprintf(stderr, "Error: Invalid URL format: %s\n", url);
        return NURL_ERR_INVALID_URL;
    }

    // Print Request Line and Host
    printf("> %s %s HTTP/1.1\n", common->method ? common->method : "GET", path);
    printf("> Host: %s\n", host);
    printf("> User-Agent: nurl/" NURL_VERSION "\n");
    printf("> Connection: close\n");

    // 1. User specified headers
    for (size_t i = 0; i < common->header_count; i++) {
        char *line = strdup(common->header[i]);
        if (line) {
            char *colon = strchr(line, ':');
            if (colon) {
                *colon = '\0';
                char *key = line;
                char *val = colon + 1;
                while (*val && isspace((unsigned char)*val)) val++;
                const char *redacted = nurl_utils_redact_header(key, val);
                printf("> %s: %s\n", key, redacted);
            } else {
                printf("> %s\n", common->header[i]);
            }
            free(line);
        }
    }

    // 2. Auth Header
    if (!common->no_auth) {
        if (common->bearer || common->token) {
            if (!has_header(common->header, common->header_count, "Authorization")) {
                printf("> Authorization: [hidden]\n");
            }
        } else if (common->user) {
            if (!has_header(common->header, common->header_count, "Authorization")) {
                printf("> Authorization: [hidden]\n");
            }
        }
    }

    // 3. JSON Header
    if (common->json && !has_header(common->header, common->header_count, "Content-Type")) {
        printf("> Content-Type: application/json\n");
    }

    // 4. Content-Length Header
    size_t body_len = common->data ? strlen(common->data) : 0;
    if (body_len > 0 && !has_header(common->header, common->header_count, "Content-Length")) {
        printf("> Content-Length: %zu\n", body_len);
    }

    printf(">\n");

    // Print Body
    if (common->data && body_len > 0) {
        printf("%s\n", common->data);
    }

    free(scheme);
    free(host);
    free(path);
    return 0;
}
