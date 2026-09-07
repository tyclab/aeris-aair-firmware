#pragma once

#include "Particle.h"

#include "../core/settings_store.h"

// Cable-free firmware update, the way ESPHome does it but on Device OS's own
// YMODEM receiver: System.firmwareUpdate() takes any Stream, and a TCPClient
// is one. Device OS checks the module CRC and platform before the bootloader
// swaps the image in, so a bad upload is refused, not booted. The receiver is
// the one listening mode uses on USB and takes system parts and the
// bootloader as readily as the application, so the secret guards all three.
//
// The listener only opens for a short window after POST /api/v2/system/update.
// Every connection is challenged ESPHome-style: the unit sends a nonce, the
// client answers sha256(ota_pass + nonce + cnonce); ota_pass is set over USB
// only, so no network peer can replace it. Three wrong answers lock the
// listener until reboot; a connection that never answers is dropped uncounted,
// so a port scanner can neither lock the unit nor stall it beyond the
// handshake timeout.
class OtaServer {
public:
    OtaServer(int port, const SettingsV2* settings);

    bool arm(uint32_t now_ms);
    void tick(uint32_t now_ms);
    bool locked() const;
    uint32_t deniedCount() const;

private:
    static const uint32_t kWindowMs = 60000;
    static const uint32_t kHandshakeMs = 500;
    static const uint8_t kMaxDenied = 3;

    TCPServer server_;
    const SettingsV2* settings_;
    uint32_t window_until_ms_;
    bool listening_;
    TCPClient pending_;
    uint32_t pending_since_ms_;
    char nonce_hex_[33];
    uint8_t fail_streak_;
    uint32_t denied_count_;

    void close();
    void sendChallenge(TCPClient& client);
    bool verify(const char* line) const;
    bool readLine(TCPClient& client, char* buf, size_t size, uint32_t deadline_ms);
};
