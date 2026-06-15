#ifndef NURL_UTILS_H
#define NURL_UTILS_H

#include <stdbool.h>
#include <stddef.h>

/**
 * Parses a URL into its constituent components.
 * Returns 0 on success, non-zero on failure.
 * The caller must free *scheme, *host, and *path if they are non-NULL.
 */
int nurl_utils_parse_url(const char *url, char **scheme, char **host, int *port, char **path);

/**
 * Returns a redacted version of header values if the header is sensitive.
 * Returns "[hidden]" for Authorization and Cookie headers, or the original value otherwise.
 */
const char *nurl_utils_redact_header(const char *key, const char *value);

/**
 * Trim leading and trailing whitespace from a string.
 * Returns a pointer to the trimmed substring within the original buffer (modifies the buffer).
 */
char *nurl_utils_trim(char *str);

/**
 * Encodes a buffer of bytes to a dynamically allocated base64 string.
 * The caller is responsible for freeing the returned pointer.
 */
char *nurl_utils_base64_encode(const unsigned char *src, size_t len);

/**
 * Appends a formatted header string to a dynamically allocated buffer, growing it if needed.
 * Returns true on success, false on OOM.
 */
bool nurl_utils_append_hdr_str(char **buf, size_t *len, size_t *cap, const char *fmt, const char *val);

/**
 * Checks if a specific header key exists in the array of headers.
 */
bool nurl_utils_has_header(char **headers, size_t count, const char *key);

/**
 * Returns the current epoch time in seconds (high resolution).
 */
double nurl_utils_get_time_sec(void);

#endif /* NURL_UTILS_H */
