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

#endif /* NURL_DIAG_H */
