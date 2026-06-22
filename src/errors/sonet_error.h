#ifndef SONET_ERROR_H
#define SONET_ERROR_H

typedef enum {
    SONET_OK            = 0,
    SONET_ERR_OOM       = 1,   /* malloc/realloc returned NULL */
    SONET_ERR_NETWORK   = 2,   /* TCP connect/recv/send failed */
    SONET_ERR_URL       = 4,   /* Malformed or unsupported URL */
    SONET_ERR_TLS       = 5,   /* TLS handshake or cert error */
    SONET_ERR_IO        = 6,   /* File read/write failed */
    SONET_ERR_RESOLVE   = 7,   /* Hostname resolution failed */
    SONET_ERR_CONNECT   = 8,   /* TCP connection failed */
    SONET_ERR_PROXY     = 9,   /* Proxy connection/handshake failed */
    SONET_ERR_TLS_HANDSHAKE = 10, /* Specific TLS handshake failure */
    SONET_ERR_TIMEOUT   = 28,  /* curl exit 28: operation timed out */
    SONET_ERR_HTTP_4XX  = 22,  /* curl exit 22: HTTP 4xx response with -f/--fail */
    SONET_ERR_HTTP_5XX  = 23,  /* mapped to 22 on exit */
    SONET_ERR_ARG       = 3,   /* curl exit 3: Bad CLI argument */
    SONET_ERR_GENERIC   = 99,
} sonet_err_t;
#endif /* SONET_ERROR_H */
