#ifndef NURL_ERROR_H
#define NURL_ERROR_H

typedef enum {
    NURL_OK            = 0,
    NURL_ERR_OOM       = 1,   /* malloc/realloc returned NULL */
    NURL_ERR_NETWORK   = 2,   /* TCP connect/recv/send failed */
    NURL_ERR_URL       = 4,   /* Malformed or unsupported URL */
    NURL_ERR_TLS       = 5,   /* TLS handshake or cert error */
    NURL_ERR_IO        = 6,   /* File read/write failed */
    NURL_ERR_RESOLVE   = 7,   /* Hostname resolution failed */
    NURL_ERR_CONNECT   = 8,   /* TCP connection failed */
    NURL_ERR_PROXY     = 9,   /* Proxy connection/handshake failed */
    NURL_ERR_TLS_HANDSHAKE = 10, /* Specific TLS handshake failure */
    NURL_ERR_TIMEOUT   = 28,  /* curl exit 28: operation timed out */
    NURL_ERR_HTTP_4XX  = 22,  /* curl exit 22: HTTP 4xx response with -f/--fail */
    NURL_ERR_HTTP_5XX  = 23,  /* mapped to 22 on exit */
    NURL_ERR_ARG       = 3,   /* curl exit 3: Bad CLI argument */
    NURL_ERR_GENERIC   = 99,
} nurl_err_t;

/* nurl_err is now a no-op side-effect wise, returning the code only */
static inline nurl_err_t nurl_err(nurl_err_t code, ...) { return code; }

/* nurl_hint is now a no-op; use nurl_diag_hint instead */
static inline void nurl_hint(const char *fmt, ...) { (void)fmt; }

#endif /* NURL_ERROR_H */
