#ifndef SONET_CONFIG_H
#define SONET_CONFIG_H

#include "sonet.h"

/**
 * Loads the config file (from SONET_CONFIG env var or ~/.config/nurl/config.toml)
 * and merges the defaults and custom headers into the parsed CommonArgs.
 */
void sonet_config_load_and_merge(CommonArgs *args);

#endif /* SONET_CONFIG_H */
