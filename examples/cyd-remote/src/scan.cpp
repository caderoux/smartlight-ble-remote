#include "scan.h"

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <freertos/queue.h>
#include <string.h>

#include "agarce.h"
#include "fanlink.h"

namespace scan {
namespace {

struct Adv {
  uint32_t ms;
  uint8_t addr[6];
  int8_t rssi;
  uint8_t len;
  uint8_t data[31];  // legacy advertising payload (BLE 4.2): at most 31 bytes
};

QueueHandle_t queue = nullptr;
volatile uint32_t heard = 0, agarce = 0, dropped = 0;
volatile bool onlyAgarce = false;

class Callbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* dev) override {
    heard = heard + 1;
    const std::vector<uint8_t>& payload = dev->getPayload();
    Adv a;
    a.len = payload.size() > sizeof(a.data) ? sizeof(a.data) : payload.size();
    memcpy(a.data, payload.data(), a.len);
    bool ag = agarce::isAgarceAdv(a.data, a.len);
    if (ag) agarce = agarce + 1;
    if (onlyAgarce && !ag) return;
    a.ms = millis();
    a.rssi = dev->getRSSI();
    memcpy(a.addr, dev->getAddress().getVal(), 6);
    if (xQueueSend(queue, &a, 0) != pdTRUE) dropped = dropped + 1;
  }
};

Callbacks callbacks;

}  // namespace

void begin() {
  queue = xQueueCreate(64, sizeof(Adv));
  NimBLEDevice::init("");
  NimBLEScan* s = NimBLEDevice::getScan();
  s->setScanCallbacks(&callbacks, true);  // true: report duplicates (each repeat of a command is evidence)
  s->setActiveScan(false);                // passive: listen only, send nothing
  s->setInterval(100);
  s->setWindow(100);                      // window = interval: listen all the time
  s->setMaxResults(0);                    // don't keep results in memory; the callback has them
  s->start(0, false, true);               // 0 = scan forever
}

void setOnlyAgarce(bool only) { onlyAgarce = only; }

void drain() {
  Adv a;
  while (queue != nullptr && xQueueReceive(queue, &a, 0) == pdTRUE) {
    bool ag = agarce::isAgarceAdv(a.data, a.len);
    // NimBLE stores the address little-endian; print it the usual way (most significant byte first)
    char line[160];
    int n = snprintf(line, sizeof(line), "ADV %lu %02x:%02x:%02x:%02x:%02x:%02x %d %s ", (unsigned long)a.ms,
                     a.addr[5], a.addr[4], a.addr[3], a.addr[2], a.addr[1], a.addr[0], a.rssi,
                     ag ? "AG" : "--");
    // A Smart Light advert's scrambled body holds the app's ID behind a fixed XOR, so print only up to its prefix
    // (flags, length, FF, F9 09, prefix) and mask the rest; the FAN line that follows carries the decoded fields.
    uint8_t shown = ag ? (a.len < 8 ? a.len : 8) : a.len;
    for (uint8_t i = 0; i < shown && n < (int)sizeof(line) - 3; i++) n += snprintf(line + n, 3, "%02x", a.data[i]);
    if (ag) snprintf(line + n, sizeof(line) - n, "+%ubytes", (unsigned)(a.len - shown));
    Serial.println(line);
    if (ag) fanlink::onAdv(a.data, a.len, a.ms, a.rssi);
  }
}

Stats stats() { return Stats{heard, agarce, dropped}; }

}  // namespace scan
