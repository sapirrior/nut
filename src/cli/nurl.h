#ifndef NURL_H
#define NURL_H

#ifndef NURL_VERSION
#define NURL_VERSION "0.2.0"
#endif

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    // Auth
    char *user;
    char *bearer;
    char *token;
    bool digest;
    bool netrc;
    char *netrc_file;
    bool no_auth;

    // Body
    char *data;
    size_t data_len;
    bool json;
    char *data_binary;
    char *data_urlencode;
    char **form;
    size_t form_count;

    // TLS
    bool no_verify;
    char *cacert;
    char *cert;
    char *key;
    bool tls12;
    bool tls13;

    // Cookies
    char *cookie;
    char *cookie_jar;
    char *session;

    // Request Control
    char **query;
    size_t query_count;
    char *method;
    unsigned long timeout;
    unsigned long connect_timeout;
    bool location;
    unsigned int max_redirects;
    char *user_agent;
    char *referer;
    char **header;
    size_t header_count;
    bool compressed;
    bool http10;
    bool http11;
    bool http2;
    char *proxy;
    char *proxy_user;
    char *no_proxy;
    char *interface;
    char *limit_rate;
    char *max_filesize;
    bool keepalive;
    unsigned int retry;
    unsigned long retry_delay;
    char *retry_on;

    // Output
    char *output;
    bool output_name;
    bool include;
    bool verbose;
    bool silent;
    bool show_error;
    char *write_out;
    bool fail;
    bool fail_with_body;
    bool raw;
    bool head_only;
    bool body_only;
    char *dump_header;
    char *trace;
    char *format;

    // Ping specific
    unsigned int ping_count;
    unsigned long ping_interval;

    // Download & Upload specific
    bool resume;
    bool progress;
    char *upload_file;
    char *upload_name;
    char *upload_mime;
    char **upload_fields;
    size_t upload_fields_count;
} CommonArgs;

#include "engine/utils/nurl_error.h"

#endif /* NURL_H */
