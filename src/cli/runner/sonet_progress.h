#ifndef SONET_PROGRESS_H
#define SONET_PROGRESS_H

#include "engine/sonet_engine_request.h"
#include <stdbool.h>

typedef struct {
    unsigned long resume_offset;
    double start_time;
    double last_update;
    bool silent;
} SonetProgressCtx;

/**
 * The actual callback implementation that draws to the terminal.
 */
void sonet_progress_update(unsigned long downloaded, unsigned long total, bool finished, void *user_data);

#endif /* SONET_PROGRESS_H */
