#ifndef NURL_REQUEST_H
#define NURL_REQUEST_H

#include "nurl.h"

/**
 * Performs a generic, secure HTTP/1.1 request (GET, POST, etc.) including redirections,
 * and outputs the response.
 */
int nurl_request_generic(const char *method, const char *url, const CommonArgs *common);

#endif /* NURL_REQUEST_H */
