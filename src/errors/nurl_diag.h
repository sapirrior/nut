#ifndef NURL_DIAG_H
#define NURL_DIAG_H

/**
 * Emits a standardized error message to stderr in Unix style.
 * nurl: error: <msg>
 */
void nurl_diag_err(const char *fmt, ...);

/**
 * Emits a standardized hint message to stderr.
 *       hint: <msg>
 */
void nurl_diag_hint(const char *fmt, ...);

/**
 * Emits a standardized warning message to stderr.
 * nurl: warning: <msg>
 */
void nurl_diag_warn(const char *fmt, ...);

/**
 * Safely emits an out-of-memory error using low-level I/O.
 */
void nurl_diag_oom(void);

#endif /* NURL_DIAG_H */
