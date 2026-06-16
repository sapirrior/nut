#include "nurl_request.h"
#include "nurl_engine.h"
#include "request.h"
#include "nurl_utils.h"
#include "errors/nurl_error_handler.h"
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

static char *string_replace(const char *orig, const char *rep, const char *with) {
    char *result;
    const char *ins;
    char *tmp;
    int len_rep;
    int len_with;
    int len_front;
    int count;

    if (!orig || !rep)
        return NULL;
    len_rep = strlen(rep);
    if (len_rep == 0)
        return NULL;
    if (!with)
        with = "";
    len_with = strlen(with);

    ins = orig;
    for (count = 0; (tmp = strstr(ins, rep)); ++count) {
        ins = tmp + len_rep;
    }

    tmp = result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);
    if (!result)
        return NULL;

    while (count--) {
        ins = strstr(orig, rep);
        len_front = ins - orig;
        tmp = strncpy(tmp, orig, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        orig += len_front + len_rep;
    }
    strcpy(tmp, orig);
    return result;
}

static void handle_write_out(const char *template, const nurl_http_response_t *res, const char *method, const char *url, double elapsed_sec) {
    if (!template) return;

    char *result = strdup(template);
    if (!result) return;

    char http_code[16];
    snprintf(http_code, sizeof(http_code), "%d", res->status_code);
    char *temp = string_replace(result, "%{http_code}", http_code);
    if (temp) { free(result); result = temp; }

    char time_total[32];
    snprintf(time_total, sizeof(time_total), "%.3f", elapsed_sec);
    temp = string_replace(result, "%{time_total}", time_total);
    if (temp) { free(result); result = temp; }

    temp = string_replace(result, "%{time_connect}", "0.010");
    if (temp) { free(result); result = temp; }

    char size_download[32];
    snprintf(size_download, sizeof(size_download), "%zu", res->body_len);
    temp = string_replace(result, "%{size_download}", size_download);
    if (temp) { free(result); result = temp; }

    temp = string_replace(result, "%{url_effective}", url);
    if (temp) { free(result); result = temp; }

    const char *content_type = "";
    for (size_t i = 0; i < res->header_count; i++) {
        if (strncasecmp(res->headers[i], "Content-Type:", 13) == 0) {
            content_type = res->headers[i] + 13;
            while (*content_type && isspace((unsigned char)*content_type)) {
                content_type++;
            }
            break;
        }
    }
    temp = string_replace(result, "%{content_type}", content_type);
    if (temp) { free(result); result = temp; }

    temp = string_replace(result, "%{num_redirects}", "0");
    if (temp) { free(result); result = temp; }

    temp = string_replace(result, "%{method}", method);
    if (temp) { free(result); result = temp; }

    char *scheme = NULL;
    char *host = NULL;
    char *path = NULL;
    int port = 0;
    nurl_utils_parse_url(url, &scheme, &host, &port, &path);

    temp = string_replace(result, "%{scheme}", scheme ? scheme : "");
    if (temp) { free(result); result = temp; }

    temp = string_replace(result, "%{host}", host ? host : "");
    if (temp) { free(result); result = temp; }

    free(scheme);
    free(host);
    free(path);

    temp = string_replace(result, "\\n", "\n");
    if (temp) { free(result); result = temp; }
    temp = string_replace(result, "\\t", "\t");
    if (temp) { free(result); result = temp; }

    printf("%s", result);
    free(result);
}

#include "nurl_progress.h"

int nurl_request_generic(const char *method, const char *url, const CommonArgs *common) {
    double start_time = nurl_utils_get_time_sec();

    nurl_http_response_t *res = NULL;
    char *effective_url = NULL;

    NurlRequest *req = nurl_request_new();
    if (!req) return NURL_ERR_OOM;
    nurl_request_from_args(req, method, url, common);

    NurlProgressCtx p_ctx;
    if (common->progress) {
        p_ctx.resume_offset = 0;
        p_ctx.silent = common->silent;
        gettimeofday(&p_ctx.start_time, NULL);
        p_ctx.last_update = p_ctx.start_time;
        req->progress_cb = nurl_progress_update;
        req->progress_data = &p_ctx;
    }

    unsigned int max_retries = common->retry;
    unsigned long delay_sec = common->retry_delay > 0 ? common->retry_delay : 1;
    int engine_err = NURL_OK;

    for (unsigned int attempt = 0; attempt <= max_retries; attempt++) {
        if (res) {
            nurl_http_response_free(res);
            res = NULL;
        }
        if (effective_url) {
            free(effective_url);
            effective_url = NULL;
        }

        engine_err = nurl_engine_execute_request(req, &res, &effective_url);

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

    if (engine_err != NURL_OK) {
        if (!common->silent) {
            nurl_handle_request_error(engine_err, req, effective_url ? effective_url : url);
        }
        nurl_request_free(req);
        if (effective_url) free(effective_url);
        return engine_err;
    }

    nurl_request_free(req);

    if (!res) {
        if (effective_url) free(effective_url);
        return NURL_ERR_GENERIC;
    }

    bool is_error_status = (res->status_code >= 400);
    bool should_suppress_output = (common->fail && is_error_status);

    if (!should_suppress_output) {
        if (common->include && !common->verbose) {
            printf("HTTP/1.1 %d %s\n", res->status_code, res->status_text);
            for (size_t i = 0; i < res->header_count; i++) {
                printf("%s\n", res->headers[i]);
            }
            printf("\n");
        }

        if (common->output) {
            bool is_stdout = (strcmp(common->output, "-") == 0);
            FILE *f = NULL;
            if (is_stdout) {
                f = stdout;
            } else {
                f = fopen(common->output, "wb");
            }
            if (!f) {
                fprintf(stderr, "nurl: (6) Could not open file for writing: %s\n", common->output);
                nurl_http_response_free(res);
                free(effective_url);
                return NURL_ERR_WRITE;
            }
            if (res->body_len > 0) {
                fwrite(res->body, 1, res->body_len, f);
            }
            if (!is_stdout) {
                fclose(f);
            } else {
                fflush(f);
            }
        } else {
            if (res->body_len > 0) {
                fwrite(res->body, 1, res->body_len, stdout);
            }
        }
    }

    double elapsed_sec = nurl_utils_get_time_sec() - start_time;

    if (common->write_out && !should_suppress_output) {
        handle_write_out(common->write_out, res, method, effective_url ? effective_url : url, elapsed_sec);
    }

    int ret_code = NURL_OK;
    if (res->status_code >= 500) {
        ret_code = NURL_ERR_STATUS_5XX;
    } else if (res->status_code >= 400) {
        ret_code = NURL_ERR_STATUS_4XX;
    }

    nurl_http_response_free(res);
    if (effective_url) {
        free(effective_url);
    }
    return ret_code;
}
