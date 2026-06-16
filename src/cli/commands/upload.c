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
#include <strings.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <ctype.h>

#include "nurl_progress.h"

int nurl_cmd_upload(const char *url, const CommonArgs *common) {
    if (!common->upload_file) {
        nurl_diag_err("no upload file specified.");
        nurl_diag_hint("use the --upload flag followed by the path to the file you wish to send.");
        return NURL_ERR_GENERIC;
    }

    struct stat st;
    if (stat(common->upload_file, &st) != 0 || !S_ISREG(st.st_mode)) {
        nurl_diag_err("could not read local upload file '%s'.", common->upload_file);
        nurl_diag_hint("ensure the file exists and you have proper read permissions.");
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
    char epilogue[256];
    size_t epilogue_len = snprintf(epilogue, sizeof(epilogue), "\r\n--%s--\r\n", boundary);

    NurlBodyPart parts[3];
    memset(parts, 0, sizeof(parts));

    parts[0].type = NURL_BODY_PART_MEM;
    parts[0].data = (uint8_t *)preamble;
    parts[0].len  = preamble_len;

    parts[1].type     = NURL_BODY_PART_FILE;
    parts[1].filepath = strdup(common->upload_file);
    parts[1].len      = (size_t)fsize;

    parts[2].type = NURL_BODY_PART_MEM;
    parts[2].data = (uint8_t *)strdup(epilogue);
    parts[2].len  = epilogue_len;

    NurlRequest *req = nurl_request_new();
    if (!req) {
        free(preamble);
        free((void *)parts[1].filepath);
        free((void *)parts[2].data);
        return NURL_ERR_OOM;
    }

    nurl_request_from_args(req, "POST", url, common);

    NurlProgressCtx p_ctx;
    if (common->progress) {
        p_ctx.resume_offset = 0;
        p_ctx.silent = common->silent;
        gettimeofday(&p_ctx.start_time, NULL);
        p_ctx.last_update = p_ctx.start_time;
        req->progress_cb = nurl_progress_update;
        req->progress_data = &p_ctx;
    }
    
    char content_type[128];
    snprintf(content_type, sizeof(content_type), "multipart/form-data; boundary=%s", boundary);
    if (req->headers) {
        nurl_headermap_set(req->headers, "Content-Type", content_type);
    }

    req->body_parts = malloc(sizeof(NurlBodyPart) * 3);
    if (!req->body_parts) {
        nurl_request_free(req);
        free(preamble);
        free((void *)parts[1].filepath);
        free((void *)parts[2].data);
        return NURL_ERR_OOM;
    }
    memcpy(req->body_parts, parts, sizeof(NurlBodyPart) * 3);
    req->body_parts_count = 3;

    nurl_http_response_t *res = NULL;
    char *effective_url = NULL;

    if (!common->silent) {
        fprintf(stderr, "* Uploading %s (%ld bytes)\n", common->upload_file, fsize);
    }

    int err = nurl_engine_execute_request(req, &res, &effective_url);

    if (err != NURL_OK) {
        if (!common->silent) {
            nurl_handle_request_error(err, req, effective_url ? effective_url : url);
        }
    } else if (res && res->status_code >= 400) {
        if (!common->silent) {
            nurl_handle_request_error((res->status_code >= 500) ? NURL_ERR_HTTP_5XX : NURL_ERR_HTTP_4XX, req, effective_url ? effective_url : url);
        }
    }

    int ret_code = err;
    if (err == NURL_OK && res) {
        if (res->status_code >= 500) {
            ret_code = NURL_ERR_STATUS_5XX;
        } else if (res->status_code >= 400) {
            ret_code = NURL_ERR_STATUS_4XX;
        }
        
        if (!common->silent && !common->fail) {
            if (res->body_len > 0) {
                fwrite(res->body, 1, res->body_len, stdout);
            }
        }
    }

    nurl_request_free(req);
    if (res) nurl_http_response_free(res);
    if (effective_url) free(effective_url);
    free(preamble);
    // Note: nurl_request_free only frees req->body_parts array, not the pointers inside NurlBodyPart
    // We should probably fix nurl_request_free to handle this or free here.
    // For now, let's free them here to be safe.
    free((void *)parts[1].filepath);
    free((void *)parts[2].data);

    return ret_code;
}
