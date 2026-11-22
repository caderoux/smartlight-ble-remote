// Following other controllers that share our ID: another board that learned ours (or that ours was learned from), or
// the app. Every command they send with the ID is heard by us too, along with the receiver's relays of our own
// commands. This decides which packets are which, and what each one changes. Pure logic, tested on the PC.
#pragma once
#include <stdint.h>

#include "agarce.h"

namespace follow {

// The last few commands, each known by its sequence number and seed (one command = one pair, however many copies the
// sender advertises; the receiver's relay has the same pair). The whole packet is kept, so a board can re-apply its
// own command. Seeds are never 0, so an empty slot matches nothing.
class RecentCommands {
 public:
  bool contains(const agarce::Packet& p) const;
  void add(const agarce::Packet& p);
  // The remembered command with this sequence number, or nullptr
  const agarce::Packet* withSequence(uint32_t sequence) const;

  // A command repeats for 3 s and its relay comes within ~1 s; neighbours' remotes add their own commands
  static constexpr int kSize = 16;

 private:
  agarce::Packet entries_[kSize] = {};
  int next_ = 0;
};

enum class Action : uint8_t {
  Ignore,           // nothing changes
  State,            // take its state (fanstate::observe); the counter stays
  StateThenOurs,    // take its state, then re-apply our own command with the same number (ours wins where they overlap)
  StateAndCounter,  // take its state and its counter; a command of ours still on the air should stop (newest wins)
};

// What a command with our ID changes, when it's heard for the first time and isn't one of our own sends. `key` holds
// our counter (its sequence number: the last we sent or took). `ours` = the command we sent with that same number,
// or nullptr if we didn't send it.
//   newer than ours  -> StateAndCounter
//   the same number  -> two controllers sent at once. Both commands are applied, the higher seed's last, so both
//                       boards end with the same state: State if its seed is higher (or we didn't send that number),
//                       StateThenOurs if ours is.
//   older            -> Ignore: a late copy, a relay of a command we missed, or a stale controller. The state never
//                       rolls back. (So the app's commands, which reuse numbers it doesn't know a board used, aren't
//                       followed once the board has sent with the ID.)
// The counter only ever moves forward.
Action decide(const agarce::Packet& heard, const agarce::Packet& key, const agarce::Packet* ours);

}  // namespace follow
