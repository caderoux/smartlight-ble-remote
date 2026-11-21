# The Smart Light ("agarce") protocol

How Smart Light / Argrace fan-lights are controlled, as this library implements it. The starting point was
[ha-ble-adv](https://github.com/NicoIIT/ha-ble-adv)'s `agarce` codec. The corrections and additions below come from
reading the Android Phone App (`ai.argrace.remotecontrol` 1.5.7) for interoperability, and every one marked
**verified** was tested on a real fan-light ([this one](https://www.amazon.com/dp/B091CTF4T8): a flush-mount
fan-light with a CCT LED panel and a 6-speed reversible fan).

There is no connection and no GATT. The phone, or this remote, **broadcasts BLE advertisements**, and the receiver
in the fan's canopy acts on the ones carrying its controller's ID.

## Sending
- **What this remote does, which works every time on the tested fan-light:** each command advertised for **3
  seconds**, connectable (AD flags `0x1A`), with the device type the app uses for that command (byte 0, below).
  3 s is the app's **maximum**; it uses 5 s on Android 8 to 12. A newer command replaces the one on the air.
- An earlier version sent 0.4 to 1 s, non-connectable, with type 8 for every command, and the fan-light obeyed only
  now and then. **Those three things changed together**, so which of them mattered isn't known.
- ha-ble-adv's author [notes](https://github.com/NicoIIT/ha-ble-adv/discussions/377) that the app sends both a
  connectable and a non-connectable frame, that switching between them hasn't helped users, and that one short burst
  is usually enough unless the signal is weak. That points at the duration or the type, not at connectable.

## The advertisement
Manufacturer-specific data, company ID `0x09F9` (`F9 09`), then 18 bytes:

| Bytes | Field |
|---|---|
| 0 | `(version & 15) \| (type << 4)`. **type** is the device category the command is for: 8 = fan light, 0 = light switch and pairing, 5 = bulb, 13 = floor fan. ha-ble-adv's "v3 / v4" are versions 3 and 4 (no difference in content has been found between them), and its "pair form" (`0x03` / `0x04`) is type 0. The app sends its light group switch and pairing as type 0, and everything else to a fan-light as type 8, light commands included. |
| 1-2 | the **seed**, little-endian, sent in the clear |
| 3-16 | scrambled (below): a 28-bit **sequence number** plus an **order** nibble (4 bytes), the **controller ID** (4 bytes), the **command** (5 bytes), an inner checksum |
| 17 | an outer checksum |

**Scrambling:** the 14 body bytes (3-16, the inner checksum included) are XORed with the seed pivot `[lo, hi, hi, lo]`
and the key `AA BB CC DD 5A A5 A5 5A`. The outer checksum (byte 17) is sent in the clear. The code is in
`src/agarce.cpp`.

**The sequence:** ha-ble-adv's "tx, restart, `00 10`" is one 28-bit little-endian counter, plus the app's "order"
number in the top nibble of the last byte (`0x10` = order 1, `0x50` = order 5). It must keep increasing. When tx wraps
from 255 to 0, "restart" (its next byte) goes up by one, and the receiver keeps obeying (**verified**).
`agarce::nextSequence` carries through all 28 bits and keeps the order nibble, and the example stores the counter in
NVS, so it never goes backwards, even across a reboot.

## Commands (5 bytes)
**Single device** (type 8): `[attr | device & 15, p0, p1, p2, group & 15 | (device << 2) & 0xC0]`. The device ID is
split between the first and last bytes.

| attr | Command | Params | Verified |
|---|---|---|---|
| `0x10` | light | `[on, ct, br]` (on 0/1; ct 0 warm to 100 cool; br 1-100) | ✅ on/off |
| `0x20` | light colour and brightness | `[ct, br, 0]` | ✅ |
| `0x80` | fan | `[on << 7 \| reverse 0x10 \| speed 1-6, 0, changed]`, where changed is a mask: speed 0x01, direction 0x02, preset 0x04, on 0x08 | ✅ speeds 1-6, direction, off |
| `0x70` | all | `[1, 0, 100]` = all off | |

**Every command carries the whole state**, as the app's do: a fan command carries the direction even when only the
speed changed. The receiver never reports its state, so a controller has to remember what it last sent
(`src/fanstate.*`).

**Group** (no device ID): `[attr, p0, p1, p2, group | fan_on << 6 | light_on << 7]`. With attr `0x07` (a group
switch), p2 picks the target: `0x01` fan (type 8), `0x02` light (type 0). Both carry the whole on/off state in the last
byte. ✅

## Pairing (verified)
The pair command is attr 0, p0 1 (`fancmd::buildPair`). The receiver accepts it only in a **short window after power
is switched on**, and **acknowledges with two light flashes**: a plain power-up gives none. Pairing survives power
cycles. The pair command carries the index (group << 4 | device), and the example pairs as **index 17, group 1,
device 1**, as the app's own commands use groups of 1 or more. After pairing at index 17, the fan-light obeyed
single-device commands to group 1, device 1. Pairing at group 0 is untested.

What works: a fresh 16-bit controller ID (0xFF-0xFFF5, as ha-ble-adv draws it), and **a fresh pair command every
2 s** (new sequence number and seed), alternating version 4 and version 3, while the fan's power is switched on. One
unchanged packet repeated across the power-up didn't pair, and a full 32-bit ID was ignored. Steps:
`docs/pairing.md`.

There is deliberately **no unpair** in this library. The codec refuses to build one.

## The receiver relays
It re-broadcasts the commands it hears as version 3, and pair commands also unscrambled. That's redundancy, and maybe
range: a listener sees every command many times anyway, since advertising repeats it for its whole duration. The relay
carries the same controller ID, and pairing goes by that ID, so learning an ID from a relayed copy works too.

## Not yet tested
The night light, the 2-hour timer, the breeze preset, and whether the receiver ever rejects a sequence number.
