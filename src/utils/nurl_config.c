#include "nurl_config.h"
#include "nurl_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>

static void strip_quotes(char *str) {
    size_t len = strlen(str);
    if (len >= 2 && ((str[0] == '"' && str[len - 1] == '"') || (str[0] == '\'' && str[len - 1] == '\''))) {
        memmove(str, str + 1, len - 2);
        str[len - 2] = '\0';
    }
}

static bool has_header_key(char **headers, size_t count, const char *key) {
    size_t key_len = strlen(key);
    for (size_t i = 0; i < count; i++) {
        if (strncasecmp(headers[i], key, key_len) == 0 && headers[i][key_len] == ':') {
            return true;
        }
    }
    return false;
}

void nurl_config_load_and_merge(CommonArgs *args) {
    char *config_path = getenv("NURL_CONFIG");
    char *allocated_path = NULL;

    if (!config_path) {
        char *home = getenv("HOME");
        if (home) {
            asprintf(&allocated_path, "%s/.config/nurl/config.toml", home);
            config_path = allocated_path;
        }
    }

    if (!config_path) return;

    FILE *f = fopen(config_path, "r");
    free(allocated_path);
    if (!f) return;

    char line[1024];
    int section = 0; // 0 = none, 1 = defaults, 2 = headers

    while (fgets(line, sizeof(line), f)) {
        char *trimmed = nurl_utils_trim(line);
        if (strlen(trimmed) == 0 || trimmed[0] == '#' || trimmed[0] == ';') {
            continue;
        }

        if (trimmed[0] == '[') {
            if (strcasecmp(trimmed, "[defaults]") == 0) {
                section = 1;
            } else if (strcasecmp(trimmed, "[headers]") == 0) {
                section = 2;
            } else {
                section = 0;
            }
            continue;
        }

        char *eq = strchr(trimmed, '=');
        if (eq) {
            *eq = '\0';
            char *key = nurl_utils_trim(trimmed);
            char *val = nurl_utils_trim(eq + 1);
            strip_quotes(val);

            if (section == 1) {
                // [defaults]
                if (strcasecmp(key, "timeout") == 0 && args->timeout == 30) {
                    args->timeout = strtoul(val, NULL, 10);
                } else if (strcasecmp(key, "connect_timeout") == 0 && args->connect_timeout == 10) {
                    args->connect_timeout = strtoul(val, NULL, 10);
                } else if (strcasecmp(key, "follow_redirects") == 0 && !args->location) {
                    if (strcasecmp(val, "true") == 0) {
                        args->location = true;
                    }
                } else if (strcasecmp(key, "user_agent") == 0 && !args->user_agent) {
                    args->user_agent = strdup(val);
                }
            } else if (section == 2) {
                // [headers]
                if (!has_header_key(args->header, args->header_count, key)) {
                    char *hdr_line = NULL;
                    if (asprintf(&hdr_line, "%s: %s", key, val) >= 0) {
                        char **temp = realloc(args->header, sizeof(char *) * (args->header_count + 1));
                        if (temp) {
                            args->header = temp;
                            args->header[args->header_count] = hdr_line;
                            args->header_count++;
                        } else {
                            free(hdr_line);
                        }
                    }
                }
            }
        }
    }

    fclose(f);
}
