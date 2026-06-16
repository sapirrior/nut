#ifndef NURL_PROGRESS_H
#define NURL_PROGRESS_H

#include "engine/request.h"
#include <sys/time.h>

typedef struct {
    unsigned long resume_offset;
    struct timeval start_time;
    struct timeval last_update;
    bool silent;
} NurlProgressCtx;

/**
 * The actual callback implementation that draws to the terminal.
 */
void nurl_progress_update(unsigned long downloaded, unsigned long total, bool finished, void *user_data);

#endif /* NURL_PROGRESS_H */
