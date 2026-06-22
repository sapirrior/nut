#ifndef SONET_ERROR_HANDLER_H
#define SONET_ERROR_HANDLER_H

#include "sonet_error.h"
#include "engine/sonet_engine_request.h"

/**
 * Centrally handles request errors by emitting smart, context-aware diagnostics.
 */
void sonet_handle_request_error(sonet_err_t err, const SonetRequest *req, const char *target_url);

#endif /* SONET_ERROR_HANDLER_H */
