#pragma once

#include "WebSocketsNetworkClient.h"

#include <esp_crt_bundle.h>
#include <esp_tls.h>

struct WebSocketsNetworkClient::Impl {
    int socket_fd = -1;
    esp_tls_t *tls = nullptr;
    bool secure = false;
    bool insecure = false;
    bool use_ca_bundle = true;
    const char *ca_cert = nullptr;
    const char *client_cert = nullptr;
    const char *private_key = nullptr;
    bool has_peeked_byte = false;
    uint8_t peeked_byte = 0;

    void reset_peek() {
        has_peeked_byte = false;
        peeked_byte = 0;
    }

    void close_socket();
    bool connect_plain(const char *host, uint16_t port, int32_t timeout_ms);
    bool connect_tls(const char *host, uint16_t port, int32_t timeout_ms);
};