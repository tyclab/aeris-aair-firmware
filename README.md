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
| `cmd/ring_brightness` | percent `0..100`, 5 steps | `state/ring_brightness` |
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

`/api/v2/state` reports `firmware_version` and `firmware_build`, and the build
stamp is `__DATE__`/`__TIME__`. The stamped object lives under
`device-os/build`, not `target/`, so `make` touches its source every run; a
build always carries its own stamp.

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

The board has the Particle `RESET` and `SETUP` buttons on its back, so DFU does
not depend on a working application: hold `SETUP`, tap `RESET`, and release when
the status LED blinks yellow. Holding `SETUP` alone for about three seconds gives
listening mode, which is the serial console the bootloader update below needs.
Never hold past yellow to white; that is factory reset, and the factory image on
an OEM unit is not ours. See `docs/hardware-components-and-interfaces.md` 6.4.

### Over Wi-Fi

Once a unit runs a build with the update listener, no cable is needed:

```bash
OTA_PASS=… tools/ota.py 10.27.4.209 target/src.bin
```

`POST /api/v2/system/update` opens TCP 3232 for 60 s. The unit greets with a
nonce and the tool answers `sha256(OTA_PASS + nonce + cnonce)`, the same
challenge-response ESPHome's OTA uses, with a secret each unit carries in its
settings (`ota_pass`, set over USB at provisioning, never over the network,
never returned); the secret never crosses the wire, and three wrong answers
lock the listener until reboot. The receiver is the same one listening mode
uses on USB, so a valid secret can also deliver system parts or a bootloader;
treat it accordingly. Then the tool sends the image by YMODEM to Device OS's own
receiver, the one listening mode uses on USB. Device OS checks the module CRC
and platform before the bootloader swaps it in, so a bad image is refused
rather than booted. The application loop is blocked for the transfer, a few
seconds; the fan keeps its PWM. Settings and the filter record are untouched,
as with DFU.

### Browser flasher

https://tyclab.github.io/aeris-aair-firmware/flasher/ &mdash; desktop Chrome,
Edge or Opera (needs WebUSB and Web Serial). Puts the unit into DFU, writes
the application, and can optionally also write the Device OS 2.3.1 system
parts; a second card sends the serial provisioning line from the section
below. It never writes the bootloader, so bootloader v7 &rarr; v1003 stays
the CLI/YMODEM step above.

The unit re-enumerates under a different USB product id when it drops into
DFU (`2b04:c006` &rarr; `2b04:d006`), and WebUSB permission is per product id.
The first time, the page asks for that permission via a "Select the DFU
device" button, opening the chooser a second time; once granted &mdash; and for
a unit already sitting in DFU &mdash; the flash runs without any further
chooser.

On Windows 10/11 both product ids bind WinUSB automatically; no driver
install. On Linux, either add a udev rule for vendor id `2b04` or run the
browser as a user with access to the USB device node.

Local: `python3 -m http.server` from `docs/`, then open `/flasher/`.

## Provision

In setup mode the application reads one line on USB serial:

```
PROV\t<ssid>\t<wifi_pass>\t<mqtt_host>\t<mqtt_port>\t<mqtt_user>\t<mqtt_pass>\t<topic_root>\t<device_id>[\t<ota_pass>]\n
```

The optional ninth field (16+ characters) is the unit's own secret for Wi-Fi
updates. It is set over USB only, never over the network, and never read
back; without it `POST /api/v2/system/update` answers 409. It answers `PROV OK rebooting` or `PROV ERR <reason>`. `IP?` returns the
current address. `tools/serial_provision.py` sends the line and retries.
Leave listening mode first with `x`; while it is active Device OS's console
owns the same port, and the application refuses to parse until it has gone.
Oversized, empty and topic-unsafe fields are rejected rather than truncated
or normalised. Both tools send `x` themselves before the line.
The SoftAP flow from upstream still works (`Aeris-XXXX`, `192.168.0.1`).

## Regenerate the gauge

```bash
python3 tools/gen_gauge.py tools/squid.svg src/assets/gauge_logo.h 176
```

Renders the SVG once per path (needs `rsvg-convert`) to learn which pixel
belongs to which tentacle and emits per-row RLE. Never hand-edit the header.

## Buttons

- Up: fan `+5%`. Down: fan `-5%`. Hold either one and it repeats: after
  500 ms it steps every 150 ms, so a hold sweeps 0 to 100 in about three
  seconds while a tap still moves a single 5% step.
- AirQ: short press toggles the key lights; hold 3 s toggles Wi-Fi.
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
