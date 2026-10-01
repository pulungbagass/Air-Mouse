#pragma once
#include <Arduino.h>
#include "AppState.h"

class TouchHandler {
public:
    void begin();

    bool update(GestureEvent &outEvent);
    unsigned long lastRawChangeTime() const { return lastRawChange; }

private:
    static const uint8_t NUM_FINGERS     = 4;
    static const uint8_t EVENT_QUEUE_SIZE = 8;

    uint8_t pins[NUM_FINGERS];

    bool          lastRawRead[NUM_FINGERS];
    unsigned long lastDebounceTime[NUM_FINGERS];
    bool          pressed[NUM_FINGERS];

    unsigned long pressStartTime[NUM_FINGERS];
    unsigned long lastHoldRepeatTime[NUM_FINGERS];
    bool          holdFired[NUM_FINGERS];

    bool suppressRelease[NUM_FINGERS];

    bool          clickPending[NUM_FINGERS];
    unsigned long clickPendingSince[NUM_FINGERS];

    unsigned long lastRawChange;

    bool          groupActive;
    bool          groupLocked;
    unsigned long groupStartTime;
    uint8_t       groupMask;

    GestureEvent eventQueue[EVENT_QUEUE_SIZE];
    uint8_t queueHead;
    uint8_t queueTail;
    uint8_t queueCount;

    void pushEvent(GestureType type, uint8_t mask, unsigned long ts);
    void onFingerPressed(uint8_t f, unsigned long now);
    void onFingerReleased(uint8_t f, unsigned long now);
    void finalizeGroup(unsigned long now);
};
