#!/usr/bin/env python3
"""Send one PROV line to a unit in setup mode over USB serial, retried.

Usage:
  serial_provision.py <device_id> <topic_root> <ssid> <mqtt_host> [port] [tty]

Secrets come from the environment so they never land in a shell history:
  WIFI_PASS, MQTT_USER, MQTT_PASS
"""
import os, sys, termios, time

if len(sys.argv) < 5:
    sys.exit(__doc__)
device_id, topic_root, ssid, mqtt_host = sys.argv[1:5]
mqtt_port = sys.argv[5] if len(sys.argv) > 5 else "1883"
tty = sys.argv[6] if len(sys.argv) > 6 else "/dev/ttyACM0"
try:
    wifi_pass, mqtt_user, mqtt_pass = (os.environ[k] for k in ("WIFI_PASS", "MQTT_USER", "MQTT_PASS"))
except KeyError as e:
    sys.exit(f"missing {e} in the environment")

fd = os.open(tty, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
attrs = termios.tcgetattr(fd)
attrs[0] = attrs[1] = attrs[3] = 0  # raw: no echo, no CR/LF mangling
attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
attrs[4] = attrs[5] = termios.B9600  # anything but 14400 (DFU) / 28800 (listening)
termios.tcsetattr(fd, termios.TCSANOW, attrs)

prov = "PROV\t" + "\t".join([ssid, wifi_pass, mqtt_host, mqtt_port, mqtt_user, mqtt_pass,
                             topic_root, device_id]) + "\n"

for attempt in range(1, 6):
    os.write(fd, b"x")  # leave listening mode; its console owns the same port
    time.sleep(0.3)
    os.write(fd, b"\n")  # flush any stray chars in the unit's line buffer
    time.sleep(0.3)
    os.write(fd, prov.encode())
    deadline, buf = time.time() + 8, b""
    while time.time() < deadline:
        try:
            buf += os.read(fd, 256)
        except BlockingIOError:
            time.sleep(0.1)
        if b"PROV OK" in buf:
            print(f"attempt {attempt}: {buf.decode(errors='replace').strip()}")
            sys.exit(0)
        if b"PROV ERR" in buf:
            sys.exit(f"attempt {attempt}: {buf.decode(errors='replace').strip()}")
    print(f"attempt {attempt}: no reply", file=sys.stderr)
sys.exit("gave up: is the unit in setup mode on this port?")
