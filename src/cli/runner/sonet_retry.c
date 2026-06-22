#include "sonet_retry.h"
#include "compat/sonet_compat.h"
#include "utils/sonet_utils.h"
#include "errors/sonet_diag.h"
#include <stdio.h>
#include <stdlib.h>

int execute_with_retry(SonetCtx *ctx, SonetRequest *req, const CommonArgs *common, sonet_http_response_t **out_res, char **out_effective_url, SonetOperationStats *stats) {
    unsigned int max_retries = common->retry;
    unsigned long delay_sec = common->retry_delay > 0 ? common->retry_delay : 1;
    int engine_err = SONET_OK;

    for (unsigned int attempt = 0; attempt <= max_retries; attempt++) {
        if (*out_res) { sonet_http_response_free(*out_res); *out_res = NULL; }
        if (*out_effective_url) { free(*out_effective_url); *out_effective_url = NULL; }

        engine_err = sonet_engine_execute_request(ctx, req, out_res, out_effective_url, stats);

        bool should_retry = false;
        if (engine_err == SONET_OK && *out_res) {
            should_retry = ((*out_res)->status_code >= 500);
            if (should_retry && attempt < max_retries && !common->silent) {
                sonet_diag_warn("HTTP %d. Retrying...", (*out_res)->status_code);
            }
        } else {
            should_retry = (engine_err == SONET_ERR_TIMEOUT || engine_err == SONET_ERR_NETWORK);
            if (should_retry && attempt < max_retries && !common->silent) {
                sonet_diag_warn("Request failed (error %d). Retrying...", engine_err);
            }
        }

        if (!should_retry) break;
        if (attempt < max_retries) sonet_sleep_ms(delay_sec * 1000);
    }
    return engine_err;
}
