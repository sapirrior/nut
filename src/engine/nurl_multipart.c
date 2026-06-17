#include "nurl_multipart.h"
#include "utils/nurl_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    char *name;
    char *value;
    char *filepath;
    char *mime;
    bool is_file;
} MultipartPart;

struct NurlMultipart {
    char boundary[64];
    MultipartPart *parts;
    size_t count;
    char *cached_content_type;
};

NurlMultipart *nurl_multipart_new(void) {
    NurlMultipart *m = calloc(1, sizeof(NurlMultipart));
    if (!m) return NULL;

    // Generate a random boundary
    unsigned int r1 = (unsigned int)time(NULL);
    unsigned int r2 = (unsigned int)rand();
    snprintf(m->boundary, sizeof(m->boundary), "------------------------%08x%08x", r1, r2);

    return m;
}

void nurl_multipart_add_file(NurlMultipart *m, const char *field_name, const char *filepath, const char *mime_type) {
    m->parts = realloc(m->parts, sizeof(MultipartPart) * (m->count + 1));
    MultipartPart *p = &m->parts[m->count];
    p->name = strdup(field_name);
    p->value = NULL;
    p->filepath = strdup(filepath);
    p->mime = mime_type ? strdup(mime_type) : strdup("application/octet-stream");
    p->is_file = true;
    m->count++;
}

void nurl_multipart_add_field(NurlMultipart *m, const char *name, const char *value) {
    m->parts = realloc(m->parts, sizeof(MultipartPart) * (m->count + 1));
    MultipartPart *p = &m->parts[m->count];
    p->name = strdup(name);
    p->value = strdup(value);
    p->filepath = NULL;
    p->mime = NULL;
    p->is_file = false;
    m->count++;
}

const char *nurl_multipart_content_type(const NurlMultipart *m) {
    if (((NurlMultipart *)m)->cached_content_type) free(((NurlMultipart *)m)->cached_content_type);
    char buf[128];
    snprintf(buf, sizeof(buf), "multipart/form-data; boundary=%s", m->boundary);
    ((NurlMultipart *)m)->cached_content_type = strdup(buf);
    return m->cached_content_type;
}

void nurl_multipart_into_request(NurlMultipart *m, NurlRequest *req) {
    if (!m || !req) return;

    // We need 2 * m->count + 1 parts (prefix + body for each part + final boundary)
    req->body_parts = calloc(2 * m->count + 1, sizeof(NurlBodyPart));
    req->body_parts_count = 0;

    for (size_t i = 0; i < m->count; i++) {
        MultipartPart *p = &m->parts[i];
        char header[512];
        if (p->is_file) {
            const char *filename = strrchr(p->filepath, '/');
            filename = filename ? filename + 1 : p->filepath;
            snprintf(header, sizeof(header), "--%s\r\nContent-Disposition: form-data; name=\"%s\"; filename=\"%s\"\r\nContent-Type: %s\r\n\r\n",
                     m->boundary, p->name, filename, p->mime);
        } else {
            snprintf(header, sizeof(header), "--%s\r\nContent-Disposition: form-data; name=\"%s\"\r\n\r\n",
                     m->boundary, p->name);
        }

        // Add header part
        req->body_parts[req->body_parts_count].type = NURL_BODY_PART_MEM;
        req->body_parts[req->body_parts_count].data = (const uint8_t *)strdup(header);
        req->body_parts[req->body_parts_count].len = strlen(header);
        req->body_parts_count++;

        // Add body part
        if (p->is_file) {
            req->body_parts[req->body_parts_count].type = NURL_BODY_PART_FILE;
            req->body_parts[req->body_parts_count].filepath = strdup(p->filepath);
            // We don't know the length yet, or we should get it?
            // Actually, nurl_http.c should handle FILE part length
        } else {
            req->body_parts[req->body_parts_count].type = NURL_BODY_PART_MEM;
            req->body_parts[req->body_parts_count].data = (const uint8_t *)strdup(p->value);
            req->body_parts[req->body_parts_count].len = strlen(p->value);
        }
        req->body_parts_count++;
        
        // Add trailing CRLF for the part body
        req->body_parts = realloc(req->body_parts, sizeof(NurlBodyPart) * (req->body_parts_count + 1));
        req->body_parts[req->body_parts_count].type = NURL_BODY_PART_MEM;
        req->body_parts[req->body_parts_count].data = (const uint8_t *)strdup("\r\n");
        req->body_parts[req->body_parts_count].len = 2;
        req->body_parts_count++;
    }

    // Final boundary
    char final[128];
    snprintf(final, sizeof(final), "--%s--\r\n", m->boundary);
    req->body_parts = realloc(req->body_parts, sizeof(NurlBodyPart) * (req->body_parts_count + 1));
    req->body_parts[req->body_parts_count].type = NURL_BODY_PART_MEM;
    req->body_parts[req->body_parts_count].data = (const uint8_t *)strdup(final);
    req->body_parts[req->body_parts_count].len = strlen(final);
    req->body_parts_count++;

    nurl_headermap_set(req->headers, "Content-Type", nurl_multipart_content_type(m));
}

void nurl_multipart_free(NurlMultipart *m) {
    if (!m) return;
    for (size_t i = 0; i < m->count; i++) {
        free(m->parts[i].name);
        free(m->parts[i].value);
        free(m->parts[i].filepath);
        free(m->parts[i].mime);
    }
    free(m->parts);
    free(m->cached_content_type);
    free(m);
}
