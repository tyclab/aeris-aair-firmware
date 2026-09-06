# Changelog

All notable changes to this project are documented in this file.

## [Unreleased]

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
