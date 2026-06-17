#ifndef NURL_CTX_H
#define NURL_CTX_H

#include "nurl_pool.h"

typedef struct {
    NurlConnPool *pool;
} NurlCtx;

NurlCtx *nurl_ctx_create(void);
void     nurl_ctx_destroy(NurlCtx *ctx);

#endif /* NURL_CTX_H */
