"""Record what the CYD hears, for a fixed time, into captures/ (git-ignored: it holds nearby devices' addresses).

    python tools/capture.py COM35 120 ["SEND LIGHT OFF" ...]

Any further arguments are console commands, sent one second apart at the start, so a send and whatever answers it
(an echo from the fan's receiver) land in the same capture.

Every line from the board is written with the PC's time in front ("<pc_s> <line>"). The board prints every
advertisement ("ALL"), so a capture shows everything nearby, not only what the board's filter expects; Smart Light
adverts are decoded on the board itself (its FAN lines). At the end it sends STATS, then prints the decoded FAN,
LEARNED and SENT lines, the Smart Light packets grouped into bursts (one burst per button press), and the path.
"""

from __future__ import annotations

import pathlib
import sys
import time
from typing import TextIO

import board

ROOT = pathlib.Path(__file__).resolve().parent.parent


def bursts(times: list[float], gap: float = 1.0) -> list[tuple[float, float, int]]:
    """Group sorted times into (start, end, count) runs separated by more than `gap` seconds."""
    out: list[tuple[float, float, int]] = []
    for t in times:
        if out and t - out[-1][1] <= gap:
            start, _, n = out[-1]
            out[-1] = (start, t, n + 1)
        else:
            out.append((t, t, 1))
    return out


class Recorder:
    """Splits the board's output into lines, timestamps them and writes them to the capture file."""

    def __init__(self, s, f: TextIO, t0: float) -> None:
        self.s, self.f, self.t0 = s, f, t0
        self.pending = b""
        self.ag_times: list[float] = []
        self.notable: list[str] = []

    def _line(self, now: float, raw: bytes) -> None:
        line = raw.decode("utf-8", "replace").rstrip("\r")
        self.f.write(f"{now:.3f} {line}\n")
        if line.startswith("ADV ") and " AG " in line:
            self.ag_times.append(now)
        elif line.split(" ", 1)[0] in ("FAN", "LEARNED", "SENT", "STATS", "STATE", "ERR", "OK", "KEY", "PAIRING", "PAIR"):
            self.notable.append(f"{now:7.2f}s {line}")

    def pump(self, seconds: float) -> None:
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            n = self.s.in_waiting
            if not n:
                time.sleep(0.01)
                continue
            self.pending += self.s.read(n)
            *done, self.pending = self.pending.split(b"\n")
            now = time.monotonic() - self.t0
            for raw in done:
                self._line(now, raw)

    def flush(self) -> None:
        if self.pending:
            self._line(time.monotonic() - self.t0, self.pending)
            self.pending = b""


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    port, seconds, commands = sys.argv[1], float(sys.argv[2]), sys.argv[3:]
    out_dir = ROOT / "captures"
    out_dir.mkdir(exist_ok=True)
    path = out_dir / time.strftime("capture-%Y%m%d-%H%M%S.log")
    s = board.open_port(port)
    with path.open("w", encoding="utf-8") as f:
        rec = Recorder(s, f, time.monotonic())
        s.write(b"ALL\n")
        for cmd in commands:
            rec.pump(1.0)
            s.write(cmd.encode("ascii") + b"\n")
        rec.pump(seconds)
        s.write(b"STATS\n")
        rec.pump(0.5)
        rec.flush()
    s.close()
    print(f"capture: {path}")
    print(*rec.notable, sep="\n")
    groups = bursts(rec.ag_times)
    print(f"Smart Light adverts: {len(rec.ag_times)} in {len(groups)} burst(s)")
    for start, end, count in groups:
        print(f"  {start:7.2f}s - {end:7.2f}s  {count} adverts")
    return 0


if __name__ == "__main__":
    sys.exit(main())
