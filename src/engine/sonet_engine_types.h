#ifndef SONET_ENGINE_TYPES_H
#define SONET_ENGINE_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* HTTP Response structure */
typedef struct {
    int status_code;
    char *status_text;
    char **headers;
    size_t header_count;
    unsigned char *body;
    size_t body_len;
} sonet_http_response_t;

/* Multipart/Body parts */
typedef enum {
    SONET_BODY_PART_MEM,
    SONET_BODY_PART_FILE
} SonetBodyPartType;

typedef struct {
    SonetBodyPartType type;
    const uint8_t *data;
    size_t len;
    const char *filepath;
} SonetBodyPart;

/* Progress callback */
typedef void (*sonet_progress_cb)(unsigned long downloaded, unsigned long total, bool finished, void *user_data);

#endif /* SONET_ENGINE_TYPES_H */
