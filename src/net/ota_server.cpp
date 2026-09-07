#include "ota_server.h"

#include "../util/sha256.h"

#include <stdio.h>
#include <string.h>

namespace {
const size_t kNonceBytes = 16;

void toHex(const uint8_t* in, size_t len, char* out) {
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < len; ++i) {
        out[i * 2] = digits[in[i] >> 4];
        out[i * 2 + 1] = digits[in[i] & 0x0f];
    }
    out[len * 2] = '\0';
}
}  // namespace

OtaServer::OtaServer(int port, const SettingsV2* settings)
    : server_(port), settings_(settings), window_until_ms_(0), listening_(false) {}

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
    // One connection per window, right or wrong; a retry needs a new arm.
    listening_ = false;
    if (authenticate(client)) {
        client.println("ok");
        // Success reboots from inside firmwareUpdate(); failure returns here.
        System.firmwareUpdate(&client);
    } else {
        client.println("denied");
    }
    client.stop();
    server_.stop();
}

bool OtaServer::armed() const {
    return listening_;
}

bool OtaServer::authenticate(TCPClient& client) {
    uint8_t nonce[kNonceBytes];
    for (size_t i = 0; i < kNonceBytes; i += 4) {
        uint32_t r = HAL_RNG_GetRandomNumber();
        memcpy(nonce + i, &r, 4);
    }
    char nonce_hex[kNonceBytes * 2 + 1];
    toHex(nonce, kNonceBytes, nonce_hex);
    client.printf("nonce=%s\n", nonce_hex);

    // "<cnonce hex> <sha256 hex>"
    char line[160];
    if (!readLine(client, line, sizeof(line), millis() + kHandshakeMs)) {
        return false;
    }
    char* space = strchr(line, ' ');
    if (space == nullptr) {
        return false;
    }
    *space = '\0';
    const char* cnonce = line;
    const char* answer = space + 1;
    if (strlen(cnonce) != kNonceBytes * 2 || strlen(answer) != 64) {
        return false;
    }

    uint8_t msg[sizeof(settings_->ota_pass) + kNonceBytes * 4];
    size_t n = 0;
    size_t pass_len = strnlen(settings_->ota_pass, sizeof(settings_->ota_pass));
    memcpy(msg + n, settings_->ota_pass, pass_len);
    n += pass_len;
    memcpy(msg + n, nonce_hex, kNonceBytes * 2);
    n += kNonceBytes * 2;
    memcpy(msg + n, cnonce, kNonceBytes * 2);
    n += kNonceBytes * 2;

    uint8_t digest[32];
    sha256(msg, n, digest);
    char expected[65];
    toHex(digest, sizeof(digest), expected);

    uint8_t diff = 0;
    for (size_t i = 0; i < 64; ++i) {
        diff |= static_cast<uint8_t>(expected[i] ^ answer[i]);
    }
    return pass_len > 0 && diff == 0;
}

bool OtaServer::readLine(TCPClient& client, char* buf, size_t size, uint32_t deadline_ms) {
    size_t len = 0;
    while (static_cast<int32_t>(millis() - deadline_ms) < 0 && client.connected()) {
        int c = client.read();
        if (c < 0) {
            continue;
        }
        if (c == '\n') {
            buf[len] = '\0';
            return true;
        }
        if (c != '\r') {
            if (len + 1 >= size) {
                return false;
            }
            buf[len++] = static_cast<char>(c);
        }
    }
    return false;
}
