#ifndef NURL_MULTIPART_H
#define NURL_MULTIPART_H

#include "engine/nurl_engine_request.h"

typedef struct NurlMultipart NurlMultipart;

NurlMultipart *nurl_multipart_new(void);
void nurl_multipart_add_file(NurlMultipart *m, const char *field_name,
                             const char *filepath, const char *mime_type);
void nurl_multipart_add_field(NurlMultipart *m, const char *name, const char *value);

// Returns Content-Type header value ("multipart/form-data; boundary=...")
const char *nurl_multipart_content_type(const NurlMultipart *m);

void nurl_multipart_into_request(NurlMultipart *m, NurlRequest *req);
void nurl_multipart_free(NurlMultipart *m);

#endif /* NURL_MULTIPART_H */
