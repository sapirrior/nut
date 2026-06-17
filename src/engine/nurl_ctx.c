#include "nurl_ctx.h"
#include <stdlib.h>

NurlCtx *nurl_ctx_create(void) {
    NurlCtx *ctx = calloc(1, sizeof(NurlCtx));
    if (!ctx) return NULL;
    
    ctx->pool = nurl_pool_create();
    if (!ctx->pool) {
        free(ctx);
        return NULL;
    }
    
    return ctx;
}

void nurl_ctx_destroy(NurlCtx *ctx) {
    if (!ctx) return;
    if (ctx->pool) {
        nurl_pool_destroy(ctx->pool);
    }
    free(ctx);
}
