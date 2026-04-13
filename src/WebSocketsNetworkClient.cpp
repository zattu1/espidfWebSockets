#include "network_client_impl.h"

#include <lwip/inet.h>
#include <lwip/sockets.h>
#include <sys/ioctl.h>

#include <cstring>

namespace {

constexpr int kDefaultSocketTimeoutMs = 5000;

timeval timeoutToTimeval(int32_t timeoutMs) {
    const int32_t boundedTimeout = timeoutMs > 0 ? timeoutMs : kDefaultSocketTimeoutMs;
    timeval timeout = {};
    timeout.tv_sec = boundedTimeout / 1000;
    timeout.tv_usec = (boundedTimeout % 1000) * 1000;
    return timeout;
}

int readFromSocket(int socketFd, uint8_t *buffer, size_t size) {
    if (socketFd < 0 || buffer == nullptr || size == 0) {
        return -1;
    }

    const int readBytes = recv(socketFd, buffer, size, 0);
    if (readBytes <= 0) {
        return readBytes;
    }

    return readBytes;
}

}  // namespace

void WebSocketsNetworkClient::Impl::close_socket() {
    reset_peek();
    if (tls != nullptr) {
        esp_tls_conn_destroy(tls);
        tls = nullptr;
    }
    if (socket_fd >= 0) {
        closesocket(socket_fd);
        socket_fd = -1;
    }
}

bool WebSocketsNetworkClient::Impl::connect_plain(const char *host, uint16_t port, int32_t timeout_ms) {
    close_socket();

    esp_tls_cfg_t cfg = {};
    cfg.timeout_ms = timeout_ms > 0 ? timeout_ms : kDefaultSocketTimeoutMs;

    int plainSocket = -1;
    const esp_err_t err = esp_tls_plain_tcp_connect(host, strlen(host), port, &cfg, nullptr, &plainSocket);
    if (err != ESP_OK || plainSocket < 0) {
        if (plainSocket >= 0) {
            closesocket(plainSocket);
        }
        return false;
    }

    const timeval timeout = timeoutToTimeval(cfg.timeout_ms);
    setsockopt(plainSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(plainSocket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    socket_fd = plainSocket;
    secure = false;
    return true;
}

bool WebSocketsNetworkClient::Impl::connect_tls(const char *host, uint16_t port, int32_t timeout_ms) {
    close_socket();

    tls = esp_tls_init();
    if (tls == nullptr) {
        return false;
    }

    esp_tls_cfg_t cfg = {};
    cfg.timeout_ms = timeout_ms > 0 ? timeout_ms : kDefaultSocketTimeoutMs;
    if (ca_cert != nullptr) {
        cfg.cacert_buf = reinterpret_cast<const unsigned char *>(ca_cert);
        cfg.cacert_bytes = strlen(ca_cert) + 1;
    } else if (use_ca_bundle) {
        cfg.crt_bundle_attach = esp_crt_bundle_attach;
    }

    if (client_cert != nullptr) {
        cfg.clientcert_buf = reinterpret_cast<const unsigned char *>(client_cert);
        cfg.clientcert_bytes = strlen(client_cert) + 1;
    }

    if (private_key != nullptr) {
        cfg.clientkey_buf = reinterpret_cast<const unsigned char *>(private_key);
        cfg.clientkey_bytes = strlen(private_key) + 1;
    }

    if (esp_tls_conn_new_sync(host, strlen(host), port, &cfg, tls) != 1) {
        esp_tls_conn_destroy(tls);
        tls = nullptr;
        return false;
    }

    socket_fd = -1;
    secure = true;
    return true;
}

WebSocketsNetworkClient::WebSocketsNetworkClient()
    : _impl(new WebSocketsNetworkClient::Impl()) {}

WebSocketsNetworkClient::WebSocketsNetworkClient(WiFiClient wifi_client)
    : _impl(new WebSocketsNetworkClient::Impl()) {
    (void)wifi_client;
}

WebSocketsNetworkClient::~WebSocketsNetworkClient() {
    if (_impl) {
        _impl->close_socket();
    }
}

int WebSocketsNetworkClient::connect(IPAddress ip, uint16_t port) {
    return connect(ip.toString().c_str(), port);
}

int WebSocketsNetworkClient::connect(const char *host, uint16_t port) {
    return connect(host, port, kDefaultSocketTimeoutMs);
}

int WebSocketsNetworkClient::connect(const char *host, uint16_t port, int32_t timeout) {
    return (_impl != nullptr && _impl->connect_plain(host, port, timeout)) ? 1 : 0;
}

size_t WebSocketsNetworkClient::write(uint8_t data) {
    return write(&data, 1);
}

size_t WebSocketsNetworkClient::write(const uint8_t *buf, size_t size) {
    if (_impl == nullptr || _impl->socket_fd < 0 || buf == nullptr || size == 0) {
        return 0;
    }

    const int sentBytes = send(_impl->socket_fd, buf, size, 0);
    return sentBytes > 0 ? static_cast<size_t>(sentBytes) : 0;
}

size_t WebSocketsNetworkClient::write(const char *str) {
    return str != nullptr ? write(reinterpret_cast<const uint8_t *>(str), strlen(str)) : 0;
}

int WebSocketsNetworkClient::available() {
    if (_impl == nullptr || _impl->socket_fd < 0) {
        return 0;
    }

    int availableBytes = 0;
    if (ioctl(_impl->socket_fd, FIONREAD, &availableBytes) != 0) {
        availableBytes = 0;
    }
    if (_impl->has_peeked_byte) {
        availableBytes += 1;
    }
    return availableBytes;
}

int WebSocketsNetworkClient::read() {
    uint8_t byte = 0;
    const int readBytes = read(&byte, 1);
    return readBytes == 1 ? byte : -1;
}

int WebSocketsNetworkClient::read(uint8_t *buf, size_t size) {
    if (_impl == nullptr || _impl->socket_fd < 0 || buf == nullptr || size == 0) {
        return 0;
    }

    size_t offset = 0;
    if (_impl->has_peeked_byte) {
        buf[0] = _impl->peeked_byte;
        _impl->reset_peek();
        offset = 1;
    }

    if (offset == size) {
        return static_cast<int>(offset);
    }

    const int readBytes = readFromSocket(_impl->socket_fd, buf + offset, size - offset);
    if (readBytes <= 0) {
        return static_cast<int>(offset);
    }

    return static_cast<int>(offset + readBytes);
}

int WebSocketsNetworkClient::peek() {
    if (_impl == nullptr || _impl->socket_fd < 0) {
        return -1;
    }

    if (_impl->has_peeked_byte) {
        return _impl->peeked_byte;
    }

    uint8_t byte = 0;
    const int readBytes = readFromSocket(_impl->socket_fd, &byte, 1);
    if (readBytes != 1) {
        return -1;
    }

    _impl->peeked_byte = byte;
    _impl->has_peeked_byte = true;
    return byte;
}

void WebSocketsNetworkClient::flush() {}

void WebSocketsNetworkClient::stop() {
    if (_impl) {
        _impl->close_socket();
    }
}

uint8_t WebSocketsNetworkClient::connected() {
    return (_impl != nullptr && _impl->socket_fd >= 0) ? 1 : 0;
}

WebSocketsNetworkClient::operator bool() {
    return connected() != 0;
}