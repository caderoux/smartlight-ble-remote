// follow: telling our own relays from other controllers' commands, and what each one changes. pio test -e native
#include <unity.h>

#include "fancmd.h"
#include "fanstate.h"
#include "follow.h"

namespace {

agarce::Packet command(uint32_t sequence, uint16_t seed) {
  agarce::Packet p;
  p.tx = (uint8_t)sequence;
  p.restart = (uint8_t)(sequence >> 8);
  p.rem0 = (uint8_t)(sequence >> 16);
  p.rem1 = (uint8_t)(0x10 | ((sequence >> 24) & 0x0F));
  p.seed = seed;
  return p;
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_recent_commands_know_a_command_by_number_and_seed() {
  follow::RecentCommands r;
  TEST_ASSERT_FALSE(r.contains(command(56, 0x1ff1)));
  TEST_ASSERT_FALSE(r.contains(command(0, 0)));  // an empty slot matches nothing, not even sequence 0 with seed 0
  r.add(command(56, 0x1ff1));
  TEST_ASSERT_TRUE(r.contains(command(56, 0x1ff1)));   // a repeat, or the receiver's relay
  TEST_ASSERT_FALSE(r.contains(command(56, 0x2000)));  // the same number from another controller: a different command
  TEST_ASSERT_FALSE(r.contains(command(57, 0x1ff1)));
  agarce::Packet relay = command(56, 0x1ff1);
  relay.prefix = 0x83;  // relayed as v3, with other fields from the receiver: still the same command
  TEST_ASSERT_TRUE(r.contains(relay));
}

void test_recent_commands_find_the_command_we_sent_with_a_number() {
  follow::RecentCommands r;
  TEST_ASSERT_NULL(r.withSequence(56));
  TEST_ASSERT_NULL(r.withSequence(0));  // empty slots aren't sequence 0
  agarce::Packet p = command(56, 0x1ff1);
  p.cmd = 0x10;
  p.arg0 = 1;
  r.add(p);
  const agarce::Packet* found = r.withSequence(56);
  TEST_ASSERT_NOT_NULL(found);
  TEST_ASSERT_EQUAL_HEX16(0x1ff1, found->seed);
  TEST_ASSERT_EQUAL_HEX8(0x10, found->cmd);  // the whole command is kept, to re-apply it
  TEST_ASSERT_NULL(r.withSequence(57));
}

void test_recent_commands_remember_the_last_sixteen() {
  follow::RecentCommands r;
  const uint32_t n = follow::RecentCommands::kSize;
  for (uint32_t i = 0; i < n; i++) r.add(command(100 + i, (uint16_t)(1 + i)));
  for (uint32_t i = 0; i < n; i++) TEST_ASSERT_TRUE(r.contains(command(100 + i, (uint16_t)(1 + i))));
  r.add(command(200, 0x77));  // one more pushes out the first
  TEST_ASSERT_FALSE(r.contains(command(100, 1)));
  TEST_ASSERT_TRUE(r.contains(command(101, 2)));
  TEST_ASSERT_TRUE(r.contains(command(200, 0x77)));
  TEST_ASSERT_EQUAL(16, (int)n);
}

void test_decide_takes_a_newer_command_and_its_counter() {
  agarce::Packet ours = command(55, 0x1234);
  TEST_ASSERT_EQUAL(follow::Action::StateAndCounter, follow::decide(command(56, 9), command(55, 0), nullptr));
  TEST_ASSERT_EQUAL(follow::Action::StateAndCounter, follow::decide(command(56, 9), command(55, 0), &ours));
  TEST_ASSERT_EQUAL(follow::Action::StateAndCounter, follow::decide(command(0, 9), command(0x0FFFFFFF, 0), nullptr));
}

void test_decide_two_controllers_that_sent_at_once() {
  // Both stood at 55 and sent 56 at once: the CYD with seed 0x1000, the Atom with 0x2000. Each hears the other's.
  agarce::Packet cyds = command(56, 0x1000), atoms = command(56, 0x2000);
  TEST_ASSERT_EQUAL(follow::Action::State, follow::decide(atoms, cyds, &cyds));          // the CYD: Atom's last
  TEST_ASSERT_EQUAL(follow::Action::StateThenOurs, follow::decide(cyds, atoms, &atoms));  // the Atom: its own last
  // We hold that number without having sent it (we took it from someone): take the state
  TEST_ASSERT_EQUAL(follow::Action::State, follow::decide(command(56, 0x0500), command(56, 0), nullptr));
}

// Apply what decide() says to a board's state, as fanlink does
void apply(fanstate::State& s, const agarce::Packet& heard, const agarce::Packet& key, const agarce::Packet* ours) {
  follow::Action a = follow::decide(heard, key, ours);
  if (a == follow::Action::Ignore) return;
  fanstate::observe(heard, s);
  if (a == follow::Action::StateThenOurs) fanstate::observe(*ours, s);
}

agarce::Packet built(fancmd::Action action, int value, const fanstate::State& s, uint32_t sequence, uint16_t seed) {
  agarce::Packet p = command(sequence, seed);
  TEST_ASSERT_TRUE(fancmd::build(action, value, s, p));
  return p;
}

void test_two_boards_that_sent_at_once_end_with_the_same_state() {
  fanstate::State start;  // light on, fan off
  for (int highIsAtom = 0; highIsAtom < 2; highIsAtom++) {
    uint16_t cydSeed = highIsAtom ? 0x1000 : 0x2000, atomSeed = highIsAtom ? 0x2000 : 0x1000;
    // Different things: the CYD starts the fan, the Atom turns the light off. Both must end with both changes.
    fanstate::State cyd = start, atom = start;
    agarce::Packet cydFan = built(fancmd::Action::FanSpeed, 3, cyd, 56, cydSeed);
    agarce::Packet atomLight = built(fancmd::Action::LightOff, 0, atom, 56, atomSeed);
    fanstate::observe(cydFan, cyd);
    fanstate::observe(atomLight, atom);
    apply(cyd, atomLight, cydFan, &cydFan);
    apply(atom, cydFan, atomLight, &atomLight);
    TEST_ASSERT_TRUE(cyd.fanOn && atom.fanOn);
    TEST_ASSERT_FALSE(cyd.lightOn || atom.lightOn);
    // The same thing: the CYD turns the light off, the Atom sets brightness 40. The higher seed's command wins on both.
    fanstate::State cyd2 = start, atom2 = start;
    agarce::Packet cydOff = built(fancmd::Action::LightOff, 0, cyd2, 57, cydSeed);
    agarce::Packet atomBr = built(fancmd::Action::Brightness, 40, atom2, 57, atomSeed);
    fanstate::observe(cydOff, cyd2);
    fanstate::observe(atomBr, atom2);
    apply(cyd2, atomBr, cydOff, &cydOff);
    apply(atom2, cydOff, atomBr, &atomBr);
    TEST_ASSERT_EQUAL(cyd2.lightOn, atom2.lightOn);
    TEST_ASSERT_EQUAL(cyd2.br, atom2.br);
  }
}

void test_decide_never_rolls_back_to_an_older_command() {
  agarce::Packet ours = command(53, 0x4444);
  // A relay of a command we missed, a late copy, the app reusing a number, a stale controller: all older
  TEST_ASSERT_EQUAL(follow::Action::Ignore, follow::decide(command(56, 9), command(57, 0), nullptr));
  TEST_ASSERT_EQUAL(follow::Action::Ignore, follow::decide(command(51, 9), command(53, 0), &ours));
  TEST_ASSERT_EQUAL(follow::Action::Ignore, follow::decide(command(0x0FFFFFFF, 9), command(0, 0), nullptr));  // wrap
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_recent_commands_know_a_command_by_number_and_seed);
  RUN_TEST(test_recent_commands_find_the_command_we_sent_with_a_number);
  RUN_TEST(test_recent_commands_remember_the_last_sixteen);
  RUN_TEST(test_decide_takes_a_newer_command_and_its_counter);
  RUN_TEST(test_decide_two_controllers_that_sent_at_once);
  RUN_TEST(test_two_boards_that_sent_at_once_end_with_the_same_state);
  RUN_TEST(test_decide_never_rolls_back_to_an_older_command);
  return UNITY_END();
}
