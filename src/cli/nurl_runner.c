#include "nurl_runner.h"
#include "commands.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>

int nurl_runner_execute(const char *command, const char *url, const CommonArgs *common) {
    if (strcasecmp(command, "GET") == 0) {
        return nurl_cmd_get(url, common);
    } else if (strcasecmp(command, "POST") == 0) {
        return nurl_cmd_post(url, common);
    } else if (strcasecmp(command, "PUT") == 0) {
        return nurl_cmd_put(url, common);
    } else if (strcasecmp(command, "DELETE") == 0) {
        return nurl_cmd_delete(url, common);
    } else if (strcasecmp(command, "PATCH") == 0) {
        return nurl_cmd_patch(url, common);
    } else if (strcasecmp(command, "OPTIONS") == 0) {
        return nurl_cmd_options(url, common);
    } else if (strcasecmp(command, "HEAD") == 0) {
        return nurl_cmd_head(url, common);
    } else if (strcasecmp(command, "RESOLVE") == 0) {
        return nurl_cmd_resolve(url, common);
    } else if (strcasecmp(command, "PING") == 0) {
        return nurl_cmd_ping(url, common);
    } else if (strcasecmp(command, "DOWNLOAD") == 0) {
        return nurl_cmd_download(url, common);
    } else if (strcasecmp(command, "UPLOAD") == 0) {
        return nurl_cmd_upload(url, common);
    } else if (strcasecmp(command, "INSPECT") == 0) {
        return nurl_cmd_inspect(url, common);
    }

    fprintf(stderr, "Error: Command '%s' is not supported yet.\n", command);
    return NURL_ERR_BAD_ARGS;
}
