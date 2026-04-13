ESP-IDF WebSockets Fork
=======================

This repository is a project-specific fork of Links2004/arduinoWebSockets.

The WebSocket protocol layer and high-level client API are retained, but the ESP32 client transport has been changed from Arduino-derived TLS/network classes to ESP-IDF networking primitives.

What changed in this fork
-------------------------

- The client-side plain TCP transport in NETWORK_CUSTOM mode is implemented with esp_tls_plain_tcp_connect and lwIP sockets.
- The client-side WSS transport in NETWORK_CUSTOM mode is implemented with esp_tls.
- TLS read and write use esp_tls_conn_read, esp_tls_conn_write, and esp_tls_get_bytes_avail.
- Root CA handling can use either an explicit CA certificate or the ESP-IDF certificate bundle via esp_crt_bundle_attach.
- The consuming firmware uses WEBSOCKETS_NETWORK_TYPE=10 so the library resolves to the custom network classes in this fork.
- This removes the previous dependency on Arduino NetworkClientSecure for the active ESP32 websocket client path.

Current scope
-------------

This fork is intended for the ESP32 client path used by this workspace.

- Protocol handling remains based on RFC6455 as implemented by the upstream library.
- The public WebSocketsClient API remains compatible with the existing application code.
- The active transport migration is focused on client mode.
- Server-side functionality and non-ESP32 targets are not the primary target of this fork.

Supported RFC6455 features
--------------------------

- text frame
- binary frame
- connection close
- ping
- pong
- continuation frame

Transport architecture
----------------------

The active ESP32 client path uses the custom network type defined in src/WebSockets.h.

- NETWORK_CUSTOM maps the client classes to WebSocketsNetworkClient and WebSocketsNetworkClientSecure.
- WebSocketsNetworkClient provides plain TCP using esp_tls_plain_tcp_connect plus socket read, write, peek, and available handling.
- WebSocketsNetworkClientSecure provides WSS using esp_tls.
- TLS teardown is handled through esp_tls_conn_destroy.

The custom transport is implemented in these files.

- src/WebSocketsNetworkClient.h
- src/WebSocketsNetworkClient.cpp
- src/WebSocketsNetworkClientSecure.h
- src/WebSocketsNetworkClientSecure.cpp
- src/network_client_impl.h

ESP-IDF TLS behavior
--------------------

Secure websocket connections in this fork rely on ESP-IDF TLS facilities.

- setCACert stores a PEM CA certificate for server verification.
- setCACertBundle enables the ESP-IDF CA bundle path.
- setCertificate and setPrivateKey pass client certificate material into esp_tls_cfg_t.
- setInsecure keeps API compatibility with the original client class behavior.

The implementation is intended to run in an ESP32 PlatformIO environment that enables ESP-IDF alongside Arduino framework compatibility.

Workspace integration
---------------------

In this workspace the library is consumed as a local dependency from the ESP32 firmware project.

Example PlatformIO configuration:

```ini
lib_deps =
    symlink://../espidfWebSockets

build_flags =
    -DWEBSOCKETS_NETWORK_TYPE=10
```

The corresponding firmware is built in a hybrid environment:

```ini
framework =
    arduino
    espidf
```

This arrangement preserves the existing application-facing WebSocketsClient usage while moving the underlying websocket transport to ESP-IDF.

Application API
---------------

The existing high-level client API remains available, including:

- begin
- beginSSL
- onEvent
- loop
- sendTXT
- sendBIN

The event type enum remains the same as the upstream library, so existing callback code can continue to operate without protocol-layer changes.

Limitations
-----------

- This README describes the active ESP32 client transport used in this fork, not a full rework of every upstream target.
- The implementation is validated in the current PlatformIO ESP32 project, not as a general replacement for every upstream example.
- Async mode and non-ESP32 board support should be treated as upstream behavior unless separately reworked.

Upstream origin
---------------

This repository originates from Links2004/arduinoWebSockets, but the README intentionally describes the fork's current ESP-IDF-backed transport behavior rather than the original Arduino-centric distribution.

License
-------

The library remains under LGPLv2.1. See LICENSE.

libb64 is included under its original terms in src/libb64/LICENSE.
