# Aeris aair firmware, tyclab flavour

Cloud-free firmware for the Particle Photon inside an Aeris aair 3-in-1 (Pro)
air purifier. Local web API, MQTT, Home Assistant friendly, and a restyled
panel: the tycstation squid is the fan gauge.

This is a fork of [CliffLin/aeris-aair-air-purifier](https://github.com/CliffLin/aeris-aair-air-purifier),
which in turn builds on [mjaymeyer/aeris-aair-home-assistant](https://github.com/mjaymeyer/aeris-aair-home-assistant).
The control core, web API and MQTT namespace are theirs; the generic fixes
below are offered back upstream as pull requests. Licence stays GPL-3.

## What this flavour changes

Panel

- The squid mark is the fan gauge: the head is always lit, the eight tentacles
  light left to right with fan speed, the rest stay grey.
- PM2.5 and PM10 values are coloured by European AQI band, blue (good) through
  purple to red (very poor).
- Theme is the mark's gradient stops: `#5aa2ff`, `#8b6cf0`, `#ff4a3d`.
- Wi-Fi icon mid-left, blue when connected, red when not.
- The stock panel is wired BGR; the driver sets MADCTL accordingly instead of
  compensating every colour.
- No boot animation. The panel goes straight to the live screen.

Board

- Key-light driver for the shift register behind the four buttons: pattern,
  five brightness levels (software PWM), blink. See `docs/hardware-components-and-interfaces.md`
  section 6 for the first-party teardown of the EC_UI board.
- Filter lifetime as a persisted wall-clock countdown, published in minutes,
  settable in days.
- Serial provisioning line for headless setup (no SoftAP dance).
- Publish queue sized for a full state burst; the old size silently dropped
  one topic per cycle.

Interfaces added on top of upstream v2 (`aeris/v2/<device_id>/...`, see
`docs/interfaces.md`):

| Command topic         | Payload                   | State topic             |
| --------------------- | ------------------------- | ----------------------- |
| `cmd/ring`            | pattern byte `0..255`     | `state/ring`            |
| `cmd/ring_brightness` | `0..4`                    | `state/ring_brightness` |
| `cmd/ring_blink`      | half period ms, `0` solid | `state/ring_blink`      |
| `cmd/status_led`      | `0` or `1`                | `state/status_led`      |
| `cmd/filter_days`     | days remaining            | `sensor/filter_minutes` |

Pattern bits: `0x03` power, `0x0C` AirQ, `0x30` down, `0xC0` up.

## Hardware notes

- Stock units ship bootloader v7. Device OS 2.3.1 needs v1003 or newer and
  DFU cannot write the bootloader sector. Update it once over the listening
  mode serial console (`f` command, YMODEM) with `photon-bootloader@2.3.1+lto.bin`
  from the Device OS release, then flash `system-part1`, `system-part2` and
  this application over DFU.
- The panel is a 320x240 ST7789 behind a portrait oval aperture that clips the
  corners hard. Layout constants live at the top of `src/drivers/display_driver.cpp`;
  the fan and PM positions are also runtime settings (`fan_x`, `fan_y`,
  `fan_font_size`, `pm_x`, `pm_y`, `pm_font_size`, `*_color`).
- The perimeter LED ring is a separate LED population no register bit
  addresses. It stays dark under this firmware; mechanism unexplained.

## Build

Local Device OS build, no Particle account needed:

```bash
git clone --branch v2.3.1 https://github.com/particle-iot/device-os.git
git -C device-os submodule update --init --recursive
make -C device-os/modules/photon/user-part PLATFORM=photon APPDIR=$PWD COMPILE_LTO=n all
# -> target/src.bin
```

Needs `gcc-arm-none-eabi` 9.x on `PATH`. The Particle libraries are pinned as
submodules under `lib/` (Adafruit_ST7735_RK 1.10.4 with GFX and BusIO, MQTT
0.4.32), so clone with `--recurse-submodules`.

## Flash

```bash
stty -F /dev/ttyACM0 14400            # 14400 baud open = enter DFU
dfu-util -d 2b04:d006 -a 0 -s 0x080A0000:leave -D target/src.bin
```

Flash map: `0x08020000` system-part1, `0x08060000` system-part2, `0x080A0000`
application. Settings and the filter record live in emulated EEPROM and
survive application flashes.

## Provision

In setup mode the application reads one line on USB serial:

```
PROV\t<ssid>\t<wifi_pass>\t<mqtt_host>\t<mqtt_port>\t<mqtt_user>\t<mqtt_pass>\t<topic_root>\t<device_id>\n
```

It answers `PROV OK rebooting` or `PROV ERR <reason>`. `IP?` returns the
current address. `tools/serial_provision.py` sends the line and retries.
The SoftAP flow from upstream still works (`Aeris-XXXX`, `192.168.0.1`).

## Regenerate the gauge

```bash
python3 tools/gen_gauge.py tools/squid.svg src/assets/gauge_logo.h 176
```

Renders the SVG once per path (needs `rsvg-convert`) to learn which pixel
belongs to which tentacle and emits per-row RLE. Never hand-edit the header.

## Buttons

- Up: fan `+5%`. Down: fan `-5%`.
- AirQ: short press toggles the key lights; hold 5 s toggles Wi-Fi.
- Power: short press toggles the purifier; hold 8 s wipes Wi-Fi and reboots
  into setup mode.

## Warnings

Back up the stock firmware first (full 1 MiB flash dump over DFU) and keep
it with your board revision notes. Never flash the bootloader over DFU or
with a mismatched image; a wrong bootloader is unrecoverable without SWD.
Custom firmware can brick the unit and voids the warranty. You are
responsible for electrical safety and local compliance.

## License

GPL-3, see `LICENSE`.
