#pragma once

#include <stddef.h>
#include <stdint.h>

// Device OS does not export its mbedtls to user firmware, so the OTA handshake
// carries its own SHA-256 (FIPS 180-4, no streaming API needed).
void sha256(const uint8_t* data, size_t len, uint8_t out[32]);
