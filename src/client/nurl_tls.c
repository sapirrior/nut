#include "nurl_tls.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>
#include <stdlib.h>

struct nurl_tls {
    SSL_CTX *ctx;
    SSL *ssl;
    bool verify;
};

nurl_tls_t *nurl_tls_create(bool verify_certs, const char *cacert_path, const char *cert_path, const char *key_path) {
    // Initialize OpenSSL library
    static bool openssl_initialized = false;
    if (!openssl_initialized) {
        OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, NULL);
        openssl_initialized = true;
    }

    nurl_tls_t *tls = malloc(sizeof(nurl_tls_t));
    if (!tls) {
        return NULL;
    }
    tls->ssl = NULL;
    tls->verify = verify_certs;

    // Use modern TLS client method
    const SSL_METHOD *method = TLS_client_method();
    tls->ctx = SSL_CTX_new(method);
    if (!tls->ctx) {
        free(tls);
        return NULL;
    }

    // Configure certificate verification
    if (!verify_certs) {
        SSL_CTX_set_verify(tls->ctx, SSL_VERIFY_NONE, NULL);
    } else {
        SSL_CTX_set_verify(tls->ctx, SSL_VERIFY_PEER, NULL);
        if (cacert_path) {
            if (SSL_CTX_load_verify_locations(tls->ctx, cacert_path, NULL) != 1) {
                // Failed to load custom CA cert
                SSL_CTX_free(tls->ctx);
                free(tls);
                return NULL;
            }
        } else {
            // Load standard system paths
            SSL_CTX_set_default_verify_paths(tls->ctx);
        }
    }

    // Configure client certificates if provided
    if (cert_path) {
        if (SSL_CTX_use_certificate_chain_file(tls->ctx, cert_path) != 1) {
            SSL_CTX_free(tls->ctx);
            free(tls);
            return NULL;
        }
    }
    if (key_path) {
        if (SSL_CTX_use_PrivateKey_file(tls->ctx, key_path, SSL_FILETYPE_PEM) != 1) {
            SSL_CTX_free(tls->ctx);
            free(tls);
            return NULL;
        }
        // Verify private key matches certificate
        if (cert_path && SSL_CTX_check_private_key(tls->ctx) != 1) {
            SSL_CTX_free(tls->ctx);
            free(tls);
            return NULL;
        }
    }

    return tls;
}

int nurl_tls_handshake(nurl_tls_t *tls, int socket_fd, const char *hostname) {
    if (!tls) {
        return -1;
    }

    tls->ssl = SSL_new(tls->ctx);
    if (!tls->ssl) {
        return -1;
    }

    SSL_set_fd(tls->ssl, socket_fd);

    // Set Server Name Indication (SNI) header
    SSL_set_tlsext_host_name(tls->ssl, hostname);

    // Enforce certificate hostname validation if verifying
    if (tls->verify) {
        X509_VERIFY_PARAM *param = SSL_get0_param(tls->ssl);
        X509_VERIFY_PARAM_set1_host(param, hostname, 0);
    }

    int ret = SSL_connect(tls->ssl);
    if (ret != 1) {
        SSL_free(tls->ssl);
        tls->ssl = NULL;
        return -1;
    }

    return 0;
}

int nurl_tls_read(nurl_tls_t *tls, void *buf, int len) {
    if (!tls || !tls->ssl) {
        return -1;
    }
    return SSL_read(tls->ssl, buf, len);
}

int nurl_tls_write(nurl_tls_t *tls, const void *buf, int len) {
    if (!tls || !tls->ssl) {
        return -1;
    }
    return SSL_write(tls->ssl, buf, len);
}

void nurl_tls_close(nurl_tls_t *tls) {
    if (tls && tls->ssl) {
        SSL_shutdown(tls->ssl);
        SSL_free(tls->ssl);
        tls->ssl = NULL;
    }
}

void nurl_tls_free(nurl_tls_t *tls) {
    if (tls) {
        nurl_tls_close(tls);
        if (tls->ctx) {
            SSL_CTX_free(tls->ctx);
        }
        free(tls);
    }
}
