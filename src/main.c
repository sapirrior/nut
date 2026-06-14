#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "nurl.h"
#include "nurl_cli.h"
#include "nurl_runner.h"
#include "nurl_config.h"

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
    printf("  -t, --timeout <sec>   Maximum request timeout in seconds (default: 30)\n");
    printf("  -L, --location        Follow HTTP 3xx redirections\n");
    printf("  -H, --header <val>    Pass custom header line (e.g. \"X-Custom: value\")\n");
    printf("  -o, --output <file>   Save response body to local file\n");
    printf("  -i, --include         Include response headers in the output\n");
    printf("  -v, --verbose         Print verbose logs with request (> ) and response (< ) details\n");
    printf("  -s, --silent          Suppress all output logging\n");
    printf("  --raw                 Print raw, unformatted JSON responses\n");
    printf("  -h, --help            Print this help dialogue\n");
}

int main(int argc, char **argv) {
    CommonArgs args;
    char *command = NULL;
    char *url = NULL;

    if (nurl_cli_parse(argc, argv, &args, &command, &url) != 0) {
        print_help(argv[0]);
        nurl_cli_free_args(&args);
        return 1;
    }

    // Load and merge default configurations
    nurl_config_load_and_merge(&args);

    // Convert command to uppercase to match standard HTTP verbs
    char *method = strdup(command);
    for (size_t i = 0; i < strlen(method); i++) {
        method[i] = (char)toupper((unsigned char)method[i]);
    }

    int result = nurl_runner_execute(method, url, &args);

    free(method);
    free(command);
    free(url);
    nurl_cli_free_args(&args);

    return result;
}
