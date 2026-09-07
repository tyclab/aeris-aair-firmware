#include "ota_server.h"

OtaServer::OtaServer(int port)
    : server_(port), port_(port), window_until_ms_(0), listening_(false) {}

void OtaServer::arm(uint32_t now_ms) {
    window_until_ms_ = now_ms + kWindowMs;
    if (!listening_) {
        listening_ = server_.begin();
    }
}

void OtaServer::tick(uint32_t now_ms) {
    if (!listening_) {
        return;
    }
    if (static_cast<int32_t>(now_ms - window_until_ms_) >= 0) {
        server_.stop();
        listening_ = false;
        return;
    }
    TCPClient client = server_.available();
    if (!client.connected()) {
        return;
    }
    // Success reboots from inside firmwareUpdate(); failure returns here.
    System.firmwareUpdate(&client);
    client.stop();
    server_.stop();
    listening_ = false;
}

bool OtaServer::armed() const {
    return listening_;
}

int OtaServer::port() const {
    return port_;
}
