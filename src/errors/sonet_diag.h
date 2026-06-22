#ifndef SONET_DIAG_H
#define SONET_DIAG_H

/**
 * Emits a standardized error message to stderr in Unix style.
 * sonet: error: <msg>
 */
void sonet_diag_err(const char *fmt, ...);

/**
 * Emits a standardized hint message to stderr.
 *       hint: <msg>
 */
void sonet_diag_hint(const char *fmt, ...);

/**
 * Emits a standardized warning message to stderr.
 * sonet: warning: <msg>
 */
void sonet_diag_warn(const char *fmt, ...);

/**
 * Safely emits an out-of-memory error using low-level I/O.
 */
void sonet_diag_oom(void);

#endif /* SONET_DIAG_H */
