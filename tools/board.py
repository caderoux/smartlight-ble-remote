"""Talking to the CYD over its USB serial port (CH340), shared by the tools.

open_port() never resets the board: DTR and RTS are set low *before* the port opens (the CH340's RTS line drives the
ESP32's EN pin, so pyserial's default of asserting them on open would hold the board in reset). reset() is explicit.
read_for() reads only what has arrived, so a continuous stream of advertisements can't keep it reading forever.
"""

from __future__ import annotations

import time

import serial

BAUD = 921600


def open_port(port: str) -> serial.Serial:
    s = serial.Serial()
    s.port = port
    s.baudrate = BAUD
    s.timeout = 0
    s.dtr = False
    s.rts = False
    s.open()
    return s


def reset(s: serial.Serial) -> None:
    """Pulse EN (via RTS) to reboot the board into its app."""
    s.rts = True
    time.sleep(0.1)
    s.rts = False


def read_for(s: serial.Serial, seconds: float) -> bytes:
    buf = bytearray()
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        n = s.in_waiting
        if n:
            buf += s.read(n)
        else:
            time.sleep(0.01)
    return bytes(buf)


def lines(data: bytes) -> list[str]:
    return data.decode("utf-8", "replace").splitlines()


def command(s: serial.Serial, cmd: str, settle: float = 0.4) -> list[str]:
    """Send one console command and return the lines that came back in the next `settle` seconds."""
    s.write(cmd.encode("ascii") + b"\n")
    return lines(read_for(s, settle))
