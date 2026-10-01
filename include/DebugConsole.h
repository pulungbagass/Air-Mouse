#pragma once
#include <Arduino.h>
#include "AppState.h"
#include "ActionMapper.h"
#include "BleHandler.h"
#include "MpuHandler.h"

class DebugConsole {
public:
    void begin(ActionMapper *am, BleHandler *bleRef, MpuHandler *mpuRef);

    void update();

private:
    ActionMapper *actionMapper = nullptr;
    BleHandler   *ble          = nullptr;
    MpuHandler   *mpu          = nullptr;
    bool          telemetryOn  = false;
    unsigned long lastTelemetry = 0;

    void printHelp();
    void handleChar(char c);
    void emit(GestureType type, uint8_t mask);
};
