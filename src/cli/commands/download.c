#include "commands.h"
#include "nurl_net.h"
#include "nurl_tls.h"
#include "nurl_utils.h"
#include "nurl_http.h"
#include "nurl_engine.h"
#include "request.h"
#include "errors/nurl_error_handler.h"
#include "errors/nurl_diag.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <ctype.h>

#include "nurl_progress.h"

int nurl_cmd_download(const char *url, const CommonArgs *common) {
    char *scheme = NULL;
    char *host = NULL;
    char *path = NULL;
    int port = 0;

    if (nurl_utils_parse_url(url, &scheme, &host, &port, &path) != 0) {
        nurl_diag_err("malformed URL '%s' provided for download.", url);
        nurl_diag_hint("ensure the URL uses a supported scheme like 'https://' and has a valid hostname.");
        return NURL_ERR_INVALID_URL;
    }

    // Determine output filename
    char *filename = NULL;
    if (common->output) {
        filename = strdup(common->output);
    } else {
        char *last_slash = strrchr(path, '/');
        if (last_slash && strlen(last_slash) > 1) {
            // Remove query parameters if present in filename
            char *q = strchr(last_slash + 1, '?');
            if (q) *q = '\0';
            filename = strdup(last_slash + 1);
            if (q) *q = '?'; // Restore
        } else {
            filename = strdup("download");
        }
    }

    if (!filename) {
        free(scheme); free(host); free(path);
        return NURL_ERR_GENERIC;
    }

    bool is_stdout = (strcmp(filename, "-") == 0);
    unsigned long start_pos = 0;

    if (common->resume && !is_stdout) {
        struct stat st;
        if (stat(filename, &st) == 0 && S_ISREG(st.st_mode)) {
            start_pos = (unsigned long)st.st_size;
        }
    }

    // Build the request
    NurlRequest *req = nurl_request_new();
    if (!req) {
        free(filename); free(scheme); free(host); free(path);
        return NURL_ERR_OOM;
    }

    nurl_request_from_args(req, "GET", url, common);

    NurlProgressCtx p_ctx;
    if (common->progress) {
        p_ctx.resume_offset = 0; // Will be updated per attempt
        p_ctx.silent = common->silent;
        gettimeofday(&p_ctx.start_time, NULL);
        p_ctx.last_update = p_ctx.start_time;
        req->progress_cb = nurl_progress_update;
        req->progress_data = &p_ctx;
    }

    // Verbose notice
    if (!common->silent) {
        fprintf(stderr, "* Saving to: %s\n", filename);
        if (start_pos > 0) {
            nurl_diag_hint("resuming download from offset: %lu", start_pos);
        }
    }

    unsigned int max_retries = common->retry;
    unsigned long delay_sec = common->retry_delay > 0 ? common->retry_delay : 1;
    int engine_err = NURL_OK;
    nurl_http_response_t *res = NULL;
    char *effective_url = NULL;

    for (unsigned int attempt = 0; attempt <= max_retries; attempt++) {
        if (res) {
            nurl_http_response_free(res);
            res = NULL;
        }
        if (effective_url) {
            free(effective_url);
            effective_url = NULL;
        }

        // Determine resume offset for this attempt
        unsigned long current_offset = 0;
        if (!is_stdout) {
            struct stat st;
            if (stat(filename, &st) == 0 && S_ISREG(st.st_mode)) {
                current_offset = (unsigned long)st.st_size;
            }
        } else {
            current_offset = start_pos;
        }

        // Open/reopen the output file
        FILE *out = NULL;
        if (is_stdout) {
            out = stdout;
        } else {
            out = fopen(filename, (current_offset > 0) ? "ab" : "wb");
        }
        if (!out) {
            nurl_diag_err("could not open local file '%s' for writing.", filename);
            nurl_diag_hint("check your file permissions or verify that the directory exists.");
            nurl_request_free(req);
            free(filename); free(scheme); free(host); free(path);
            return NURL_ERR_WRITE;
        }

        req->out = out;
        req->resume_offset = current_offset;
        if (common->progress) {
            p_ctx.resume_offset = current_offset;
        }

        engine_err = nurl_engine_execute_request(req, &res, &effective_url);

        if (!is_stdout) {
            fclose(out);
        } else {
            fflush(out);
        }

        if (engine_err == NURL_OK && res) {
            if (res->status_code < 500) {
                // Success, break out of retry loop
                break;
            } else {
                // 5xx error, retry
                if (attempt < max_retries && !common->silent) {
                    fprintf(stderr, "nurl: Warning: HTTP %d. Retrying in %lu seconds...\n", res->status_code, delay_sec);
                }
            }
        } else {
            // Network failure, retry
            if (attempt < max_retries && !common->silent) {
                fprintf(stderr, "nurl: Warning: Request failed (error %d). Retrying in %lu seconds...\n", engine_err, delay_sec);
            }
        }

        if (attempt < max_retries) {
            sleep(delay_sec);
        }
    }

    int ret_code = engine_err;
    if (engine_err == NURL_OK && res) {
        if (res->status_code >= 500) {
            ret_code = NURL_ERR_STATUS_5XX;
        } else if (res->status_code >= 400) {
            ret_code = NURL_ERR_STATUS_4XX;
        }
    }

    if (engine_err != NURL_OK) {
        if (!common->silent) {
            nurl_handle_request_error(engine_err, req, effective_url ? effective_url : url);
        }
    } else if (res && res->status_code >= 400) {
        if (!common->silent) {
            nurl_handle_request_error((res->status_code >= 500) ? NURL_ERR_HTTP_5XX : NURL_ERR_HTTP_4XX, req, effective_url ? effective_url : url);
        }
    }

    if (res) {
        nurl_http_response_free(res);
    }
    if (effective_url) {
        free(effective_url);
    }
    nurl_request_free(req);
    free(filename); free(scheme); free(host); free(path);

    return ret_code;
}
