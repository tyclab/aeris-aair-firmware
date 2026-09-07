#pragma once

#include "Particle.h"

// Cable-free application update, the way ESPHome does it but on Device OS's
// own YMODEM receiver: System.firmwareUpdate() takes any Stream, and a
// TCPClient is one. Device OS checks the module CRC and platform before the
// bootloader swaps the image in, so a bad upload is refused, not booted.
//
// The listener only opens for a short window after POST /api/v2/system/update:
// the receiver blocks the application loop for the transfer, and a port that is
// always open would let any stray connection stall the purifier.
class OtaServer {
public:
    explicit OtaServer(int port);

    void arm(uint32_t now_ms);
    void tick(uint32_t now_ms);
    bool armed() const;
    int port() const;

private:
    static const uint32_t kWindowMs = 60000;

    TCPServer server_;
    int port_;
    uint32_t window_until_ms_;
    bool listening_;
};
