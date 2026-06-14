# nurl: A Modern Developer's Guide to NetworkURL

Welcome to `nurl` (NetworkURL)! This guide will walk you through installing, configuring, and mastering `nurl` for your daily development workflows.

Unlike traditional HTTP clients that clutter your screen with verbose layouts and flashing colors, `nurl` is built with a simple philosophy: **plain text, structured details, and machine-parseable outputs by default.**

---

## 1. Installation & Setup

### Requirements
Ensure you have the Rust toolchain installed. If not, get it from [rustup.rs](https://rustup.rs/).

### Installation from Crates.io
You can install `nurl` directly via Cargo:

```bash
cargo install networkurl
```

### Installing from Source
Clone the repository and compile the binary:

```bash
git clone https://github.com/sapirrior/nurl.git
cd nurl
cargo install --path .
```

Verify the installation is successful:
```bash
nurl --version
```

---

## 2. Getting Started: Your First Request

With `nurl`, you don't need to remember verbose subcommands for simple tasks. 

### Quick GET
To test a simple endpoint, simply type `nurl` followed by the host. `nurl` automatically normalizes the URL scheme to `https://` and triggers a GET request:

```bash
nurl postman-echo.com/get
```

*Output:*
```json
{
  "args": {},
  "headers": {
    "host": "postman-echo.com",
    "user-agent": "nurl/0.1.0"
  },
  "url": "https://postman-echo.com/get"
}
```

---

## 3. Practical Guides

### Guide A: Working with REST APIs

When developing APIs, you constantly transition between GET, POST, and PUT requests. 

#### 1. Fetching Resource Details (GET)
To explicitly make a GET request and inspect headers:
```bash
nurl get https://httpbin.org/json --include
```
*Note: The `--include` (or `-i`) flag prints the HTTP response headers directly above the body.*

#### 2. Sending JSON Payloads (POST)
To POST data, use the `-d` (data) flag. Adding `-j` (JSON shorthand) automatically injects the `Content-Type: application/json` header:
```bash
nurl post https://httpbin.org/post -d '{"username": "nolan", "role": "admin"}' -j
```

#### 3. Bypassing Certificates on Local Host
If you are developing locally with self-signed TLS certificates (e.g. `https://localhost:8443`), bypass TLS verification securely using `-k` / `--no-verify`:
```bash
nurl get https://localhost:8443/api/status -k
```

---

### Guide B: Working with Files

`nurl` splits file operations into dedicated subcommands to optimize resource management.

#### 1. Downloading Files with Resume Support
Rather than buffering large downloads in system memory, the `download` command streams data directly to disk. If the download gets cut off, simply append `--resume` to pick up where you left off:

```bash
nurl download https://example.com/large-archive.zip -o local.zip --resume
```

#### 2. Uploading Files (Multipart Form Data)
Use the `upload` command to send binary files (like images or zip packages) alongside text fields:

```bash
nurl upload https://api.example.com/media ./profile.jpg --field user_id="42" --name avatar
```

---

### Guide C: Debugging & Health Checks

#### 1. Checking Latency (Ping)
Quickly test if an endpoint is alive and see its response times. Specify the `--count` flag to perform multiple pings and receive min/avg/max latency statistics:

```bash
nurl ping https://api.github.com --count 3
```

*Output:*
```text
200  OK  api.github.com  88ms
200  OK  api.github.com  92ms
200  OK  api.github.com  89ms

min 88ms  avg 89ms  max 92ms
```

#### 2. Dry-Run Inspection
If you are debugging a complex authorization token or request header payload, use the `inspect` subcommand. This parses and prints your request layout exactly as it would be sent, **without making any network call**:

```bash
nurl inspect post https://api.example.com/data -H "Authorization: Bearer confidential_tok" -j
```

*Output:*
```http
> POST /data HTTP/1.1
> Host: api.example.com
> Content-Type: application/json
> Authorization: [hidden]
> Content-Length: 0
>
```
*Note: Sensitive values like `Authorization` tokens are automatically printed as `[hidden]` to prevent leaking secrets in console logs.*

#### 3. Checking CORS headers
Inspect cross-origin rules supported by an endpoint using `options`:
```bash
nurl options https://api.example.com/users
```

---

## 4. Configuration Guide

Save time by adding persistent default headers and options in `~/.config/nurl/config.toml`:

```toml
[defaults]
timeout = 15
connect_timeout = 5
follow_redirects = true
user_agent = "nurl/0.1.0"

[headers]
# These headers will be attached to every outgoing request
Accept = "application/json"
X-Developer-Client = "nurl"
```

### Environment Variable Overrides
For quick shell customizations, `nurl` respects environment variables:

- `NURL_BEARER`: Automatically injects `Authorization: Bearer <token>` into requests.
- `NURL_TIMEOUT`: Overrides default request timeouts.

---

## 5. Command & Flag Cheat Sheet

| Command | Action | Key Flags |
| :--- | :--- | :--- |
| `get` | GET Request | `-i` (include headers) |
| `post` / `put` / `patch` | Write Requests | `-d` (body), `-j` (JSON header) |
| `delete` | DELETE Request | `--header` (custom headers) |
| `options` | CORS Header Check | None |
| `download` | Streaming Download | `-o` (output file), `--resume` |
| `upload` | Multipart File Upload | `--field key=val`, `--name` (field name) |
| `inspect` | Dry-run Request Check | Takes standard subcommand args |
| `ping` | Latency Check | `--count <n>`, `--interval <ms>` |
| `resolve` | Resolve host DNS | None |
