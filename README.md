# smartlight-ble-remote

A touchscreen remote for **Smart Light / Argrace** ceiling fan-lights, the ones run by the Android app
`ai.argrace.remotecontrol` (protocol "agarce"). It runs on a cheap **ESP32 2.8" touch display**, and it doesn't need
the phone, the app's cloud, Home Assistant or WiFi. It pairs with the fan-light as its own controller, and sends
commands the way the app does, as Bluetooth LE advertisements.

- **Main screen:** LIGHT on/off and FAN on/off, big enough to hit without looking. Each turns on with the last
  settings.
- **Settings:** brightness and colour temperature in 10% steps, fan speeds 1-6, and forward/reverse. A change made
  while the light or fan is off is saved for the next "on", not sent.
- **A library** (`SmartLightBLE`, pure C++, tested on the PC) with the packet codec, the commands and the state, for
  building your own controller.

What's known about the protocol, including where it differs from earlier write-ups: [`docs/protocol.md`](docs/protocol.md).

## Hardware
- **The ESP32-32E "Cheap Yellow Display"** (2.8", 320×240, ILI9341 display, XPT2046 resistive touch), e.g. the ELEGOO
  2.8" ESP32 display, or LCDwiki E32R28T. **Only this variant has been tested.** Other CYD variants (such as the
  original ESP32-2432S028R) may need different `TFT_*` flags in `examples/cyd-remote/platformio.ini`.
- Any **USB charger** to run it.
- A fan-light whose receiver speaks Smart Light / agarce. If the app is "Smart Light" by Argrace, it probably does.
  Tested on [this one](https://www.amazon.com/dp/B091CTF4T8): CCT light, 6-speed reversible fan.

## Build and flash
You need [PlatformIO](https://platformio.org/) (`pip install platformio`, or the VS Code extension).

```sh
git clone https://github.com/caderoux/smartlight-ble-remote.git
cd smartlight-ble-remote/examples/cyd-remote
pio run -t upload --upload-port <port>     # e.g. COM5 or /dev/ttyUSB0
```

The title on the main screen is a build setting: edit `'-DREMOTE_TITLE="Fan"'` in `examples/cyd-remote/platformio.ini`.

## First boot
1. **Touch setup:** the screen shows five crosses, one at a time. Tap each one firmly. It asks again if a tap is off,
   then saves the calibration.
2. **"Not paired":** the board has no key for a fan-light yet. Pair it: [`docs/pairing.md`](docs/pairing.md). It
   takes a minute and a trip to the wall switch.

After that it boots straight to the main screen. The key, the last state and the calibration live in NVS, so they
survive power cuts and reflashing.

## Using it
- **Tap LIGHT or FAN** to switch it. The top line shows `Sending: …` for the 3 seconds a command is on the air (less if
  another controller sends a newer one: the newest wins), then `Sent: …`.
- **Settings ›** opens the settings screen. **Back** returns to the main screen, and so does 30 seconds without a tap.
- **The fan-light never reports its state.** The screen shows what the remote last sent, or last heard from another
  controller that uses the same ID, such as a second board that learned this one's: those commands update the state,
  and the buttons with it. The wall switch, and anything the board doesn't hear, aren't seen until the next command.
  The phone app is followed only until the board sends: the app reuses sequence numbers it doesn't know the board
  used, and a board never follows an older number (`docs/protocol.md`).

## Serial console (921600 baud)
For pairing, diagnostics and tests. One command per line:

| Command | Does |
|---|---|
| `NEWKEY`, `PAIR <1-60>`, `KEY`, `FORGET` | make the board's key, broadcast the pair command, show the key's fingerprint, erase the key |
| `SEND LIGHT ON\|OFF`, `SEND BRIGHT <1-100>`, `SEND CT <0-100>` | the light (CT 0 = warm, 100 = cool) |
| `SEND FAN <1-6>`, `SEND FAN OFF`, `SEND FAN FWD\|REV`, `SEND ALL OFF` | the fan |
| `STATE`, `HEAP`, `STATS` | the state last sent, free memory, adverts heard |
| `TAP <x> <y>`, `CALIBRATE`, `SKIP` | tap the screen at pixels; redo the touch setup; cancel it |
| `ONLY AG`, `ALL` | print only Smart Light adverts, or every advert heard |
| `LEARN`, `PREFIX`, `RAW`, `DURATION`, `SET`, `GROUP`, `STOP` | protocol tools (see the top of `examples/cyd-remote/src/main.cpp`) |

`tools/capture.py <port> <seconds> ["COMMAND" …]` records everything the board prints to `captures/`, without resetting
the board, sending the commands as it goes. `tools/board.py` is the small serial helper it uses.

## The library
In your own PlatformIO project:

```ini
lib_deps = SmartLightBLE=https://github.com/caderoux/smartlight-ble-remote.git#v0.1.2
```

- `agarce.h`: decode and encode advertisements (`decodeAdv`, `encodeAdv`, `Packet`); the sequence number
  (`sequence`, `nextSequence`, and `newer`, for controllers that share an ID).
- `fancmd.h`: build a command from an action and the current state (`build`, `buildPair`, `parse`).
- `fanstate.h`: track what was sent (`observe`).
- `follow.h`: follow other controllers that share the ID: tell the receiver's relays of our own commands from theirs
  (`RecentCommands`), and decide what each changes (`decide`).
- `describe.h`: a packet in words, for logs.
- `remote.h`, `pt_touch.h`: the touchscreen remote's layout model (and `change`: the words for a command another
  controller sent), and the touch maths.

Tests run on the PC: `pio test -e native` from the repo root. They need a C++17 compiler on the PATH.

## Limits
- One fan-light per board (group 1, device 1 after pairing).
- No timer or night light yet (the protocol has them; they're untested).
- There's no unpair, on purpose.
- Only one board has been paired with one fan-light. A second board should pair the same way, but that's untested.

## Credits and licence
MIT; see [`LICENSE`](LICENSE). The codec is a C++ port of [ha-ble-adv](https://github.com/NicoIIT/ha-ble-adv)'s
`agarce` codec (MIT, © @NicoIIT), with its test vectors; see [`NOTICE.md`](NOTICE.md). The findings were shared there
in [a ha-ble-adv Discussion](https://github.com/NicoIIT/ha-ble-adv/discussions/377).

Not affiliated with Argrace or the fan-light's maker.
