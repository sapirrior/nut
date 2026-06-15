#ifndef NURL_ERROR_H
#define NURL_ERROR_H

typedef enum {
    NURL_OK            = 0,
    NURL_ERR_OOM       = 1,   /* malloc/realloc returned NULL */
    NURL_ERR_NETWORK   = 2,   /* TCP connect/recv/send failed */
    NURL_ERR_URL       = 4,   /* Malformed or unsupported URL */
    NURL_ERR_TLS       = 5,   /* TLS handshake or cert error */
    NURL_ERR_IO        = 6,   /* File read/write failed */
    NURL_ERR_TIMEOUT   = 28,  /* curl compat: operation timed out */
    NURL_ERR_HTTP_4XX  = 22,  /* curl compat: 4xx response, -f/--fail */
    NURL_ERR_HTTP_5XX  = 43,  /* curl compat: 5xx response, -f/--fail */
    NURL_ERR_ARG       = 3,   /* Bad CLI argument */
    NURL_ERR_GENERIC   = 99,
} nurl_err_t;

/* Backward compatibility aliases */
#define NURL_ERR_INVALID_URL NURL_ERR_URL
#define NURL_ERR_BAD_ARGS    NURL_ERR_ARG
#define NURL_ERR_WRITE       NURL_ERR_IO
#define NURL_ERR_STATUS_4XX  NURL_ERR_HTTP_4XX
#define NURL_ERR_STATUS_5XX  NURL_ERR_HTTP_5XX

/* Emit a formatted error to stderr and return the code */
nurl_err_t nurl_err(nurl_err_t code, const char *fmt, ...);

/* Emit a suggestion hint (no exit) */
void nurl_hint(const char *fmt, ...);

#endif /* NURL_ERROR_H */
