#include "store.h"

#include <Preferences.h>

namespace store {

Key loadKey() {
  Key k;
  Preferences p;
  p.begin("fan", true);
  k.valid = p.getBool("valid", false);
  k.own = p.getBool("own", false);
  k.id = p.getUInt("id", 0);
  k.index = p.getUChar("index", 0);
  k.prefix = p.getUChar("prefix", 0x84);
  k.rem0 = p.getUChar("rem0", 0x00);
  k.rem1 = p.getUChar("rem1", 0x10);
  k.tx = p.getUChar("tx", 0);
  k.restart = p.getUChar("restart", 0);
  p.end();
  return k;
}

void saveKey(const Key& k) {
  Preferences p;
  p.begin("fan", false);
  p.putBool("valid", k.valid);
  p.putBool("own", k.own);
  p.putUInt("id", k.id);
  p.putUChar("index", k.index);
  p.putUChar("prefix", k.prefix);
  p.putUChar("rem0", k.rem0);
  p.putUChar("rem1", k.rem1);
  p.putUChar("tx", k.tx);
  p.putUChar("restart", k.restart);
  p.end();
}

fanstate::State loadState() {
  fanstate::State s;  // defaults where nothing is stored yet
  Preferences p;
  p.begin("fanstate", true);
  s.lightOn = p.getBool("light", s.lightOn);
  s.br = p.getUChar("br", s.br);
  s.ct = p.getUChar("ct", s.ct);
  s.fanOn = p.getBool("fan", s.fanOn);
  s.speed = p.getUChar("speed", s.speed);
  s.reverse = p.getBool("rev", s.reverse);
  s.breeze = p.getBool("breeze", s.breeze);
  p.end();
  return s;
}

void saveState(const fanstate::State& s) {
  Preferences p;
  p.begin("fanstate", false);
  p.putBool("light", s.lightOn);
  p.putUChar("br", s.br);
  p.putUChar("ct", s.ct);
  p.putBool("fan", s.fanOn);
  p.putUChar("speed", s.speed);
  p.putBool("rev", s.reverse);
  p.putBool("breeze", s.breeze);
  p.end();
}

uint32_t fingerprintSalt() {
  Preferences p;
  p.begin("fp", false);
  uint32_t salt = p.getUInt("salt", 0);
  if (salt == 0) {
    while (salt == 0) salt = esp_random();
    p.putUInt("salt", salt);
  }
  p.end();
  return salt;
}

std::string loadCalibration() {
  Preferences p;
  p.begin("touch", true);
  String text = p.getString("cal", "");
  p.end();
  return std::string(text.c_str());
}

void saveCalibration(const std::string& text) {
  Preferences p;
  p.begin("touch", false);
  p.putString("cal", text.c_str());
  p.end();
}

}  // namespace store
