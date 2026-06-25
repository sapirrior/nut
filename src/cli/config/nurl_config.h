#ifndef NURL_CONFIG_H
#define NURL_CONFIG_H

#include "nurl.h"

/**
 * Loads the config file (from NURL_CONFIG env var or ~/.config/nurl/config.toml)
 * and merges the defaults and custom headers into the parsed CommonArgs.
 */
void nurl_config_load_and_merge(CommonArgs *args);

#endif /* NURL_CONFIG_H */
