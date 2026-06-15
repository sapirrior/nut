# NetworkURL (nurl)

NetworkURL (`nurl`) is a clean, fast, portable, and structured HTTP client CLI written in C. 

Unlike traditional command-line HTTP clients that clutter the terminal with complex layouts, `nurl` is built with a simple design philosophy: **plain text, structured details, and machine-parseable outputs by default.**

---

## 1. Project Architecture

The codebase is organized into nested modules dividing user-interface commands from the request execution engine:

```text
src/
├── main.c                  # Program entry point (WSA startup/cleanup)
├── cli/                    # CLI Interface Layer
│   ├── parser/             # Argument parsing (nurl_cli.c)
│   ├── runner/             # Subcommand routing & output styling (nurl_request.c)
│   └── commands/           # HTTP command drivers (get.c, post.c, download.c, etc.)
└── engine/                 # Protocol & Network Engine Layer
    ├── nurl_engine.c       # Central engine request orchestrator
    ├── net/                # TCP Socket & Proxy handler (Winsock/POSIX abstractions)
    ├── tls/                # OpenSSL contexts & verification setup
    ├── http/               # HTTP parser, gzip/deflate decompression, redirect resolution
    └── utils/              # Cookies manager, configurations, base64 & high-res time utilities
```

---

## 2. Build Instructions

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
*   **Zero Bloat**: By removing HTTP/2 and HTTP/3 framing libraries, the final binary remains lean and focused.
*   **Dead Code Elimination**: The linker removes unused functions at link time via `-Wl,--gc-sections -s`, stripping all symbols to keep the final executable size compact (~2.5MB total, including OpenSSL).

---

## 3. Command Usage Guide

Instead of compiling complex command flags, `nurl` uses focused, readable subcommands:

### 3.1. Standard REST Operations

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

### 3.2. File Transfer Operations

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

### 3.3. First-Class Stdin & Stdout Piping (Like curl)

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

### 3.4. Debugging & Inspection

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

## 4. Advanced Options Reference

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

## 5. Protocol Support

`nurl` is built as an exclusive, highly-optimized **HTTP/1.1** client. 

By focusing on a rock-solid, manual implementation of the HTTP/1.1 state machine, `nurl` ensures maximum predictability and speed for CLI-based transfers without the overhead and complexity of binary framing libraries. 

*   **HTTP/1.1**: The core engine, supporting keep-alive, chunked transfers, and byte-range resumes.
*   **TLS 1.2/1.3**: Fully supported via OpenSSL with automatic ALPN negotiation for `http/1.1`.


---

## 6. Cross-Platform Compatibility

`nurl` is designed to compile natively and run identically on **Linux, macOS, and Windows**:
*   **Windows (Winsock)**: Automatically initializes socket subsystems using `WSAStartup`/`WSACleanup` and handles socket read/write calls using Winsock2 abstractions.
*   **High-Resolution Timers**: Tracks request elapsed times accurately on Windows (using `FILETIME`) and Unix-like OSes (using `gettimeofday`).
*   **Async Event Loop Engine**: Wraps `poll()` on POSIX platforms and `WSAPoll()` on Windows to support non-blocking connections gracefully.
