#include "nurl_cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <strings.h>

void nurl_cli_init_args(CommonArgs *args) {
    memset(args, 0, sizeof(CommonArgs));
    args->timeout = 30; // Default timeout 30s
    args->connect_timeout = 10;
    args->ping_count = 1;
    args->ping_interval = 1000;
    args->upload_name = strdup("file");
}

static bool is_subcommand(const char *arg) {
    const char *commands[] = {
        "get", "post", "put", "delete", "head", "patch", "options",
        "download", "upload", "inspect", "ping", "resolve"
    };
    size_t count = sizeof(commands) / sizeof(commands[0]);
    for (size_t i = 0; i < count; i++) {
        if (strcmp(arg, commands[i]) == 0) {
            return true;
        }
    }
    return false;
}

int nurl_cli_parse(int argc, char **argv, CommonArgs *args, char **command, char **url) {
    nurl_cli_init_args(args);

    static struct option long_options[] = {
        {"user",            required_argument, NULL, 'u'},
        {"bearer",          required_argument, NULL, 1},
        {"token",           required_argument, NULL, 2},
        {"no-auth",         no_argument,       NULL, 3},
        {"data",            required_argument, NULL, 'd'},
        {"json",            no_argument,       NULL, 'j'},
        {"no-verify",       no_argument,       NULL, 'k'},
        {"cacert",          required_argument, NULL, 4},
        {"timeout",         required_argument, NULL, 't'},
        {"location",        no_argument,       NULL, 'L'},
        {"header",          required_argument, NULL, 'H'},
        {"output",          required_argument, NULL, 'o'},
        {"include",         no_argument,       NULL, 'i'},
        {"verbose",         no_argument,       NULL, 'v'},
        {"silent",          no_argument,       NULL, 's'},
        {"raw",             no_argument,       NULL, 5},
        {"count",           required_argument, NULL, 6},
        {"interval",        required_argument, NULL, 7},
        {"resume",          no_argument,       NULL, 8},
        {"progress",        no_argument,       NULL, 9},
        {"mime",            required_argument, NULL, 10},
        {"name",            required_argument, NULL, 11},
        {"field",           required_argument, NULL, 12},
        {"cookie",          required_argument, NULL, 'b'},
        {"cookie-jar",      required_argument, NULL, 'c'},
        {"session",         required_argument, NULL, 13},
        {"write-out",       required_argument, NULL, 'w'},
        {"version",         no_argument,       NULL, 'V'},
        {"help",            no_argument,       NULL, 'h'},
        {"cert",            required_argument, NULL, 14},
        {"key",             required_argument, NULL, 15},
        {"proxy",           required_argument, NULL, 'x'},
        {"proxy-user",      required_argument, NULL, 16},
        {"no-proxy",        required_argument, NULL, 17},
        {"user-agent",      required_argument, NULL, 'A'},
        {"compressed",      no_argument,       NULL, 18},
        {"retry",           required_argument, NULL, 19},
        {"retry-delay",     required_argument, NULL, 20},
        {"referer",         required_argument, NULL, 'e'},
        {"fail",            no_argument,       NULL, 'f'},
        {NULL, 0, NULL, 0}
    };

    int opt;
    opterr = 0; // Disable default getopt error printing

    while ((opt = getopt_long(argc, argv, "u:d:jt:LH:o:ivshkb:c:w:Vx:A:e:f", long_options, NULL)) != -1) {
        switch (opt) {
            case 'u':
                if (args->user) free(args->user);
                args->user = strdup(optarg);
                if (!args->user) {
                    fprintf(stderr, "Error: Out of memory.\n");
                    return -1;
                }
                break;
            case 1:
                if (args->bearer) free(args->bearer);
                args->bearer = strdup(optarg);
                if (!args->bearer) {
                    fprintf(stderr, "Error: Out of memory.\n");
                    return -1;
                }
                break;
            case 2:
                if (args->token) free(args->token);
                args->token = strdup(optarg);
                if (!args->token) {
                    fprintf(stderr, "Error: Out of memory.\n");
                    return -1;
                }
                break;
            case 3:
                args->no_auth = true;
                break;
            case 'd':
                if (args->data) free(args->data);
                args->data = strdup(optarg);
                if (!args->data) {
                    fprintf(stderr, "Error: Out of memory.\n");
                    return -1;
                }
                break;
            case 'j':
                args->json = true;
                break;
            case 'k':
                args->no_verify = true;
                break;
            case 4:
                if (args->cacert) free(args->cacert);
                args->cacert = strdup(optarg);
                if (!args->cacert) {
                    fprintf(stderr, "Error: Out of memory.\n");
                    return -1;
                }
                break;
            case 't':
                args->timeout = strtoul(optarg, NULL, 10);
                break;
            case 'L':
                args->location = true;
                break;
            case 'H': {
                char **temp = realloc(args->header, sizeof(char *) * (args->header_count + 1));
                if (!temp) {
                    fprintf(stderr, "Error: Out of memory.\n");
                    return -1;
                }
                args->header = temp;
                args->header[args->header_count] = strdup(optarg);
                if (!args->header[args->header_count]) {
                    fprintf(stderr, "Error: Out of memory.\n");
                    return -1;
                }
                args->header_count++;
                break;
            }
            case 'o':
                if (args->output) free(args->output);
                args->output = strdup(optarg);
                if (!args->output) {
                    fprintf(stderr, "Error: Out of memory.\n");
                    return -1;
                }
                break;
            case 'i':
                args->include = true;
                break;
            case 'v':
                args->verbose = true;
                break;
            case 's':
                args->silent = true;
                break;
            case 5:
                args->raw = true;
                break;
            case 6:
                args->ping_count = (unsigned int)strtoul(optarg, NULL, 10);
                break;
            case 7:
                args->ping_interval = strtoul(optarg, NULL, 10);
                break;
            case 8:
                args->resume = true;
                break;
            case 9:
                args->progress = true;
                break;
            case 10:
                if (args->upload_mime) free(args->upload_mime);
                args->upload_mime = strdup(optarg);
                if (!args->upload_mime) {
                    fprintf(stderr, "Error: Out of memory.\n");
                    return -1;
                }
                break;
            case 11:
                if (args->upload_name) free(args->upload_name);
                args->upload_name = strdup(optarg);
                if (!args->upload_name) {
                    fprintf(stderr, "Error: Out of memory.\n");
                    return -1;
                }
                break;
            case 12: {
                char **temp = realloc(args->upload_fields, sizeof(char *) * (args->upload_fields_count + 1));
                if (!temp) {
                    fprintf(stderr, "Error: Out of memory.\n");
                    return -1;
                }
                args->upload_fields = temp;
                args->upload_fields[args->upload_fields_count] = strdup(optarg);
                if (!args->upload_fields[args->upload_fields_count]) {
                    fprintf(stderr, "Error: Out of memory.\n");
                    return -1;
                }
                args->upload_fields_count++;
                break;
            }
            case 'b':
                if (args->cookie) free(args->cookie);
                args->cookie = strdup(optarg);
                if (!args->cookie) {
                    fprintf(stderr, "Error: Out of memory.\n");
                    return -1;
                }
                break;
            case 'c':
                if (args->cookie_jar) free(args->cookie_jar);
                args->cookie_jar = strdup(optarg);
                if (!args->cookie_jar) {
                    fprintf(stderr, "Error: Out of memory.\n");
                    return -1;
                }
                break;
            case 13:
                if (args->session) free(args->session);
                args->session = strdup(optarg);
                if (!args->session) {
                    fprintf(stderr, "Error: Out of memory.\n");
                    return -1;
                }
                break;
            case 'w':
                if (args->write_out) free(args->write_out);
                args->write_out = strdup(optarg);
                if (!args->write_out) {
                    fprintf(stderr, "Error: Out of memory.\n");
                    return -1;
                }
                break;
            case 14:
                if (args->cert) free(args->cert);
                args->cert = strdup(optarg);
                if (!args->cert) {
                    fprintf(stderr, "Error: Out of memory.\n");
                    return -1;
                }
                break;
            case 15:
                if (args->key) free(args->key);
                args->key = strdup(optarg);
                if (!args->key) {
                    fprintf(stderr, "Error: Out of memory.\n");
                    return -1;
                }
                break;
            case 'x':
                if (args->proxy) free(args->proxy);
                args->proxy = strdup(optarg);
                if (!args->proxy) {
                    fprintf(stderr, "Error: Out of memory.\n");
                    return -1;
                }
                break;
            case 16:
                if (args->proxy_user) free(args->proxy_user);
                args->proxy_user = strdup(optarg);
                if (!args->proxy_user) {
                    fprintf(stderr, "Error: Out of memory.\n");
                    return -1;
                }
                break;
            case 17:
                if (args->no_proxy) free(args->no_proxy);
                args->no_proxy = strdup(optarg);
                if (!args->no_proxy) {
                    fprintf(stderr, "Error: Out of memory.\n");
                    return -1;
                }
                break;
            case 'A':
                if (args->user_agent) free(args->user_agent);
                args->user_agent = strdup(optarg);
                if (!args->user_agent) {
                    fprintf(stderr, "Error: Out of memory.\n");
                    return -1;
                }
                break;
            case 18:
                args->compressed = true;
                break;
            case 19:
                args->retry = (unsigned int)strtoul(optarg, NULL, 10);
                break;
            case 20:
                args->retry_delay = strtoul(optarg, NULL, 10);
                break;
            case 'e':
                if (args->referer) free(args->referer);
                args->referer = strdup(optarg);
                if (!args->referer) {
                    fprintf(stderr, "Error: Out of memory.\n");
                    return -1;
                }
                break;
            case 'f':
                args->fail = true;
                break;
            case 'V':
                printf("nurl %s\n", NURL_VERSION);
                exit(0);
            case 'h':
                return -1; // Help requested
            default:
                fprintf(stderr, "nurl: option %s: is unknown\n", argv[optind - 1]);
                return -1;
        }
    }

    int remaining = argc - optind;
    if (remaining <= 0) {
        fprintf(stderr, "nurl: no URL specified!\n");
        return -1;
    }

    if (remaining >= 2) {
        const char *first = argv[optind];
        if (is_subcommand(first)) {
            *command = strdup(first);
            if (strcasecmp(first, "upload") == 0) {
                if (remaining >= 3) {
                    *url = strdup(argv[optind + 1]);
                    args->upload_file = strdup(argv[optind + 2]);
                } else {
                    fprintf(stderr, "nurl: subcommand 'upload' requires target URL and local file path.\n");
                    free(*command);
                    *command = NULL;
                    return -1;
                }
            } else {
                *url = strdup(argv[optind + 1]);
            }
        } else {
            *command = strdup("get");
            *url = strdup(first);
        }
    } else {
        const char *first = argv[optind];
        if (is_subcommand(first)) {
            fprintf(stderr, "nurl: subcommand '%s' requires a target URL.\n", first);
            return -1;
        } else {
            *command = strdup("get");
            *url = strdup(first);
        }
    }

    return 0;
}

void nurl_cli_free_args(CommonArgs *args) {
    if (args) {
        free(args->user);
        free(args->bearer);
        free(args->token);
        free(args->data);
        free(args->cacert);
        free(args->output);
        free(args->upload_file);
        free(args->upload_name);
        free(args->upload_mime);
        free(args->cookie);
        free(args->cookie_jar);
        free(args->session);
        free(args->write_out);
        free(args->cert);
        free(args->key);
        free(args->proxy);
        free(args->proxy_user);
        free(args->no_proxy);
        free(args->user_agent);
        free(args->referer);
        if (args->upload_fields) {
            for (size_t i = 0; i < args->upload_fields_count; i++) {
                free(args->upload_fields[i]);
            }
            free(args->upload_fields);
        }
        if (args->header) {
            for (size_t i = 0; i < args->header_count; i++) {
                free(args->header[i]);
            }
            free(args->header);
        }
        memset(args, 0, sizeof(CommonArgs));
    }
}
