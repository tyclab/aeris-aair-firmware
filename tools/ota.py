#!/usr/bin/env python3
"""Flash the application over Wi-Fi: arm the unit, then send the image by YMODEM.

Usage:
  ota.py <host> <target/src.bin> [port]

POST /api/v2/system/update opens a 60 s listener; the unit's own Device OS YMODEM
receiver takes the file, verifies it and reboots into it.
"""
import json, os, socket, sys, time, urllib.request

SOH, STX, EOT, ACK, NAK, CA, CRC = b"\x01", b"\x02", b"\x04", b"\x06", b"\x15", b"\x18", b"C"

if len(sys.argv) < 3:
    sys.exit(__doc__)
host, path = sys.argv[1:3]
port = int(sys.argv[3]) if len(sys.argv) > 3 else 3232
image = open(path, "rb").read()


def crc16(data):
    crc = 0
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc.to_bytes(2, "big")


def packet(header, seq, data):
    return header + bytes([seq & 0xFF, (~seq) & 0xFF]) + data + crc16(data)


def wait_for(sock, wanted, timeout):
    sock.settimeout(timeout)
    text = b""
    while True:
        c = sock.recv(1)
        if not c:
            sys.exit("unit closed the connection\n" + text.decode(errors="replace"))
        if c in wanted:
            return c
        text += c  # the receiver prints its verdict as text


with urllib.request.urlopen(f"http://{host}/api/v2/system/update", data=b"") as r:
    arm = json.load(r)
if not arm.get("ok"):
    sys.exit(f"arm failed: {arm}")

sock = None
for _ in range(20):
    try:
        sock = socket.create_connection((host, port), timeout=3)
        break
    except OSError:
        time.sleep(0.5)
if sock is None:
    sys.exit(f"{host}:{port} never opened")

wait_for(sock, CRC, 15)
name = os.path.basename(path).encode() + b"\0" + str(len(image)).encode() + b" "
sock.sendall(packet(SOH, 0, name.ljust(128, b"\0")))
if wait_for(sock, ACK + CA, 10) == CA:
    sys.exit("unit refused the header (image too large for the slot?)")
wait_for(sock, CRC, 10)

seq, sent = 1, 0
while sent < len(image):
    chunk = image[sent:sent + 1024].ljust(1024, b"\0")
    for _ in range(5):
        sock.sendall(packet(STX, seq, chunk))
        reply = wait_for(sock, ACK + NAK + CA, 10)
        if reply == ACK:
            break
        if reply == CA:
            sys.exit("unit aborted while saving a chunk")
    else:
        sys.exit(f"chunk {seq} never acknowledged")
    seq += 1
    sent += 1024
    print(f"\r{min(sent, len(image))}/{len(image)} bytes", end="", flush=True)
print()

sock.sendall(EOT)
wait_for(sock, ACK, 10)
sock.sendall(packet(SOH, 0, bytes(128)))  # empty header ends the session
verdict = wait_for(sock, ACK + CA, 30)
sock.settimeout(3)
try:
    while True:
        text = sock.recv(256)
        if not text:
            break
        print(text.decode(errors="replace"), end="")
except OSError:
    pass
print()
sys.exit(0 if verdict == ACK else "unit rejected the image")
