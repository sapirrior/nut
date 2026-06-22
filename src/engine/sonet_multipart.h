#ifndef SONET_MULTIPART_H
#define SONET_MULTIPART_H

#include "engine/sonet_engine_request.h"

typedef struct SonetMultipart SonetMultipart;

SonetMultipart *sonet_multipart_new(void);
void sonet_multipart_add_file(SonetMultipart *m, const char *field_name,
                             const char *filepath, const char *mime_type);
void sonet_multipart_add_field(SonetMultipart *m, const char *name, const char *value);

// Returns Content-Type header value ("multipart/form-data; boundary=...")
const char *sonet_multipart_content_type(const SonetMultipart *m);

void sonet_multipart_into_request(SonetMultipart *m, SonetRequest *req);
void sonet_multipart_free(SonetMultipart *m);

#endif /* SONET_MULTIPART_H */
