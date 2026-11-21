// The board's link to the fan: decodes Smart Light packets as the scanner hears them, keeps the key (an ID and
// index, plus its counters, in NVS; the ID is never printed in full) and sends commands with it.
// The key is either the board's own (NEWKEY, then PAIR while the fan is in its pairing window after power-up), or
// learned from the app (LEARN), which needs the app to send something.
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "fancmd.h"
#include "fanstate.h"

namespace fanlink {

struct Status {
  bool haveKey;
  bool learning;
  uint32_t pairingLeftMs;  // > 0 while the pair command is on the air
  uint32_t appCommands;    // distinct Smart Light commands decoded since boot
  char last[64];           // the last command heard or sent, described
  bool onAir;              // a command is being broadcast now (the screen shows "Sending…")
};

void begin();                                                       // load the key and the fan state from NVS
void onAdv(const uint8_t* adv, size_t len, uint32_t ms, int rssi);  // every Smart Light advertisement, from loop()
void tick();                                                        // from loop(): ends a pairing broadcast on time

void newKey();               // make the board's own key (refused if one exists)
void setPrefix(uint8_t prefix);  // the key's packet version: 0x83 (v3) or 0x84 (v4)
void setDuration(uint32_t ms);  // how long SEND advertises each command: 200-5000 ms (default 3000, as the app)
void setField(const char* args);  // "REM1 <hex>" | "RESTART <n>" | "INDEX <n>" (group<<4 | device): key fields
void learn();                // adopt the ID of the next app command heard
void forget();               // erase the key
void printKey();             // masked
void printState();           // what the board last sent: "STATE light=on br=.. ct=.. fan=.. speed=.. dir=.."
void pair(uint32_t seconds); // 1 = one pair command; up to 60 s = a fresh one every 2 s
void stop();                 // stop whatever is on the air
void raw(const char* args);  // "RAW <ms 20-2000> <hex>": broadcast one payload made off the board
// Send one command, in fancmd::parse's words ("LIGHT ON", "BRIGHT 40", "FAN 3", "FAN REV", ...). Prints SENT or ERR.
bool send(const char* what);  // false if nothing was sent (a parse error, too soon, no key)
// The same from the screen: no one-second gap (a newer tap replaces the command on the air). False if not sent.
bool send(fancmd::Action action, int value);
// What the board last sent (the fan never reports its state): the screen's read model.
const fanstate::State& state();
// Keep `s` as that state without sending anything (a setting changed while its device is off: the next "on"
// carries it). Saved to NVS like a send.
void remember(const fanstate::State& s);
// "GROUP <attr> <p0> <p1> <p2> <last>" (hex): the app's group command, which reaches every device paired to a group.
void sendGroup(const char* args);
Status status();

}  // namespace fanlink
