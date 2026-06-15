#ifndef NURL_CLI_H
#define NURL_CLI_H

#include "nurl.h"

/**
 * Initializes CommonArgs with default values.
 */
void nurl_cli_init_args(CommonArgs *args);

/**
 * Parses the CLI arguments into CommonArgs, routing command name and target URL.
 * Returns 0 on success, non-zero on parsing error or help request.
 */
int nurl_cli_parse(int argc, char **argv, CommonArgs *args, char **command, char **url);

/**
 * Frees all dynamically allocated strings/arrays within CommonArgs.
 */
void nurl_cli_free_args(CommonArgs *args);

#endif /* NURL_CLI_H */
