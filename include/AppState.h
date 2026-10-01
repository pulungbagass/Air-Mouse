#pragma once
#include <Arduino.h>

enum class OperationMode : uint8_t {
    MODE_1_NAVIGATION = 1,
    MODE_2_MEDIA      = 2
};

enum class FingerID : uint8_t {
    INDEX  = 0,
    MIDDLE = 1,
    RING   = 2,
    PINKY  = 3,
    COUNT  = 4
};

namespace FingerMask {
    constexpr uint8_t INDEX  = 0x01;
    constexpr uint8_t MIDDLE = 0x02;
    constexpr uint8_t RING   = 0x04;
    constexpr uint8_t PINKY  = 0x08;
    constexpr uint8_t ALL    = 0x0F;
}

enum class GestureType : uint8_t {
    NONE = 0,
    SINGLE_TAP,
    DOUBLE_TAP,
    HOLD_TRIGGERED,
    HOLD_REPEAT,
    COMBO_TAP
};

struct GestureEvent {
    GestureType   type       = GestureType::NONE;
    uint8_t       fingerMask = 0;
    unsigned long timestamp  = 0;
};
