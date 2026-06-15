#ifndef NURL_PLATFORM_H
#define NURL_PLATFORM_H

#include <stdio.h>

#if defined(_WIN32)
  #define NURL_PLATFORM_WINDOWS
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <io.h>
  #define nurl_socket_t   SOCKET
  #define NURL_INVALID_SOCK INVALID_SOCKET
  #define nurl_close_socket(s) closesocket(s)
  #define nurl_is_pipe()  (!_isatty(_fileno(stdin)))
#else
  #define NURL_PLATFORM_POSIX
  #include <unistd.h>
  #include <sys/socket.h>
  #define nurl_socket_t   int
  #define NURL_INVALID_SOCK (-1)
  #define nurl_close_socket(s) close(s)
  #define nurl_is_pipe()  (!isatty(STDIN_FILENO))
#endif

#endif /* NURL_PLATFORM_H */
