#ifndef SONET_BUF_H
#define SONET_BUF_H

#include <stddef.h>
#include <stdbool.h>

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} SonetBuf;

void  sonet_buf_init(SonetBuf *b);
bool  sonet_buf_append(SonetBuf *b, const char *s, size_t n);
bool  sonet_buf_printf(SonetBuf *b, const char *fmt, ...);
void  sonet_buf_free(SonetBuf *b);
char *sonet_buf_take(SonetBuf *b);

#endif /* SONET_BUF_H */
