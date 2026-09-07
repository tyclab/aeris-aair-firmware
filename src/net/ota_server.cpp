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

bool isHex(const char* s, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        char c = s[i];
        bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        if (!ok) {
            return false;
        }
    }
    return true;
}
}  // namespace

OtaServer::OtaServer(int port, const SettingsV2* settings)
    : server_(port),
      settings_(settings),
      window_until_ms_(0),
      listening_(false),
      pending_since_ms_(0),
      line_len_(0),
      fail_streak_(0),
      denied_count_(0) {
    memset(nonce_hex_, 0, sizeof(nonce_hex_));
    memset(line_, 0, sizeof(line_));
}

bool OtaServer::arm(uint32_t now_ms) {
    if (locked()) {
        return false;
    }
    window_until_ms_ = now_ms + kWindowMs;
    if (!listening_) {
        listening_ = server_.begin();
    }
    return listening_;
}

void OtaServer::tick(uint32_t now_ms) {
    if (!listening_) {
        return;
    }
    if (static_cast<int32_t>(now_ms - window_until_ms_) >= 0) {
        close();
        return;
    }

    if (!pending_.connected()) {
        pending_ = server_.available();
        if (!pending_.connected()) {
            return;
        }
        pending_since_ms_ = now_ms;
        line_len_ = 0;
        sendChallenge(pending_);
        return;
    }

    switch (readAnswer()) {
        case Answer::Incomplete:
            if (now_ms - pending_since_ms_ >= kHandshakeMs) {
                dropPending();  // silent or dawdling: not a guess, not counted
            }
            return;
        case Answer::Malformed:
            dropPending();  // junk is not a guess either
            return;
        case Answer::Wrong:
            denied_count_ += 1;
            fail_streak_ += 1;
            pending_.println("denied");
            dropPending();
            if (locked()) {
                close();
            }
            return;
        case Answer::Right:
            fail_streak_ = 0;
            pending_.println("ok");
            // Success reboots from inside firmwareUpdate(); failure returns here.
            System.firmwareUpdate(&pending_);
            close();
            return;
    }
}

bool OtaServer::locked() const {
    return fail_streak_ >= kMaxDenied;
}

uint32_t OtaServer::deniedCount() const {
    return denied_count_;
}

void OtaServer::setDeniedCount(uint32_t count) {
    denied_count_ = count;
}

void OtaServer::close() {
    dropPending();
    server_.stop();
    listening_ = false;
}

void OtaServer::dropPending() {
    pending_.stop();
    line_len_ = 0;
}

void OtaServer::sendChallenge(TCPClient& client) {
    uint8_t nonce[kNonceBytes];
    for (size_t i = 0; i < kNonceBytes; i += 4) {
        uint32_t r = HAL_RNG_GetRandomNumber();
        memcpy(nonce + i, &r, 4);
    }
    toHex(nonce, kNonceBytes, nonce_hex_);
    client.printf("nonce=%s\n", nonce_hex_);
}

// Consumes only what has already arrived; a line spans ticks if it has to.
OtaServer::Answer OtaServer::readAnswer() {
    while (pending_.available() > 0) {
        int c = pending_.read();
        if (c < 0) {
            break;
        }
        if (c == '\n') {
            line_[line_len_] = '\0';
            line_len_ = 0;
            return judge(line_);
        }
        if (c == '\r') {
            continue;
        }
        if (line_len_ + 1 >= sizeof(line_)) {
            return Answer::Malformed;
        }
        line_[line_len_++] = static_cast<char>(c);
    }
    return Answer::Incomplete;
}

// "<cnonce hex> <sha256 hex>"; only a well-formed answer counts as a guess.
OtaServer::Answer OtaServer::judge(const char* line) const {
    const char* space = strchr(line, ' ');
    if (space == nullptr) {
        return Answer::Malformed;
    }
    size_t cnonce_len = static_cast<size_t>(space - line);
    const char* answer = space + 1;
    if (cnonce_len != kNonceBytes * 2 || strlen(answer) != 64 ||
        !isHex(line, cnonce_len) || !isHex(answer, 64)) {
        return Answer::Malformed;
    }

    uint8_t msg[sizeof(settings_->ota_pass) + kNonceBytes * 4];
    size_t n = 0;
    size_t pass_len = strnlen(settings_->ota_pass, sizeof(settings_->ota_pass));
    memcpy(msg + n, settings_->ota_pass, pass_len);
    n += pass_len;
    memcpy(msg + n, nonce_hex_, kNonceBytes * 2);
    n += kNonceBytes * 2;
    memcpy(msg + n, line, kNonceBytes * 2);
    n += kNonceBytes * 2;

    uint8_t digest[32];
    sha256(msg, n, digest);
    char expected[65];
    toHex(digest, sizeof(digest), expected);

    uint8_t diff = 0;
    for (size_t i = 0; i < 64; ++i) {
        diff |= static_cast<uint8_t>(expected[i] ^ answer[i]);
    }
    return (pass_len > 0 && diff == 0) ? Answer::Right : Answer::Wrong;
}
