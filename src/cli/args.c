#include "args.h"
#include <stdlib.h>

void nurl_args_free_base(BaseArgs *args) {
    if (!args) return;
    free(args->url);
    free(args->method);
    free(args->user);
    free(args->bearer);
    free(args->token);

    if (args->header) {
        for (size_t i = 0; i < args->header_count; i++) {
            free(args->header[i]);
        }
        free(args->header);
    }

    free(args->data);
    free(args->cacert);
    free(args->cert);
    free(args->key);
    free(args->proxy);
    free(args->proxy_user);
    free(args->no_proxy);
    free(args->output);
    free(args->write_out);
    free(args->user_agent);
    free(args->referer);
    free(args->cookie);
    free(args->cookie_jar);
    free(args->session);
}

void nurl_args_free_upload(UploadArgs *args) {
    if (!args) return;
    free(args->file);
    free(args->name);
    free(args->mime);

    if (args->fields) {
        for (size_t i = 0; i < args->field_count; i++) {
            free(args->fields[i]);
        }
        free(args->fields);
    }
}
