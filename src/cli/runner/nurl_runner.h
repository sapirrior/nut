#ifndef NURL_RUNNER_H
#define NURL_RUNNER_H

#include "nurl.h"

/**
 * Routes and executes the specified command and options against the URL.
 * Returns 0 on success, or a non-zero exit code matching the original nurl exit codes.
 */
int nurl_runner_execute(const char *command, const char *url, const CommonArgs *common);

#endif /* NURL_RUNNER_H */
