#ifndef SONET_STREAM_H
#define SONET_STREAM_H

#include <stddef.h>
#include <stdbool.h>
#include "sonet_tls.h"

#define SONET_STREAM_BUFFER_SIZE 8192

typedef struct {
    unsigned char data[SONET_STREAM_BUFFER_SIZE];
    size_t        pos;
    size_t        len;
} SonetStreamBuffer;

typedef struct SonetStream {
    int              fd;
    sonet_tls_t      *tls;
    SonetStreamBuffer read_buf;
    unsigned long    limit_rate;    /* bytes per second, 0=unlimited */
    double           last_throttle_time;
    unsigned long    bytes_this_sec;
} SonetStream;

/**
 * Creates a new stream wrapping a socket and optionally a TLS context.
 * The stream does NOT take ownership of the fd or tls (it won't close them on free).
 */
SonetStream *sonet_stream_new(int fd, sonet_tls_t *tls);

/**
 * Sets the bandwidth limit rate in bytes per second.
 */
void        sonet_stream_set_limit_rate(SonetStream *s, unsigned long rate);

/**
 * Frees the stream structure.
 */
void        sonet_stream_free(SonetStream *s);

/**
 * Reads up to 'len' bytes into 'buf'. Returns bytes read, or <= 0 on error/EOF.
 * Uses the internal buffer to minimize syscalls/SSL_read calls.
 */
int         sonet_stream_read(SonetStream *s, void *buf, size_t len);

/**
 * Reads a single line (up to \n) into 'buf' of max size 'max_len'.
 * The resulting string is null-terminated and includes the newline if space permits.
 * Returns bytes read (including \n), or <= 0 on error/EOF.
 */
int         sonet_stream_read_line(SonetStream *s, char *buf, size_t max_len);

/**
 * Reads exactly 'len' bytes into 'buf'.
 * Returns 'len' on success, or <= 0 on error/EOF.
 */
int         sonet_stream_read_exact(SonetStream *s, void *buf, size_t len);

/**
 * Writes 'len' bytes from 'buf' to the stream.
 * Returns bytes written, or <= 0 on error.
 */
int         sonet_stream_write(SonetStream *s, const void *buf, size_t len);

/**
 * Checks if the stream has any buffered data remaining.
 */
bool        sonet_stream_has_buffered(const SonetStream *s);

#endif /* SONET_STREAM_H */
