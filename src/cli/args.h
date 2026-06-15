#ifndef ARGS_H
#define ARGS_H

#include <stdbool.h>
#include <stddef.h>

/* Fields relevant to every HTTP command */
typedef struct {
    char     *url;           /* set after parse */
    char     *method;

    /* Auth */
    char     *user;
    char     *bearer;
    char     *token;
    bool      no_auth;

    /* Headers / body */
    char    **header;
    size_t    header_count;
    char     *data;
    size_t    data_len;
    bool      data_is_stdin;
    bool      json;

    /* TLS */
    bool      no_verify;
    char     *cacert;
    char     *cert;
    char     *key;
    bool      tls12;
    bool      tls13;

    /* Proxy */
    char     *proxy;
    char     *proxy_user;
    char     *no_proxy;

    /* Transfer control */
    unsigned long timeout;
    bool          location;
    unsigned int  retry;
    unsigned int  retry_delay;
    bool          fail;
    bool          compressed;

    /* Output control */
    char     *output;
    bool      include;
    bool      verbose;
    bool      silent;
    bool      raw;
    char     *write_out;
    char     *user_agent;
    char     *referer;

    /* Cookies */
    char     *cookie;
    char     *cookie_jar;
    char     *session;
} BaseArgs;

/* Download-only */
typedef struct {
    bool          resume;
    bool          progress;
} DownloadArgs;

/* Upload-only */
typedef struct {
    char         *file;
    char         *name;
    char         *mime;
    char        **fields;
    size_t        field_count;
} UploadArgs;

/* Ping-only */
typedef struct {
    unsigned int  count;
    unsigned long interval_ms;
} PingArgs;

void nurl_args_free_base(BaseArgs *args);
void nurl_args_free_upload(UploadArgs *args);

#endif /* ARGS_H */
