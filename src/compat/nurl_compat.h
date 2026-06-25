#ifndef NURL_COMPAT_H
#define NURL_COMPAT_H

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <windows.h>
#  include <io.h>
#  include <sys/stat.h>
#  define nurl_sleep_ms(ms) Sleep(ms)
#  define nurl_strcasecmp(a,b)    _stricmp(a,b)
#  define nurl_strncasecmp(a,b,n) _strnicmp(a,b,n)
#  define nurl_stat   _stat
#  define nurl_fstat  _fstat
#  define NURL_S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)

/* Simplified strcasestr for Windows */
static inline char *nurl_strcasestr(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    for (; *haystack; haystack++) {
        if (tolower((unsigned char)*haystack) == tolower((unsigned char)*needle)) {
            const char *h, *n;
            for (h = haystack, n = needle; *h && *n; h++, n++) {
                if (tolower((unsigned char)*h) != tolower((unsigned char)*n)) break;
            }
            if (!*n) return (char *)haystack;
        }
    }
    return NULL;
}

#else
#  include <unistd.h>
#  include <sys/time.h>
#  include <sys/socket.h>
#  include <netdb.h>
#  include <arpa/inet.h>
#  include <sys/stat.h>
#  include <strings.h>
#define nurl_sleep_ms(ms) usleep((ms) * 1000)
#define nurl_strcasecmp(a,b)    strcasecmp(a,b)
#define nurl_strncasecmp(a,b,n) strncasecmp(a,b,n)
#  define nurl_strcasestr(a,b) strcasestr(a,b)
#  define nurl_stat   stat
#  define nurl_fstat  fstat
#  define NURL_S_ISREG(m) S_ISREG(m)
#endif

#endif /* NURL_COMPAT_H */
