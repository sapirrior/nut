# nurl — Modernization & Architecture Plan

> **Goal**: Transform nurl into a curl-equivalent engine with a drastically better CLI surface, first-class piping, tolerant error handling, and a codebase that scales without friction.

---

## 0. Problem Diagnosis (Current State)

Before prescribing changes, here is what is wrong right now:

| Area | Problem |
|---|---|
| **Header assembly** | Auth/User-Agent header building is copy-pasted verbatim in `download.c`, `upload.c`, `options.c`, `ping.c`, and `inspect.c`. Any bug or new header type must be fixed in 5+ places. |
| **Connection lifecycle** | Every command opens its own raw socket → TLS handshake → sends → closes. There is no connection reuse, no keep-alive, no pooling. |
| **Error codes** | `NURL_ERR_*` constants exist in `nurl.h` but error messages use raw `fprintf(stderr, ...)` with inconsistent formats (`nurl: (2)`, `nurl: (5)`, etc.). No central error sink. |
| **User mistakes** | A wrong subcommand, a missing URL, or a bad flag silently falls through to a generic `print_help()`. The user gets a wall of text instead of "did you mean `get`?". |
| **upload.c** | Reads the entire file into memory (`malloc(fsize)`). A 2GB upload will OOM on constrained devices. |
| **Makefile** | Single flat compile of 24 `.c` files — no incremental build, no object file caching. Rebuild from scratch every time. |
| **Platform guards** | `#ifdef _WIN32` scattered across `main.c` and `nurl_net.c`. No abstraction layer. |
| **CommonArgs** | A single flat struct carries everything: REST flags, ping-specific fields, upload-specific fields. Every command sees every flag. |
| **No `nurl_err_t` type** | Returning raw `int` means callers must know magic numbers. There is no type safety on error codes. |
| **No input validation** | If `--timeout abc` is passed, `strtoul` silently returns 0. No `--retry -1` guard either. |

---

## 1. Target Architecture

```
nurl/
├── src/
│   ├── main.c                      # Thin: init, parse, dispatch, cleanup
│   │
│   ├── cli/
│   │   ├── args.h / args.c         # CommonArgs definition + memory management
│   │   ├── parser.h / parser.c     # getopt_long wrapper, validation, suggestion engine
│   │   ├── output.h / output.c     # All printf/fprintf output formatting (single choke-point)
│   │   ├── runner.h / runner.c     # Command dispatch table
│   │   └── commands/
│   │       ├── cmd_http.c          # GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS (all thin)
│   │       ├── cmd_download.c      # Streaming download (chunked, no full-buffer)
│   │       ├── cmd_upload.c        # Streaming multipart upload
│   │       ├── cmd_ping.c          # Latency probe
│   │       ├── cmd_resolve.c       # DNS resolution
│   │       └── cmd_inspect.c       # Dry-run inspector
│   │
│   ├── engine/
│   │   ├── nurl_engine.h / .c      # Public engine API (request context in, response out)
│   │   ├── request.h / .c          # NurlRequest builder (replaces scattered header assembly)
│   │   ├── response.h / .c         # NurlResponse struct + parser
│   │   │
│   │   ├── net/
│   │   │   ├── nurl_net.h / .c     # Platform-abstracted TCP (POSIX + Winsock behind macros)
│   │   │   ├── nurl_conn.h / .c    # Connection lifecycle + keep-alive pool
│   │   │   └── nurl_proxy.h / .c   # Proxy CONNECT tunnel (extracted from nurl_net)
│   │   │
│   │   ├── tls/
│   │   │   └── nurl_tls.h / .c     # (unchanged interface, internals cleaned)
│   │   │
│   │   ├── http/
│   │   │   ├── nurl_http.h / .c    # HTTP/1.1 request serializer + response parser
│   │   │   ├── nurl_decompress.h/.c# gzip/deflate/br decompression
│   │   │   └── nurl_redirect.h/.c  # 3xx redirect follower
│   │   │
│   │   └── utils/
│   │       ├── nurl_error.h / .c   # Central error type, codes, message table, fprintf sink
│   │       ├── nurl_headers.h / .c # Header list builder (single impl, used everywhere)
│   │       ├── nurl_url.h / .c     # URL parser + encoder (extracted from nurl_utils)
│   │       ├── nurl_cookies.h / .c # Cookie jar (unchanged)
│   │       ├── nurl_config.h / .c  # Config file loader (unchanged)
│   │       ├── nurl_io.h / .c      # Stdin reader, file streaming helpers
│   │       └── nurl_time.h / .c    # High-res timer (platform-abstracted)
│   │
│   └── compat/
│       ├── nurl_platform.h         # All platform detection macros (one place)
│       └── nurl_compat.c           # strndup, getline, etc. for MSVC/older libc
│
├── tests/
│   ├── unit/                       # Pure C unit tests, no network
│   └── integration/                # Shell scripts hitting httpbin or local mock
│
├── docs/
│   ├── ERRORS.md                   # Every error code, meaning, and fix hint
│   └── EXTENDING.md                # How to add a new command
│
├── Makefile                        # Object-file based, incremental
└── README.md
```

**Key structural rules:**
- Every `src/engine/` module is callable without the CLI layer (testable standalone).
- Every `src/cli/commands/` file is a thin dispatcher — 30–60 lines max. Business logic lives in `engine/`.
- No `fprintf(stderr, ...)` outside of `cli/output.c` and `engine/utils/nurl_error.c`.

---

## 2. The Error System

This is the highest-leverage change. Every part of the codebase touches errors.

### 2.1 Central Error Type

```c
/* engine/utils/nurl_error.h */

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

/* Emit a formatted error to stderr and return the code */
nurl_err_t nurl_err(nurl_err_t code, const char *fmt, ...);

/* Emit a suggestion hint (no exit) */
void nurl_hint(const char *fmt, ...);
```

Every function in `engine/` returns `nurl_err_t`. No raw `int`. No scattered `fprintf`.

### 2.2 Error Message Format

Consistent, actionable format across all errors:

```
nurl: error [2]: could not connect to 'api.example.com:443'
      reason: connection refused (ECONNREFUSED)
      hint: check that the host is reachable and the port is correct
```

The `nurl_hint()` function is for non-fatal suggestions printed to stderr, not prefixed with "error". This is the user-mistake recovery path — it fires before or instead of returning an error code wherever the intent can be inferred.

---

## 3. User Mistake Tolerance

The single design principle: **if a user makes a recoverable mistake, fix it or say exactly what to do. Never show the full help screen for a single bad argument.**

### 3.1 Subcommand Suggestion Engine

When an unknown subcommand is entered, compute Levenshtein distance against the known command list and suggest the closest match:

```
$ nurl gett https://example.com
nurl: unknown command 'gett'
      did you mean: get
```

Implementation: `cli/parser.c` — a small static array of known commands + a simple edit-distance function (under 30 lines). If distance > 3, print the short command list. Never dump the full help.

### 3.2 Flag Validation at Parse Time

All flag values are validated immediately after `getopt_long`, before any network call:

| Flag | Validation |
|---|---|
| `--timeout <n>` | Must be a positive integer. If `abc` → `nurl: invalid timeout 'abc', expected a number in seconds` |
| `--retry <n>` | Must be 0–99. If negative → error + hint |
| `-H "BadHeader"` | Must contain `:`. If not → `nurl: header 'BadHeader' is missing ':', use 'Key: Value' format` |
| `-d -` + `--data value` | Conflict: `nurl: cannot use both '-d -' (stdin) and explicit --data` |
| `--tls1.2` + `--tls1.3` | Conflict: only one TLS version allowed |
| Missing URL | `nurl: no URL provided. Usage: nurl <command> <URL> [options]` |
| `https` vs `http` typo | If scheme is unrecognized → `nurl: unsupported scheme 'htps', did you mean 'https'?` |

All validation lives in `cli/parser.c:nurl_args_validate()`, called once after parse, before any command dispatch.

### 3.3 URL Normalization

Before the URL reaches the engine, `nurl_url_normalize()` in `engine/utils/nurl_url.c`:

- Strips trailing whitespace (pasted URLs often have trailing newlines)
- Prepends `https://` if no scheme is detected (with a printed notice, not silent)
- Lowercases scheme and host components
- Decodes double-escaped percent sequences

```
$ nurl get example.com/path
nurl: no scheme in URL, assuming https://
```

### 3.4 Exit Code Contract

Exit codes are guaranteed consistent — scripts relying on nurl can trust them:

| Code | Meaning |
|---|---|
| 0 | Success |
| 1 | Internal/OOM |
| 2 | Network error |
| 3 | Bad argument / missing URL |
| 4 | Malformed URL |
| 5 | TLS error |
| 6 | File I/O error |
| 22 | HTTP 4xx (only with `--fail`) |
| 28 | Timeout |
| 43 | HTTP 5xx (only with `--fail`) |

These are intentionally curl-compatible for exit code 22, 28, and 43.

---

## 4. Header Assembly Consolidation

Currently duplicated in 5+ files. Replace with a single `NurlHeaderList` API:

```c
/* engine/utils/nurl_headers.h */

typedef struct {
    char **entries;   /* "Key: Value\r\n" strings */
    size_t count;
    size_t capacity;
} NurlHeaderList;

NurlHeaderList *nurl_headers_new(void);
nurl_err_t      nurl_headers_add(NurlHeaderList *h, const char *key, const char *value);
nurl_err_t      nurl_headers_add_raw(NurlHeaderList *h, const char *line);  /* "Key: Value" */
bool            nurl_headers_has(const NurlHeaderList *h, const char *key); /* case-insensitive */
nurl_err_t      nurl_headers_apply_auth(NurlHeaderList *h, const CommonArgs *a);
nurl_err_t      nurl_headers_apply_common(NurlHeaderList *h, const CommonArgs *a);
char           *nurl_headers_serialize(const NurlHeaderList *h);            /* heap alloc, caller frees */
void            nurl_headers_free(NurlHeaderList *h);
```

`nurl_headers_apply_auth()` and `nurl_headers_apply_common()` are the single canonical implementations of auth header injection (Basic, Bearer, Token), User-Agent, Referer, Cookie, etc. All commands call these — no more copy-paste.

---

## 5. Request Context Object

Replace scattered per-command argument threading with a clean `NurlRequest` context:

```c
/* engine/request.h */

typedef struct {
    const char      *method;       /* "GET", "POST", etc. */
    const char      *url;
    NurlHeaderList  *headers;
    const uint8_t   *body;
    size_t           body_len;
    bool             body_is_stream; /* read from stdin lazily */

    /* Transfer config */
    unsigned int     timeout_sec;
    bool             follow_redirect;
    unsigned int     max_redirects;   /* default: 10 */
    unsigned int     retry_count;
    unsigned int     retry_delay_sec;
    bool             fail_on_error;

    /* TLS config */
    bool             tls_verify;
    const char      *cacert;
    const char      *cert;
    const char      *key;
    int              tls_version;     /* 0=auto, 12=TLSv1.2, 13=TLSv1.3 */

    /* Proxy */
    const char      *proxy;
    const char      *proxy_user;
    const char      *no_proxy;

    /* Output */
    FILE            *out;             /* stdout or file handle */
    bool             include_headers; /* print response headers */
    bool             verbose;
    bool             silent;
    bool             raw_output;
    bool             decompress;

    /* Download-specific */
    bool             resume;
    unsigned long    resume_offset;

    /* Upload-specific */
    const char      *upload_file;
    /* ... */
} NurlRequest;

NurlRequest *nurl_request_new(void);
void         nurl_request_from_args(NurlRequest *req, const char *method,
                                    const char *url, const CommonArgs *a);
void         nurl_request_free(NurlRequest *req);
```

`nurl_engine_execute(NurlRequest *req, NurlResponse **resp_out)` is the single entry point into the engine from all commands. No command ever calls `nurl_net_*`, `nurl_tls_*`, or `nurl_http_*` directly.

---

## 6. Connection Pooling (Incremental)

The current design opens a new TCP+TLS connection per request. This is fine for one-shot use but expensive for `--retry`, `ping --count`, and any future scripted usage.

### Phase 1 (immediate): Keep-Alive on retry

When `--retry N` is active, hold the connection open between retry attempts on 5xx/transient errors instead of teardown+reconnect.

### Phase 2 (future): Connection Cache

Inspired by libcurl's `conncache`, implement a simple per-host LRU connection cache in `engine/net/nurl_conn.c`:

```c
typedef struct NurlConn NurlConn;

NurlConn   *nurl_conn_acquire(const char *host, int port, const ConnConfig *cfg);
void        nurl_conn_release(NurlConn *conn); /* returns to pool, not closed */
void        nurl_conn_close(NurlConn *conn);   /* actually closes */
void        nurl_conn_pool_flush(void);         /* close all idle */
```

Pool limit: 8 connections per host, 32 total. Connections idle >30s are evicted. This is enough for scripted multi-request use without the complexity of a full multi-handle.

---

## 7. Streaming I/O

### 7.1 Download: No Full-Buffer

Current `download.c` accumulates the entire response into `nurl_http_response_t.body`. For large files this is fatal on embedded/Termux.

New design: `nurl_engine_execute()` accepts an output `FILE *out`. The response body is written in chunks of 64KB directly to `out` as bytes arrive from the socket — no intermediate `body` buffer at all.

```c
/* engine/http/nurl_http.h */

typedef nurl_err_t (*nurl_body_cb)(const uint8_t *chunk, size_t len, void *userdata);

nurl_err_t nurl_http_stream_response(NurlConn *conn, nurl_body_cb cb, void *userdata);
```

`cmd_download.c` passes a callback that writes to the output file (or stdout).

### 7.2 Upload: Chunked Reading

Current `upload.c` does `malloc(fsize)` to read the entire file before building the multipart body. New design:

- Build multipart preamble (headers, boundary) as a small heap buffer.
- Stream file bytes directly from disk in 64KB chunks into the TLS send buffer.
- Build multipart epilogue (final boundary) as another small buffer.

No full-file buffer. Works on files of any size.

### 7.3 Stdin Pipe — Lazy Read

Currently stdin is read fully into `args.data` in `main.c` before any network call. For piped binary data this doubles memory. New: pass `body_is_stream = true` on the request and read from stdin directly in the send loop.

---

## 8. Platform Abstraction

Move all platform guards to `src/compat/nurl_platform.h`:

```c
#if defined(_WIN32)
  #define NURL_PLATFORM_WINDOWS
  #include <winsock2.h>
  #include <ws2tcpip.h>
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
```

No `#ifdef _WIN32` appears anywhere else in the codebase. `nurl_net.c` uses the macros above. This makes porting to a new platform a one-file change.

---

## 9. Makefile Overhaul

Replace the single-compilation Makefile with a proper object-file build:

```makefile
CC      = gcc
VERSION ?= 0.1.3
CFLAGS  = -std=c11 -Wall -Wextra -Os -ffunction-sections -fdata-sections \
          -fno-ident -D_GNU_SOURCE -DNURL_VERSION=\"$(VERSION)\" \
          -Isrc -Isrc/cli -Isrc/engine -Isrc/engine/utils \
          -Isrc/engine/net -Isrc/engine/tls -Isrc/engine/http -Isrc/compat
LDFLAGS = -Wl,-Bstatic -lssl -lcrypto -Wl,-Bdynamic -lpthread -ldl -lz \
          -Wl,--gc-sections -s

SRCS    := $(shell find src -name '*.c')
OBJS    := $(SRCS:src/%.c=build/%.o)
TARGET  = nurl

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build $(TARGET)

.PHONY: all clean
```

Incremental builds: only changed files recompile. `build/` mirrors `src/` directory tree. Add `test` and `install` targets later.

---

## 10. CommonArgs Cleanup

The current `CommonArgs` struct is a 40-field flat blob that every command sees, including irrelevant fields (e.g. `ping_count` in a `get` call). Split it:

```c
/* cli/args.h */

/* Fields relevant to every HTTP command */
typedef struct {
    char     *url;           /* set after parse */
    char     *method;

    /* Auth */
    char     *user;
    char     *bearer;
    char     *token;
    bool      no_auth;

    /* Headers / body */
    char    **headers;
    size_t    header_count;
    char     *data;
    size_t    data_len;
    bool      data_is_stdin;
    bool      json;

    /* TLS */
    bool      no_verify;
    char     *cacert;
    char     *cert;
    char     *key;
    bool      tls12;
    bool      tls13;

    /* Proxy */
    char     *proxy;
    char     *proxy_user;
    char     *no_proxy;

    /* Transfer control */
    unsigned long timeout;
    bool          location;
    unsigned int  retry;
    unsigned int  retry_delay;
    bool          fail;
    bool          compressed;

    /* Output control */
    char     *output;
    bool      include;
    bool      verbose;
    bool      silent;
    bool      raw;
    char     *write_out;
    char     *user_agent;
    char     *referer;

    /* Cookies */
    char     *cookie;
    char     *cookie_jar;
    char     *session;
} BaseArgs;

/* Download-only */
typedef struct {
    bool          resume;
    bool          progress;
} DownloadArgs;

/* Upload-only */
typedef struct {
    char         *file;
    char         *name;
    char         *mime;
    char        **fields;
    size_t        field_count;
} UploadArgs;

/* Ping-only */
typedef struct {
    unsigned int  count;
    unsigned long interval_ms;
} PingArgs;
```

`nurl_request_from_args()` accepts `BaseArgs *` and merges the relevant sub-struct depending on the command. Commands only receive what they need.

---

## 11. Output Layer

All output goes through `cli/output.c`. No `printf` or `fprintf` in commands or engine.

```c
/* cli/output.h */

typedef struct {
    bool verbose;
    bool silent;
    bool include_headers;
    bool raw;
    bool is_tty;           /* set at startup: isatty(STDOUT_FILENO) */
} NurlOutputCtx;

void nurl_out_response_headers(const NurlOutputCtx *ctx, const NurlResponse *res);
void nurl_out_response_body(const NurlOutputCtx *ctx, const uint8_t *body, size_t len);
void nurl_out_verbose_request(const NurlOutputCtx *ctx, const NurlRequest *req);
void nurl_out_verbose_response(const NurlOutputCtx *ctx, const NurlResponse *res);
void nurl_out_write_out(const NurlOutputCtx *ctx, const char *template, const NurlResponse *res);
void nurl_out_progress(const NurlOutputCtx *ctx, size_t received, size_t total);
```

`is_tty` controls whether progress bars and color hints fire. When stdout is a pipe (`nurl get url | jq`), progress goes to stderr only, body bytes go to stdout clean. This is how curl handles it — nurl must too.

---

## 12. Extending nurl: Adding a Command

With the new structure, adding a command is:

1. Add `int nurl_cmd_foo(const BaseArgs *a, const FooArgs *fa)` in `src/cli/commands/cmd_foo.c`.
2. Register it in the dispatch table in `cli/runner.c` — one line.
3. Add its flags to `cli/parser.c` — one `getopt_long` entry + one validation block.
4. Document in `docs/EXTENDING.md`.

No changes to engine, no changes to other commands, no changes to `main.c`.

---

## 13. Testing Plan

### Unit Tests (`tests/unit/`)

| Test file | Covers |
|---|---|
| `test_url.c` | URL parser: IPv6, query strings, percent-encoding, no-scheme normalization |
| `test_headers.c` | `NurlHeaderList`: add, has (case-insensitive), serialize, auth injection |
| `test_args.c` | Parser: bad flags, conflicting flags, missing URL, value validation |
| `test_error.c` | Error code format, `nurl_err()` output, all codes defined |
| `test_mime.c` | MIME type detection from extension |

Tests are plain C with `assert()`. Run with `make test`. No external framework required.

### Integration Tests (`tests/integration/`)

Shell scripts using `httpbin.org` or a local netcat mock:

- GET / POST / PUT / DELETE round-trip
- `--retry` on 503 response
- `--resume` on partial download (truncate file, re-download)
- Piped stdin → POST body
- `-o -` stdout piping with binary body
- TLS verification failure with self-signed cert
- Proxy CONNECT tunnel

---

## 14. Implementation Phases

### Phase 1 — Foundation (do first, unblocks everything)
- `src/compat/nurl_platform.h` — all platform macros in one file
- `engine/utils/nurl_error.h/.c` — central error type + `nurl_err()` function
- `engine/utils/nurl_headers.h/.c` — `NurlHeaderList` with auth injection
- `cli/args.h` — split `CommonArgs` into `BaseArgs` + sub-structs
- Makefile → incremental object-file build

### Phase 2 — CLI Hardening
- `cli/parser.c` — add validation pass + subcommand suggestion engine
- `cli/output.c` — centralize all output, add `is_tty` detection
- All commands → remove direct `fprintf`, route through `nurl_out_*` and `nurl_err()`

### Phase 3 — Engine Refactor
- `engine/request.h/.c` — `NurlRequest` + `nurl_request_from_args()`
- All commands → call `nurl_engine_execute()` instead of raw `nurl_net_*`
- `engine/net/nurl_proxy.c` — extract proxy tunnel logic from `nurl_net.c`

### Phase 4 — Streaming
- `download.c` → chunked body streaming, no full buffer
- `upload.c` → streaming multipart, no full-file malloc
- Stdin → lazy body read in send loop

### Phase 5 — Connection Pooling
- `engine/net/nurl_conn.c` — per-host LRU pool
- `--retry` → hold connection alive between attempts

### Phase 6 — Tests & Docs
- `tests/unit/` — all test files listed in §13
- `docs/ERRORS.md` — every error code, meaning, hint
- `docs/EXTENDING.md` — step-by-step new command guide

---

## 15. What Does Not Change

- The CLI surface (subcommands, flag names). Existing nurl usage stays 100% compatible.
- OpenSSL as the TLS backend. No dependency additions.
- `-Os` size optimization. Binary must stay compact.
- Static linking of `libssl`/`libcrypto`. No runtime dependency.
- Single-binary output. No shared libs, no plugin loading.

---

## Summary

The changes above address every diagnosed problem in a specific, ordered way:

- **Duplication** → `NurlHeaderList` + `nurl_headers_apply_auth/common()`
- **User mistakes** → validation pass, suggestion engine, URL normalization
- **Memory on large transfers** → streaming body callbacks, no full-file buffer
- **Platform leakage** → `nurl_platform.h` single source of truth
- **Error chaos** → `nurl_err_t` type, central sink, consistent format
- **Slow builds** → incremental object-file Makefile
- **Extensibility** → dispatch table + `BaseArgs`, one file per new command
- **Testability** → engine has no CLI dependency, unit-testable standalone

