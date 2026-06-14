#ifndef COMMANDS_H
#define COMMANDS_H

#include "nurl.h"

int nurl_cmd_get(const char *url, const CommonArgs *common);
int nurl_cmd_post(const char *url, const CommonArgs *common);
int nurl_cmd_put(const char *url, const CommonArgs *common);
int nurl_cmd_delete(const char *url, const CommonArgs *common);
int nurl_cmd_head(const char *url, const CommonArgs *common);
int nurl_cmd_patch(const char *url, const CommonArgs *common);
int nurl_cmd_options(const char *url, const CommonArgs *common);
int nurl_cmd_resolve(const char *host, const CommonArgs *common);
int nurl_cmd_ping(const char *url, const CommonArgs *common);
int nurl_cmd_inspect(const char *url, const CommonArgs *common);
int nurl_cmd_download(const char *url, const CommonArgs *common);
int nurl_cmd_upload(const char *url, const CommonArgs *common);

#endif /* COMMANDS_H */
