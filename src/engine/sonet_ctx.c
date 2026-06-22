#include "sonet_ctx.h"
#include <stdlib.h>

SonetCtx *sonet_ctx_create(void) {
    SonetCtx *ctx = calloc(1, sizeof(SonetCtx));
    if (!ctx) return NULL;
    
    ctx->pool = sonet_pool_create();
    if (!ctx->pool) {
        free(ctx);
        return NULL;
    }
    
    return ctx;
}

void sonet_ctx_destroy(SonetCtx *ctx) {
    if (!ctx) return;
    if (ctx->pool) {
        sonet_pool_destroy(ctx->pool);
    }
    free(ctx);
}
