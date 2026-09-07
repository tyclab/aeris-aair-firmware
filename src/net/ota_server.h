#pragma once

#include "Particle.h"

#include "../core/settings_store.h"

// Cable-free application update, the way ESPHome does it but on Device OS's
// own YMODEM receiver: System.firmwareUpdate() takes any Stream, and a
// TCPClient is one. Device OS checks the module CRC and platform before the
// bootloader swaps the image in, so a bad upload is refused, not booted.
//
// The listener only opens for a short window after POST /api/v2/system/update,
// and the first line on the socket is an ESPHome-style challenge-response:
// the unit sends a nonce, the client answers sha256(password + nonce + cnonce),
// the unit's own ota_pass being the shared secret. The password never crosses
// the wire, and a wrong answer closes the window, so a stray or hostile
// connection costs the purifier at most the handshake timeout.
class OtaServer {
public:
    OtaServer(int port, const SettingsV2* settings);

    void arm(uint32_t now_ms);
    void tick(uint32_t now_ms);
    bool armed() const;

private:
    static const uint32_t kWindowMs = 60000;
    static const uint32_t kHandshakeMs = 3000;

    TCPServer server_;
    const SettingsV2* settings_;
    uint32_t window_until_ms_;
    bool listening_;

    bool authenticate(TCPClient& client);
    bool readLine(TCPClient& client, char* buf, size_t size, uint32_t deadline_ms);
};
