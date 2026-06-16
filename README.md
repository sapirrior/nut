# NetworkURL (nurl)

NetworkURL (`nurl`) is a clean, fast, portable, and structured HTTP client CLI written in C. 

Unlike traditional command-line HTTP clients that clutter the terminal with complex layouts, `nurl` is built with a simple design philosophy: **plain text, structured details, and smart diagnostics by default.**

---

## 1. Project Architecture

The codebase is organized into nested modules dividing user-interface commands from the request execution engine:

```text
src/
├── main.c                  # Program entry point (WSA startup/cleanup)
├── cli/                    # CLI Interface Layer
│   ├── parser/             # Argument parsing (nurl_cli.c)
│   ├── runner/             # Subcommand routing & progress reporting (nurl_progress.c)
│   └── commands/           # HTTP command drivers (download.c, upload.c, ping.c, etc.)
├── engine/                 # Protocol & Network Engine Layer
│   ├── nurl_engine.c       # Central engine request orchestrator (Stage-based)
│   ├── net/                # Buffered I/O (NurlStream) & Proxy handler
│   ├── tls/                # OpenSSL contexts & verification setup
│   ├── http/               # HTTP parser, gzip/deflate decompression, redirects
│   └── utils/              # Cookies manager, configurations, base64 & high-res time
└── errors/                 # Smart Error DX Layer
    ├── nurl_diag.c         # Concise Unix-style diagnostics
    ├── nurl_error_handler.c # Context-aware diagnostic logic
    └── nurl_error.c        # Centralized error code definitions
```

---

## 2. Smart Error DX (Developer Experience)

`nurl` features a context-aware diagnostic system designed to help you solve issues quickly. Instead of cryptic error codes or bulky blocks, you get concise, standard Unix-style messages with helpful hints:

```text
nurl: error: network connection reset or interrupted during the request to 'https://api.example.com'
      hint: check your internet connection or verify if the server is reachable
      hint: since you are downloading a file to disk, you can attempt to pick up where you left off by adding the --resume flag
```

---

## 3. Build Instructions

### Prerequisites
Make sure you have GCC/Clang, `libssl-dev` (OpenSSL), and `zlib1g-dev` installed.

### Compiling
To clean and build the optimized, statically linked binary directly:
```bash
make clean
make
```

### Build Size & Portability Features
The [Makefile](Makefile) is tuned for production-grade builds:
*   **Static Linking**: Links `libssl` and `libcrypto` statically (`-Wl,-Bstatic -lssl -lcrypto`), producing an executable with zero external OpenSSL dependencies.
*   **Size Optimization**: Compiled with `-Os` (size optimization), `-ffunction-sections`, and `-fdata-sections`.
*   **Zero Bloat**: By strictly focusing on HTTP/1.1 and removing legacy dependencies, the binary remains lean and high-performance.
*   **Dead Code Elimination**: The linker removes unused functions at link time via `-Wl,--gc-sections -s`, stripping all symbols to keep the final executable size compact (~2.5MB total, including OpenSSL).

---

## 4. Command Usage Guide

Instead of compiling complex command flags, `nurl` uses focused, readable subcommands:

### 4.1. Standard REST Operations

#### GET Request
```bash
nurl get https://httpbin.org/get -i
```
*(The `-i` / `--include` option outputs the HTTP response headers above the JSON body).*

#### POST JSON Payload
```bash
nurl post https://httpbin.org/post -d '{"developer": "sapirrior", "tool": "nurl"}' -j
```
*(The `-j` flag is shorthand to automatically attach `Content-Type: application/json` headers).*

#### PUT & DELETE
```bash
nurl put https://httpbin.org/put -d 'payload'
nurl delete https://httpbin.org/delete
```

---

### 4.2. File Transfer Operations

#### Streaming Downloads (With Auto-Resume)
Download files directly to disk without buffering the entire payload in memory. If a transfer disconnects, pick up exactly where it stopped:
```bash
nurl download https://example.com/large-archive.zip -o output.zip --resume
```

#### Multipart File Uploads
```bash
nurl upload https://api.example.com/media ./image.jpg --field user_id="101" --name avatar
```

---

### 4.3. First-Class Stdin & Stdout Piping (Like curl)

`nurl` supports stdin/stdout streams out of the box, making it fully compatible with Unix pipes:

#### Piping Response to Another Command (Stdout Redirection)
Stream files directly to `stdout` using `-o -` or silence connection/progress logs using `-s` while outputting the body payload:
```bash
nurl get https://claude.ai/install.sh -L -s | bash
nurl download https://httpbin.org/image/png -o - > output.png
```

#### Uploading/Sending Streams (Stdin Redirection)
Automatically read payloads from `stdin` during write requests (`POST`, `PUT`, `PATCH`), or when `-d -` is explicitly passed. This supports both text and binary data safely:
```bash
echo "hello world" | nurl post https://httpbin.org/post -j
cat image.png | nurl post https://api.example.com/upload -d -
```

---

### 4.4. Debugging & Inspection

#### Latency Analysis (Ping)
Check host response times over TLS/TCP:
```bash
nurl ping https://api.github.com --count 3
```

#### Dry-run Request Inspection
Inspect the generated headers and payload outline exactly as they would be sent, **without launching an outbound network call**:
```bash
nurl inspect post https://api.example.com/data -H "Authorization: Bearer confidential_token" -j
```
*(Sensitive headers like `Authorization` or cookies are automatically printed as `[hidden]` to prevent leaking secrets in console logs).*

#### DNS Host Resolution
```bash
nurl resolve httpbin.org
```

---

## 5. Advanced Options Reference

| Flag / Option | Description |
| :--- | :--- |
| `--compressed` | Request gzip/deflate compression and automatically decompress response payloads using `zlib`. |
| `-e, --referer <URL>` | Set custom `Referer` headers. |
| `-f, --fail` | Fail silently on server errors, suppressing body outputs and returning exit codes 22 (4xx) or 43 (5xx). |
| `--retry <num>` | Number of retries on transient connection or 5xx failures. |
| `--retry-delay <sec>` | Wait duration between retries in seconds (default: 1). |
| `--tls1.2` | Enforce TLS v1.2 connections only. |
| `--tls1.3` | Enforce TLS v1.3 connections only. |
| `-k, --no-verify` | Skip TLS certificate validation (useful for localhost self-signed APIs). |
| `-x, --proxy <url>` | Tunnel outbound traffic through an HTTP proxy. |

---

## 6. Protocol Support

`nurl` is built as an exclusive, highly-optimized **HTTP/1.1** client. 

By focusing on a rock-solid, manual implementation of the HTTP/1.1 state machine combined with a high-performance buffered I/O system, `nurl` ensures maximum predictability and speed for CLI-based transfers.

*   **Buffered I/O (8KB)**: Uses a unified `NurlStream` abstraction for both raw TCP and TLS, drastically reducing syscall overhead and improving throughput (especially during encrypted transfers).
*   **HTTP/1.1**: The core engine, supporting keep-alive connection pooling, chunked transfers, and byte-range resumes.
*   **TLS 1.2/1.3**: Fully supported via OpenSSL with automatic ALPN negotiation.
*   **Modular Pipeline**: Connections are orchestrated in distinct stages (DNS -> TCP -> Proxy -> TLS Handshake), ensuring that failures are accurately diagnosed at the correct layer.


---

## 7. Cross-Platform Compatibility

`nurl` is designed to compile natively and run identically on **Linux, macOS, and Windows**:
*   **Windows (Winsock)**: Automatically initializes socket subsystems using `WSAStartup`/`WSACleanup` and handles socket read/write calls using Winsock2 abstractions.
*   **High-Resolution Timers**: Tracks request elapsed times accurately on Windows (using `FILETIME`) and Unix-like OSes (using `gettimeofday`).
*   **Async Event Loop Engine**: Wraps `poll()` on POSIX platforms and `WSAPoll()` on Windows to support non-blocking connections gracefully.
