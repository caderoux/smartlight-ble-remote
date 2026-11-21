#include "radio.h"

#include <NimBLEDevice.h>

namespace radio {
namespace {
constexpr uint16_t kIntervalUnits = 32;  // 32 x 0.625 ms = 20 ms
}

bool putBytesOnAir(const uint8_t* adv, size_t len, uint32_t ms) {
  NimBLEAdvertising* a = NimBLEDevice::getAdvertising();
  if (a->isAdvertising()) a->stop();
  NimBLEAdvertisementData data;
  data.addData(adv, len);
  a->setAdvertisementData(data);
  a->setConnectableMode(BLE_GAP_CONN_MODE_UND);  // connectable, as the app advertises
  a->setMinInterval(kIntervalUnits);
  a->setMaxInterval(kIntervalUnits);
  return a->start(ms);  // stops by itself after ms
}

bool putOnAir(const agarce::Packet& p, uint32_t ms) {
  uint8_t adv[31];
  size_t len = agarce::encodeAdv(p, adv, sizeof(adv));
  if (len == 0) return false;
  return putBytesOnAir(adv, len, ms);
}

void stop() {
  NimBLEAdvertising* a = NimBLEDevice::getAdvertising();
  if (a->isAdvertising()) a->stop();
}

bool onAir() { return NimBLEDevice::getAdvertising()->isAdvertising(); }

}  // namespace radio
