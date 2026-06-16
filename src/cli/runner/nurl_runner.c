#include "nurl_runner.h"
#include "commands.h"
#include "nurl_request.h"
#include "errors/nurl_diag.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>

int nurl_runner_execute(const char *command, const char *url, const CommonArgs *common) {
    if (strcasecmp(command, "GET") == 0 ||
        strcasecmp(command, "POST") == 0 ||
        strcasecmp(command, "PUT") == 0 ||
        strcasecmp(command, "DELETE") == 0 ||
        strcasecmp(command, "PATCH") == 0) {
        return nurl_request_generic(command, url, common);
    } else if (strcasecmp(command, "HEAD") == 0) {
        // For HEAD command, we always want to include headers in the output.
        CommonArgs args_copy = *common;
        args_copy.include = true;
        return nurl_request_generic("HEAD", url, &args_copy);
    } else if (strcasecmp(command, "OPTIONS") == 0) {
        return nurl_cmd_options(url, common);
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

    nurl_diag_err("command '%s' is not supported by nurl.", command);
    nurl_diag_hint("supported commands include: get, post, put, delete, head, patch, options, download, upload, inspect, ping, resolve.");
    return NURL_ERR_BAD_ARGS;
}
