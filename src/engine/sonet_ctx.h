#ifndef SONET_CTX_H
#define SONET_CTX_H

#include "sonet_pool.h"

typedef struct {
    SonetConnPool *pool;
} SonetCtx;

SonetCtx *sonet_ctx_create(void);
void     sonet_ctx_destroy(SonetCtx *ctx);

#endif /* SONET_CTX_H */
