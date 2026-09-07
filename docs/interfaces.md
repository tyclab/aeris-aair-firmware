# Aeris AAIR v2 Interfaces

## MQTT Namespace
Root: `aeris/v2/<device_id>`

### Command Topics
- `aeris/v2/<device_id>/cmd/fan_percent` payload `0..100`
- `aeris/v2/<device_id>/cmd/lights` payload `0|1`
- `aeris/v2/<device_id>/cmd/screen_light` payload `0|1` (A1 TFT backlight)
- `aeris/v2/<device_id>/cmd/ring` payload `0..255` key-light pattern byte (`0x03` power, `0x0C` AirQ, `0x30` down, `0xC0` up)
- `aeris/v2/<device_id>/cmd/ring_brightness` payload `0..100` percent, quantised to 5 duty levels; `state/ring_brightness` reports the quantised percent (`0`, `25`, `50`, `75` or `100`)
- `aeris/v2/<device_id>/cmd/ring_blink` payload half period in ms, `0` = solid
- `aeris/v2/<device_id>/cmd/status_led` payload `0|1`
- `aeris/v2/<device_id>/cmd/filter_days` payload days of filter life remaining

### State Topics
- `aeris/v2/<device_id>/state/fan_percent`
- `aeris/v2/<device_id>/state/fan_pwm`
- `aeris/v2/<device_id>/state/lights`
- `aeris/v2/<device_id>/state/ring`
- `aeris/v2/<device_id>/state/ring_brightness`
- `aeris/v2/<device_id>/state/ring_blink`
- `aeris/v2/<device_id>/state/status_led`
- `aeris/v2/<device_id>/sensor/pm25`
- `aeris/v2/<device_id>/sensor/pm10`
- `aeris/v2/<device_id>/sensor/filter_minutes`
- `aeris/v2/<device_id>/health/uptime_s`
- `aeris/v2/<device_id>/health/wifi_reconnect_count`
- `aeris/v2/<device_id>/health/mqtt_reconnect_count`
- `aeris/v2/<device_id>/health/mqtt_publish_drop_count`
- `aeris/v2/<device_id>/health/ota_denied_count`
- `aeris/v2/<device_id>/health/sensor_parse_errors`
- `aeris/v2/<device_id>/health/command_drop_{button,mqtt,web}_count`

Payloads are primitive strings.

## Serial Provisioning
In setup mode the application reads lines on USB serial (any baud except 14400 and 28800, which Device OS reserves for DFU and listening mode):
- `PROV\t<ssid>\t<wifi_pass>\t<mqtt_host>\t<mqtt_port>\t<mqtt_user>\t<mqtt_pass>\t<topic_root>\t<device_id>[\t<ota_pass>]` saves settings and reboots; replies `PROV OK rebooting` or `PROV ERR <reason>`.
- `IP?` prints the current IP address.
- Both are ignored while listening mode is active, because Device OS's own console reads the same port and would consume part of the line. Leave listening mode with `x` first (`w` starts the Wi-Fi wizard, it does not exit).
- `PROV ERR` reasons: `fields` (fewer than eight tab-separated values; a ninth, `ota_pass`, is optional and cleared when absent), `ota_pass` (shorter than 16 characters), `port` (not 1-65535), `ssid` (empty), `host` (empty), `too long` (a field exceeds its stored size), `device_id` (not alphanumeric/`_`/`-`), `topic_root` (not slash-separated alphanumeric/`_`/`-` segments), `save` (EEPROM write failed). Fields are never silently truncated or normalised.

## Web API
Base path on device local IP:
- `GET /` serves the built-in Web UI dashboard.
- `GET /api/v2/settings` returns current settings JSON (without secret redaction logic).
- `POST /api/v2/settings` with urlencoded form updates settings (`wifi_ssid`, `wifi_pass`, `mqtt_host`, `mqtt_user`, `mqtt_pass`, `device_id`, `mqtt_topic_root`, display fields). Secrets are accepted, never returned; `ota_pass` is not accepted here, only over USB `PROV`.
- `GET /api/v2/state` returns live runtime state (`pm25`, `pm10`, fan, connectivity, `screen_light_on`), plus `firmware_version` and `firmware_build`, which is the compile timestamp and so only truthful after a clean build.
- `POST /api/v2/control` with urlencoded form sends runtime commands (`fan_percent`, `lights`, `screen_light`).
- `POST /api/v2/system/reboot` requests reboot.
- `POST /api/v2/system/dfu` requests DFU mode.
- `POST /api/v2/system/update` opens TCP 3232 for 60 s. The unit sends `nonce=<32 hex>`; the client answers `<cnonce 32 hex> <sha256(ota_pass + nonce + cnonce) hex>` and gets `ok` or `denied`. With `ota_pass` unset the POST answers 409; after three well-formed wrong answers it answers 423 until reboot, and `health/ota_denied_count` counts every wrong answer, persisted across reboots. A connection that stays silent for 500 ms or sends a malformed line is dropped uncounted. After `ok` the socket is Device OS's YMODEM receiver; `tools/ota.py` does all of it and the unit reboots into the verified image.

Validation failures return HTTP 400 with JSON body.

## SoftAP Setup
Captive portal path handled by `softap_http`:
- `GET /save?s=<ssid>&p=<pass>` stores Wi-Fi credentials and reboots.
