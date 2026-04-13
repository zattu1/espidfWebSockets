#include "WebSocketsNetworkClientSecure.h"

#include "network_client_impl.h"

#include <lwip/sockets.h>
#include <sys/ioctl.h>

#include <cstring>

namespace {

constexpr int kDefaultSocketTimeoutMs = 5000;

ssize_t tlsRead(esp_tls_t *tls, void *buffer, size_t size) {
    if (tls == nullptr || buffer == nullptr || size == 0) {
        return -1;
    }

    const ssize_t readBytes = esp_tls_conn_read(tls, buffer, size);
    return readBytes > 0 ? readBytes : 0;
}

}  // namespace

WebSocketsNetworkClientSecure::WebSocketsNetworkClientSecure()
    : WebSocketsNetworkClient() {
    _impl->secure = true;
}

WebSocketsNetworkClientSecure::WebSocketsNetworkClientSecure(WiFiClient wifi_client)
    : WebSocketsNetworkClient(wifi_client) {
    _impl->secure = true;
}

WebSocketsNetworkClientSecure::~WebSocketsNetworkClientSecure() {
    stop();
}

int WebSocketsNetworkClientSecure::connect(IPAddress ip, uint16_t port) {
    return connect(ip.toString().c_str(), port);
}

int WebSocketsNetworkClientSecure::connect(const char *host, uint16_t port) {
    return connect(host, port, kDefaultSocketTimeoutMs);
}

int WebSocketsNetworkClientSecure::connect(const char *host, uint16_t port, int32_t timeout) {
    return (_impl != nullptr && _impl->connect_tls(host, port, timeout)) ? 1 : 0;
}

size_t WebSocketsNetworkClientSecure::write(uint8_t data) {
    return write(&data, 1);
}

size_t WebSocketsNetworkClientSecure::write(const uint8_t *buf, size_t size) {
    if (_impl == nullptr || _impl->tls == nullptr || buf == nullptr || size == 0) {
        return 0;
    }

    const ssize_t writtenBytes = esp_tls_conn_write(_impl->tls, buf, size);
    return writtenBytes > 0 ? static_cast<size_t>(writtenBytes) : 0;
}

size_t WebSocketsNetworkClientSecure::write(const char *str) {
    return str != nullptr ? write(reinterpret_cast<const uint8_t *>(str), strlen(str)) : 0;
}

int WebSocketsNetworkClientSecure::available() {
    if (_impl == nullptr || _impl->tls == nullptr) {
        return 0;
    }

    int availableBytes = static_cast<int>(esp_tls_get_bytes_avail(_impl->tls));
    if (availableBytes == 0) {
        int socketFd = -1;
        if (esp_tls_get_conn_sockfd(_impl->tls, &socketFd) == ESP_OK && socketFd >= 0) {
            int socketBytes = 0;
            if (ioctl(socketFd, FIONREAD, &socketBytes) == 0 && socketBytes > 0) {
                availableBytes = socketBytes;
            }
        }
    }
    if (_impl->has_peeked_byte) {
        availableBytes += 1;
    }
    return availableBytes;
}

int WebSocketsNetworkClientSecure::read() {
    uint8_t byte = 0;
    const int readBytes = read(&byte, 1);
    return readBytes == 1 ? byte : -1;
}

int WebSocketsNetworkClientSecure::read(uint8_t *buf, size_t size) {
    if (_impl == nullptr || _impl->tls == nullptr || buf == nullptr || size == 0) {
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

    const ssize_t readBytes = tlsRead(_impl->tls, buf + offset, size - offset);
    return static_cast<int>(offset + (readBytes > 0 ? readBytes : 0));
}

int WebSocketsNetworkClientSecure::peek() {
    if (_impl == nullptr || _impl->tls == nullptr) {
        return -1;
    }

    if (_impl->has_peeked_byte) {
        return _impl->peeked_byte;
    }

    uint8_t byte = 0;
    const ssize_t readBytes = tlsRead(_impl->tls, &byte, 1);
    if (readBytes != 1) {
        return -1;
    }

    _impl->peeked_byte = byte;
    _impl->has_peeked_byte = true;
    return byte;
}

void WebSocketsNetworkClientSecure::flush() {}

void WebSocketsNetworkClientSecure::stop() {
    if (_impl) {
        _impl->close_socket();
    }
}

uint8_t WebSocketsNetworkClientSecure::connected() {
    return (_impl != nullptr && _impl->tls != nullptr) ? 1 : 0;
}

WebSocketsNetworkClientSecure::operator bool() {
    return connected() != 0;
}

void WebSocketsNetworkClientSecure::setCACert(const char *rootCA) {
    if (_impl == nullptr) {
        return;
    }
    _impl->ca_cert = rootCA;
    _impl->use_ca_bundle = (rootCA == nullptr);
}

void WebSocketsNetworkClientSecure::setCACertBundle(const uint8_t *bundle, size_t bundle_size) {
    (void)bundle;
    (void)bundle_size;
    if (_impl == nullptr) {
        return;
    }
    _impl->use_ca_bundle = true;
}

void WebSocketsNetworkClientSecure::setCertificate(const char *client_ca) {
    if (_impl) {
        _impl->client_cert = client_ca;
    }
}

void WebSocketsNetworkClientSecure::setPrivateKey(const char *private_key) {
    if (_impl) {
        _impl->private_key = private_key;
    }
}

void WebSocketsNetworkClientSecure::setInsecure() {
    if (_impl == nullptr) {
        return;
    }
    _impl->insecure = true;
    _impl->use_ca_bundle = true;
}

bool WebSocketsNetworkClientSecure::verify(const char *fingerprint, const char *domain_name) {
    (void)fingerprint;
    (void)domain_name;
    return true;
}