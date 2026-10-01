#pragma once
#include <Arduino.h>
#include "AppState.h"
#include "BleHandler.h"
#include "MpuHandler.h"

class ActionMapper {
public:
    void begin(BleHandler *bleRef, MpuHandler *mpuRef);
    void handleGesture(const GestureEvent &event);

    OperationMode getMode() const         { return currentMode; }
    bool          isMotionPaused() const  { return motionPaused; }
    bool          isDragLockActive() const{ return dragLockActive; }

private:
    BleHandler *ble = nullptr;
    MpuHandler *mpu = nullptr;

    OperationMode currentMode    = OperationMode::MODE_1_NAVIGATION;
    bool          dragLockActive = false;
    bool          motionPaused   = false;

    void toggleMode();

    void handleMode1(const GestureEvent &event);
    void handleMode2(const GestureEvent &event);

    void handleMode1SingleTap(uint8_t mask);
    void handleMode1DoubleTap(uint8_t mask);
    void handleMode1Hold(const GestureEvent &event);
    void handleMode1Combo(uint8_t mask);

    void handleMode2SingleTap(uint8_t mask);
    void handleMode2DoubleTap(uint8_t mask);
    void handleMode2Hold(const GestureEvent &event);
    void handleMode2Combo(uint8_t mask);
};
