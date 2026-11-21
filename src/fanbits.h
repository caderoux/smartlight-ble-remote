// The fan-light's command bytes (single-device commands, type 8), from the vendor app's code and ha-ble-adv's
// agarce translators. Shared by building, state tracking and describing. Pure constants.
#pragma once
#include <stdint.h>

namespace fanbits {

// Commands (the high nibble of the command byte; the low nibble carries the device ID)
constexpr uint8_t kLight = 0x10;     // [on, ct, br]
constexpr uint8_t kLightCtBr = 0x20; // [ct, br, 0]: colour temperature (0 warm .. 100 cold) and brightness (1-100)
constexpr uint8_t kAll = 0x70;       // [<=1 off / >=2 on, 0, 100]
constexpr uint8_t kFan = 0x80;       // [on | reverse | breeze | speed, oscillation, changed mask]

// Fan arg0
constexpr uint8_t kFanOn = 0x80, kFanReverse = 0x10, kFanBreeze = 0x20, kFanSpeedMask = 0x0F;
// Fan arg2: which fields changed
constexpr uint8_t kChgSpeed = 0x01, kChgDir = 0x02, kChgPreset = 0x04, kChgOn = 0x08, kChgOsc = 0x10;

// Group commands (no device ID): the group switch, attr 7, [0, 0, target]; last byte = group | fan<<6 | light<<7
constexpr uint8_t kGroupSwitch = 0x07;
constexpr uint8_t kSwitchFan = 0x01, kSwitchLight = 0x02;
constexpr uint8_t kGroupFanOn = 0x40, kGroupLightOn = 0x80;

}  // namespace fanbits
