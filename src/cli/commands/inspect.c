#include "commands.h"
#include "nurl_utils.h"
#include "errors/nurl_diag.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

int nurl_cmd_inspect(const char *url, const CommonArgs *common) {
    char *scheme = NULL;
    char *host = NULL;
    char *path = NULL;
    int port = 0;

    if (nurl_utils_parse_url(url, &scheme, &host, &port, &path) != 0) {
        nurl_diag_err("malformed URL '%s' provided for inspection.", url);
        nurl_diag_hint("ensure the URL uses a supported scheme like 'https://' and has a valid hostname.");
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
            if (!nurl_utils_has_header(common->header, common->header_count, "Authorization")) {
                printf("> Authorization: [hidden]\n");
            }
        } else if (common->user) {
            if (!nurl_utils_has_header(common->header, common->header_count, "Authorization")) {
                printf("> Authorization: [hidden]\n");
            }
        }
    }

    // 3. JSON Header
    if (common->json && !nurl_utils_has_header(common->header, common->header_count, "Content-Type")) {
        printf("> Content-Type: application/json\n");
    }

    // 4. Content-Length Header
    size_t body_len = 0;
    if (common->data) {
        body_len = common->data_len > 0 ? common->data_len : strlen(common->data);
    }
    if (body_len > 0 && !nurl_utils_has_header(common->header, common->header_count, "Content-Length")) {
        printf("> Content-Length: %zu\n", body_len);
    }

    printf(">\n");

    // Print Body
    if (common->data && body_len > 0) {
        fwrite(common->data, 1, body_len, stdout);
        printf("\n");
    }

    free(scheme);
    free(host);
    free(path);
    return 0;
}
