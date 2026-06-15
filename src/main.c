#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "nurl.h"
#include "nurl_cli.h"
#include "nurl_runner.h"
#include "nurl_config.h"
#include "nurl_net.h"
#include "nurl_utils.h"

#ifdef _WIN32
#include <io.h>
#define is_stdin_a_pipe() (!_isatty(_fileno(stdin)))
#else
#include <unistd.h>
#define is_stdin_a_pipe() (!isatty(STDIN_FILENO))
#endif

static void print_help(const char *prog_name) {
    printf("NetworkURL (nurl) — A clean, fast, and structured HTTP client CLI\n\n");
    printf("Usage:\n");
    printf("  %s [command] <URL> [options]\n\n", prog_name);
    printf("Commands:\n");
    printf("  get               Send a GET request to a URL (default)\n");
    printf("  post              Send a POST request to a URL\n");
    printf("  put               Send a PUT request to a URL\n");
    printf("  delete            Send a DELETE request to a URL\n");
    printf("  head              Fetch HTTP headers from a URL\n");
    printf("  patch             Send a PATCH request to a URL\n");
    printf("  options           Show Allow and Access-Control headers\n");
    printf("  download          Stream a file download to disk\n");
    printf("  upload            Upload a file as multipart/form-data\n");
    printf("  inspect           Inspect the request headers and body without sending\n");
    printf("  ping              Ping a URL to measure latency\n");
    printf("  resolve           Resolve host DNS records\n\n");
    printf("Options:\n");
    printf("  -u, --user <val>      Basic auth credentials (username:password)\n");
    printf("  --bearer <val>        Bearer token authorization\n");
    printf("  --token <val>         API token authorization\n");
    printf("  --no-auth             Strip any Authorization headers\n");
    printf("  -d, --data <val>      HTTP POST/PUT request body payload\n");
    printf("  -j, --json            Shorthand: sets Content-Type to application/json\n");
    printf("  -k, --no-verify       Skip TLS/SSL certificate verification\n");
    printf("  --cacert <file>       CA certificate PEM bundle path\n");
    printf("  --cert <file>         TLS client certificate PEM file\n");
    printf("  --key <file>          TLS private key file\n");
    printf("  -x, --proxy <val>     HTTP proxy URL or host:port to tunnel requests\n");
    printf("  --proxy-user <val>    Proxy authentication credentials (username:password)\n");
    printf("  --no-proxy <val>      Comma-separated list of hosts to bypass proxy for\n");
    printf("  -t, --timeout <sec>   Maximum request timeout in seconds (default: 30)\n");
    printf("  -L, --location        Follow HTTP 3xx redirections\n");
    printf("  -H, --header <val>    Pass custom header line (e.g. \"X-Custom: value\")\n");
    printf("  -o, --output <file>   Save response body to local file\n");
    printf("  -i, --include         Include response headers in the output\n");
    printf("  -v, --verbose         Print verbose logs with request (> ) and response (< ) details\n");
    printf("  -s, --silent          Suppress all output logging\n");
    printf("  --raw                 Print raw, unformatted JSON responses\n");
    printf("  --compressed          Request compressed response and decompress automatically\n");
    printf("  -e, --referer <val>   Referer URL header value\n");
    printf("  -f, --fail            Fail silently on server errors (return non-zero on 4xx/5xx)\n");
    printf("  --retry <num>         Number of retries on network/5xx transient errors\n");
    printf("  --retry-delay <sec>   Delay between retries in seconds (default: 1)\n");
    printf("  --tls1.2              Enforce TLS v1.2 protocol\n");
    printf("  --tls1.3              Enforce TLS v1.3 protocol\n");
    printf("  -h, --help            Print this help dialogue\n");
}

int main(int argc, char **argv) {
    if (nurl_net_init() != 0) {
        fprintf(stderr, "Error: Network initialization failed.\n");
        return 1;
    }

    CommonArgs args;
    char *command = NULL;
    char *url = NULL;

    int parse_res = nurl_cli_parse(argc, argv, &args, &command, &url);
    if (parse_res != 0) {
        if (parse_res == -1) {
            print_help(argv[0]);
            parse_res = 0; // Exit with 0 on help
        }
        nurl_cli_free_args(&args);
        nurl_net_cleanup();
        return parse_res;
    }

    // Load and merge default configurations
    nurl_config_load_and_merge(&args);

    // Convert command to uppercase to match standard HTTP verbs
    char *method = strdup(command);
    for (size_t i = 0; i < strlen(method); i++) {
        method[i] = (char)toupper((unsigned char)method[i]);
    }

    // If -d - is specified or (method is POST/PUT/PATCH and stdin is a pipe and no data is set)
    bool is_write_method = (strcmp(method, "POST") == 0 || strcmp(method, "PUT") == 0 || strcmp(method, "PATCH") == 0);
    bool explicit_stdin = (args.data && strcmp(args.data, "-") == 0);
    if (explicit_stdin || (is_write_method && is_stdin_a_pipe() && !args.data)) {
        size_t stdin_len = 0;
        char *stdin_payload = nurl_utils_read_stdin(&stdin_len);
        if (stdin_payload) {
            if (args.data) free(args.data);
            args.data = stdin_payload;
            args.data_len = stdin_len;
        }
    }

    int result = nurl_runner_execute(method, url, &args);

    free(method);
    free(command);
    free(url);
    nurl_cli_free_args(&args);
    nurl_net_cleanup();

    return result;
}
