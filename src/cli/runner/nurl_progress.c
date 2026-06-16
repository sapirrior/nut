#include "nurl_progress.h"
#include <stdio.h>
#include <sys/time.h>

void nurl_progress_update(unsigned long downloaded, unsigned long total, bool finished, void *user_data) {
    NurlProgressCtx *ctx = (NurlProgressCtx *)user_data;
    if (!ctx || ctx->silent) return;

    struct timeval now;
    gettimeofday(&now, NULL);

    double elapsed_sec = (now.tv_sec - ctx->start_time.tv_sec) + (now.tv_usec - ctx->start_time.tv_usec) / 1000000.0;
    double since_last_sec = (now.tv_sec - ctx->last_update.tv_sec) + (now.tv_usec - ctx->last_update.tv_usec) / 1000000.0;

    // Throttle updates to 5 times per second, unless it's the final update
    if (finished || since_last_sec >= 0.2 || (total > 0 && downloaded == total)) {
        ctx->last_update = now;
        double speed_mb = 0.0;
        if (elapsed_sec > 0.0) {
            speed_mb = ((double)(downloaded - ctx->resume_offset) / (1024.0 * 1024.0)) / elapsed_sec;
        }

        if (total > 0) {
            int percent = (int)(((double)downloaded / (double)total) * 100.0);
            double remaining_sec = 0.0;
            if (total > downloaded && speed_mb > 0.0) {
                remaining_sec = (double)(total - downloaded) / (speed_mb * 1024.0 * 1024.0);
            }
            fprintf(stderr, "\r  %.2f MB / %.2f MB  %d%%  %.2f MB/s  %.0fs left",
                (double)downloaded / (1024.0 * 1024.0),
                (double)total / (1024.0 * 1024.0),
                percent,
                speed_mb,
                remaining_sec);
        } else {
            fprintf(stderr, "\r  %.2f MB / Unknown  %.2f MB/s",
                (double)downloaded / (1024.0 * 1024.0),
                speed_mb);
        }
        fflush(stderr);
    }
}
