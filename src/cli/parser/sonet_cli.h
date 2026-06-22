#ifndef SONET_CLI_H
#define SONET_CLI_H

#include "sonet.h"

/**
 * Initializes CommonArgs with default values.
 */
void sonet_cli_init_args(CommonArgs *args);

/**
 * Parses the CLI arguments into CommonArgs, routing command name and target URL.
 * Returns 0 on success, non-zero on parsing error or help request.
 */
int sonet_cli_parse(int argc, char **argv, CommonArgs *args, char **url);

/**
 * Frees all dynamically allocated strings/arrays within CommonArgs.
 */
void sonet_cli_free_args(CommonArgs *args);

#endif /* SONET_CLI_H */
