# Pairing the remote with your fan-light

Pair once. The board keeps its key (its controller ID and counters) in NVS through reboots and reflashing, and the
fan-light keeps the pairing through power cycles.

**You need:** the board on USB to a PC, with a serial terminal at **921600 baud** (`pio device monitor` from
`examples/cyd-remote`, or `python tools/capture.py <port> <seconds> "<command>" ...`), and the fan-light's **wall
switch or breaker** within reach.

1. **Make the board's own key:** send `NEWKEY`. It answers with the key's fingerprint, a `#` and four hex digits, and its index, 17.
   The fingerprint is keyed with a random number kept on the board, so it doesn't give the ID away, but there's no
   need to share it.
2. **Switch the fan-light off** at the wall or breaker, and wait a few seconds.
3. **Start the pairing broadcast:** send `PAIR 60`. For 60 seconds it sends a fresh pair command every 2 s.
4. **Switch the fan-light on** within those 60 seconds.
5. **Watch the light: two flashes = paired.** It may flash twice more as it catches a second pair command. With no
   flashes, wait for `PAIR done`, then go back to step 2.
6. **Check it:** tap LIGHT on the screen, or send `SEND LIGHT OFF`, then `SEND LIGHT ON`.

Notes:
- **Leave the wall switch on afterwards.** The remote can't control a fan-light whose power is off.
- **The phone app probably keeps working**, if it was paired: the receiver also took a handheld remote alongside the
  app. Not verified here (this fan's app could no longer sign in).
- `KEY` shows the key's fingerprint; `FORGET` erases the board's key (the fan-light is not told: there's no unpair).
- **The fan-light doesn't report its state**, so the screen shows what the board last sent. If someone uses the phone
  app or the wall switch, the screen can be out of date until the next tap.
