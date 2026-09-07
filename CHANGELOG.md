# Changelog

All notable changes to this project are documented in this file.

## [Unreleased]

### Board

- `/api/v2/state` reports `firmware_version` and `firmware_build`, so two units running
  different unreleased work can be told apart without consulting a flashing log.
- The filter countdown decrements and persists every 10 min instead of hourly, so a
  restart forfeits at most 10 min of filter life.
- The shift-register driver is `KeyLightDriver`: it drives the four key lights, the ring
  glow is bleed. MQTT topics and state fields keep the `ring` name.

- Application updates over Wi-Fi: `POST /api/v2/system/update` opens TCP 3232 for 60 s,
  the unit challenges with a nonce and accepts `sha256(ota_pass + nonce + cnonce)`
  (ESPHome-style, the secret never on the wire), then `tools/ota.py` sends the image
  by YMODEM. Device OS's own receiver and verification do the work, so nothing new
  writes flash.
- Settings schema 5 adds `ota_pass`, a per-unit secret set by the ninth `PROV` field,
  the browser flasher or `POST /api/v2/settings`, and never returned. A schema 4 record
  is taken over unchanged with the secret unset, so the flash that introduces it keeps
  Wi-Fi and broker; updates stay refused (409) until the secret is set.

### Provisioning

- Serial provisioning refuses to parse while listening mode is active. Device OS's own
  console reads the same port and would consume part of the `PROV` line.
- Empty, oversized and topic-unsafe fields are rejected (`PROV ERR ssid`, `PROV ERR host`,
  `PROV ERR too long`, `PROV ERR device_id`, `PROV ERR topic_root`) instead of being silently
  truncated or normalised into a unit that provisions cleanly and never connects.
- `tools/serial_provision.py` and the browser flasher send `x` themselves before the line, so
  the fallback still works against a unit that boots straight into listening mode.
- The setup web server no longer pretends to serve the SoftAP interface. `TCPServer::begin()`
  and `available()` both return early unless `Network.ready()`, which listening mode never
  satisfies, so the config API is station-only.

### Board

- Up and down repeat while held, 500 ms before the first repeat then every 150 ms, so the
  full fan range takes about three seconds instead of twenty taps. A tap is unchanged at 5%.
- State publishes coalesce to at most one burst per 400 ms, so a held button cannot outrun
  the 24-slot publish queue.

### Flasher

- Browser flasher at `docs/flasher/`: WebUSB DFU flash of the application, optional Device OS 2.3.1 system parts, and Web Serial provisioning, served from GitHub Pages.
- Bundles particle-usb 4.3.0 for the browser (`docs/flasher/vendor/`) with a Photon-only stand-in for its `UNLICENSED` device-constants peer; never writes the bootloader sector.

## [tyclab v1.1.0] - 2026-09-05

First release of the tyclab flavour, forked from upstream v1.0.0.

### Panel

- Squid fan gauge: head always lit, eight tentacles light left to right with fan speed.
- PM values coloured by European AQI band; theme from the tycstation mark.
- Wi-Fi icon mid-left, blue connected / red not.
- Panel driven BGR via MADCTL (stock wiring); colours now render as specified.
- Boot animation removed; live screen at power-on.
- Fan/PM defaults re-laid for the oval aperture (`fan_x/y` 228/104 size 4, `pm_x/y` 60/182 size 3).

### Board and interfaces

- Key-light shift-register driver: `cmd/ring`, `cmd/ring_brightness`, `cmd/ring_blink`, `cmd/status_led` with matching state topics.
- Filter lifetime countdown persisted in EEPROM: `cmd/filter_days`, `sensor/filter_minutes`.
- Serial provisioning line (`PROV\t...`, `IP?`) for headless setup; `tools/serial_provision.py`.
- Publish queue sized for a full state burst (was dropping one topic per cycle).
- First-party EC_UI board teardown in `docs/hardware-components-and-interfaces.md`.

### Build

- Local Device OS 2.3.1 build documented; Particle libraries pinned as submodules under `lib/`.
- `tools/gen_gauge.py` regenerates the gauge asset from `tools/squid.svg`.

## [v1.0.0] - 2026-02-28

### Release Scope

- First public GitHub release of this firmware.
- Public release version is `v1.0.0`.
- Interface namespace in firmware remains `v2` for compatibility:
- Web API path: `/api/v2/...`
- MQTT root: `aeris/v2/<device_id>/...`

### Added - Device Control

- Fan PWM control on `D0` with 0..100 percent mapping to 0..255.
- Power toggle behavior that restores last non-zero fan speed when turned back on.
- Light toggle behavior for panel/backlight integration.
- Dedicated screen backlight control (`screen_light`) over API/MQTT.
- Button controls:
- `D1`: fan `+5`
- `D2`: fan `-5`
- `D3` short press: toggle lights
- `D3` long press `>=5s`: toggle Wi-Fi on/off
- `D4` short press: toggle purifier power
- `D4` long press `>=8s`: clear Wi-Fi settings and reboot

### Added - PM Sensor Pipeline

- PM sensor UART ingest on `Serial1` at `9600`.
- Frame parser for 32-byte packets with header `0x32 0x3D`.
- Checksum validation (sum of first 30 bytes vs last 2 bytes).
- PM field extraction for `pm25_raw` and `pm10_raw`.
- Moving average smoothing for reported PM values.
- Sensor watchdog that sends wake/start command sequence when no data is received for 10 seconds.

### Added - Wi-Fi Provisioning and Setup Mode

- Setup mode when no saved Wi-Fi SSID exists.
- SoftAP onboarding with generated SSID format `Aeris-XXXX`.
- Setup screen showing SoftAP SSID and target setup IP (`192.168.0.1`).
- SoftAP captive endpoint `/save?s=<ssid>&p=<pass>` to store Wi-Fi and reboot.
- Wi-Fi credentials stored in EEPROM settings after sanitize/validation.

### Added - Web UI and Local Web API

- Built-in dashboard at `GET /`.
- Settings endpoints:
- `GET /api/v2/settings`
- `POST /api/v2/settings` (form-urlencoded)
- Runtime state endpoint:
- `GET /api/v2/state`
- Runtime control endpoint:
- `POST /api/v2/control` (supports `fan_percent`, `lights`, `screen_light`)
- System endpoints:
- `POST /api/v2/system/reboot`
- `POST /api/v2/system/dfu`
- Input validation with HTTP 400 responses for invalid fields.
- Queue-full handling with HTTP 503 when runtime command queue is full.

### Added - MQTT Control and Telemetry

- MQTT command subscriptions:
- `cmd/fan_percent`
- `cmd/lights`
- `cmd/screen_light`
- MQTT state publish:
- `state/fan_percent`
- `state/fan_pwm`
- `state/lights`
- `sensor/pm25`
- `sensor/pm10`
- MQTT health publish:
- `health/uptime_s`
- `health/wifi_reconnect_count`
- `health/mqtt_reconnect_count`
- `health/sensor_parse_errors`
- `health/command_drop_button_count`
- `health/command_drop_mqtt_count`
- `health/command_drop_web_count`
- `health/mqtt_publish_drop_count`
- MQTT reconnect strategy with exponential backoff and temporary suspend after repeated failures.

### Added - Runtime Reliability and Diagnostics

- EEPROM `SettingsV2` lifecycle with CRC32/magic/version/length validation.
- Settings sanitization and default fallback on invalid persisted data.
- Command queue with source-specific drop counters (button/MQTT/web).
- Publish queue drop counter for MQTT backpressure visibility.
- Display Wi-Fi status cues (connected/connecting/disconnected and temporary IP display).

### Added - Build and Flash Workflow

- Project build via `make`:
- `particle compile photon --saveTo aerisFirmware.bin`
- USB DFU workflow documented using:
- `particle usb dfu`
- `particle flash --usb ...`
