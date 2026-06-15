#include "commands.h"
#include "nurl_net.h"
#include "nurl_tls.h"
#include "nurl_utils.h"
#include "nurl_http.h"
#include "nurl_engine.h"
#include "request.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>

int nurl_cmd_upload(const char *url, const CommonArgs *common) {
    if (!common->upload_file) {
        fprintf(stderr, "nurl: (1) No upload file specified.\n");
        return NURL_ERR_GENERIC;
    }

    struct stat st;
    if (stat(common->upload_file, &st) != 0 || !S_ISREG(st.st_mode)) {
        fprintf(stderr, "nurl: (6) Could not read upload file '%s'\n", common->upload_file);
        return NURL_ERR_WRITE;
    }
    long fsize = st.st_size;

    const char *file_name = strrchr(common->upload_file, '/');
    if (file_name) {
        file_name++;
    } else {
        file_name = common->upload_file;
    }

    const char *mime_type = common->upload_mime;
    if (!mime_type) {
        const char *ext = strrchr(file_name, '.');
        if (ext) {
            ext++; // skip '.'
            if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0) {
                mime_type = "image/jpeg";
            } else if (strcasecmp(ext, "png") == 0) {
                mime_type = "image/png";
            } else if (strcasecmp(ext, "gif") == 0) {
                mime_type = "image/gif";
            } else if (strcasecmp(ext, "pdf") == 0) {
                mime_type = "application/pdf";
            } else if (strcasecmp(ext, "txt") == 0) {
                mime_type = "text/plain";
            } else if (strcasecmp(ext, "json") == 0) {
                mime_type = "application/json";
            } else if (strcasecmp(ext, "html") == 0 || strcasecmp(ext, "htm") == 0) {
                mime_type = "text/html";
            } else {
                mime_type = "application/octet-stream";
            }
        } else {
            mime_type = "application/octet-stream";
        }
    }

    const char *boundary = "------------------------nurlboundary1234567890";

    // Build multipart preamble in a dynamic memory buffer
    size_t preamble_cap = 4096;
    for (size_t i = 0; i < common->upload_fields_count; i++) {
        preamble_cap += strlen(common->upload_fields[i]) + 256;
    }
    char *preamble = malloc(preamble_cap);
    if (!preamble) {
        return NURL_ERR_OOM;
    }
    size_t preamble_len = 0;

    // 1. Add form fields
    for (size_t i = 0; i < common->upload_fields_count; i++) {
        char *field_copy = strdup(common->upload_fields[i]);
        if (!field_copy) continue;
        char *eq = strchr(field_copy, '=');
        if (eq) {
            *eq = '\0';
            char *k = nurl_utils_trim(field_copy);
            char *v = nurl_utils_trim(eq + 1);

            preamble_len += snprintf(preamble + preamble_len, preamble_cap - preamble_len, "--%s\r\n", boundary);
            preamble_len += snprintf(preamble + preamble_len, preamble_cap - preamble_len, "Content-Disposition: form-data; name=\"%s\"\r\n\r\n", k);
            preamble_len += snprintf(preamble + preamble_len, preamble_cap - preamble_len, "%s\r\n", v);
        }
        free(field_copy);
    }

    // 2. Add file part header
    preamble_len += snprintf(preamble + preamble_len, preamble_cap - preamble_len, "--%s\r\n", boundary);
    preamble_len += snprintf(preamble + preamble_len, preamble_cap - preamble_len,
        "Content-Disposition: form-data; name=\"%s\"; filename=\"%s\"\r\n",
        common->upload_name ? common->upload_name : "file", file_name);
    preamble_len += snprintf(preamble + preamble_len, preamble_cap - preamble_len, "Content-Type: %s\r\n\r\n", mime_type);

    // Build multipart epilogue
    // Wait, the boundary is close boundary: "--boundary--\r\n"
    char epilogue_buf[128];
    snprintf(epilogue_buf, sizeof(epilogue_buf), "\r\n--%s--\r\n", boundary);

    // Set up body parts
    NurlBodyPart parts[3];
    parts[0].type = NURL_BODY_PART_MEM;
    parts[0].data = (const uint8_t *)preamble;
    parts[0].len = preamble_len;
    parts[0].filepath = NULL;

    parts[1].type = NURL_BODY_PART_FILE;
    parts[1].data = NULL;
    parts[1].len = fsize;
    parts[1].filepath = common->upload_file;

    parts[2].type = NURL_BODY_PART_MEM;
    parts[2].data = (const uint8_t *)epilogue_buf;
    parts[2].len = strlen(epilogue_buf);
    parts[2].filepath = NULL;

    // Create and execute NurlRequest
    NurlRequest *req = nurl_request_new();
    if (!req) {
        free(preamble);
        return NURL_ERR_OOM;
    }

    nurl_request_from_args(req, "POST", url, common);
    
    // Add Content-Type multipart header
    char ct_val[256];
    snprintf(ct_val, sizeof(ct_val), "multipart/form-data; boundary=%s", boundary);
    nurl_headermap_set(req->headers, "Content-Type", ct_val);

    req->body_parts = malloc(sizeof(NurlBodyPart) * 3);
    if (!req->body_parts) {
        free(preamble);
        nurl_request_free(req);
        return NURL_ERR_OOM;
    }
    memcpy(req->body_parts, parts, sizeof(NurlBodyPart) * 3);
    req->body_parts_count = 3;

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

        engine_err = nurl_engine_execute_request(req, &res, &effective_url);

        if (engine_err == NURL_OK && res) {
            if (res->status_code < 500) {
                break;
            } else {
                if (attempt < max_retries && !common->silent) {
                    fprintf(stderr, "nurl: Warning: HTTP %d. Retrying in %lu seconds...\n", res->status_code, delay_sec);
                }
            }
        } else {
            if (attempt < max_retries && !common->silent) {
                fprintf(stderr, "nurl: Warning: Request failed (error %d). Retrying in %lu seconds...\n", engine_err, delay_sec);
            }
        }

        if (attempt < max_retries) {
            sleep(delay_sec);
        }
    }

    free(preamble);

    if (engine_err != NURL_OK) {
        if (effective_url) free(effective_url);
        nurl_request_free(req);
        return engine_err;
    }

    if (!res) {
        if (effective_url) free(effective_url);
        nurl_request_free(req);
        return NURL_ERR_GENERIC;
    }

    bool should_suppress_output = (common->fail && res->status_code >= 400);
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
            FILE *out_f = NULL;
            if (is_stdout) {
                out_f = stdout;
            } else {
                out_f = fopen(common->output, "wb");
            }
            if (!out_f) {
                fprintf(stderr, "nurl: (6) Could not open file for writing: %s\n", common->output);
                nurl_http_response_free(res);
                free(effective_url);
                nurl_request_free(req);
                return NURL_ERR_WRITE;
            }
            if (res->body_len > 0) {
                fwrite(res->body, 1, res->body_len, out_f);
            }
            if (!is_stdout) {
                fclose(out_f);
            } else {
                fflush(out_f);
            }
        } else {
            if (res->body_len > 0) {
                fwrite(res->body, 1, res->body_len, stdout);
            }
        }
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
    nurl_request_free(req);
    return ret_code;
}
