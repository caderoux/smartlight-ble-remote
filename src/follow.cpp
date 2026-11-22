#include "follow.h"

namespace follow {

bool RecentCommands::contains(const agarce::Packet& p) const {
  for (const agarce::Packet& e : entries_) {
    if (e.seed != 0 && e.seed == p.seed && agarce::sequence(e) == agarce::sequence(p)) return true;
  }
  return false;
}

void RecentCommands::add(const agarce::Packet& p) {
  entries_[next_] = p;
  next_ = (next_ + 1) % kSize;
}

const agarce::Packet* RecentCommands::withSequence(uint32_t sequence) const {
  for (const agarce::Packet& e : entries_) {
    if (e.seed != 0 && agarce::sequence(e) == sequence) return &e;
  }
  return nullptr;
}

Action decide(const agarce::Packet& heard, const agarce::Packet& key, const agarce::Packet* ours) {
  if (agarce::newer(heard, key)) return Action::StateAndCounter;
  if (agarce::sequence(heard) != agarce::sequence(key)) return Action::Ignore;  // older
  return ours == nullptr || heard.seed > ours->seed ? Action::State : Action::StateThenOurs;
}

}  // namespace follow
