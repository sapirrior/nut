#ifndef SONET_DISPATCH_H
#define SONET_DISPATCH_H

#include "sonet.h"
#include "engine/sonet_ctx.h"

int sonet_dispatch(SonetCtx *ctx, const char *method, const char *url, const CommonArgs *args);

#endif /* SONET_DISPATCH_H */
