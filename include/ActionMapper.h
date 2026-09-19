/**
 * ActionMapper.h
 * ----------------------------------------------------------------------
 * Lapisan "business logic": menerjemahkan GestureEvent murni dari
 * TouchHandler menjadi aksi BLE nyata lewat BleHandler, sesuai tabel
 * shortcut Mode 1 (Navigasi) & Mode 2 (Media/Produktivitas), termasuk:
 *   - toggle mode (hold Manis 3 detik)
 *   - drag & drop lock (hold Telunjuk di Mode 1)
 *   - pause/resume sensor gerak (hold Kelingking di Mode 1)
 *   - re-center MPU9250 (hold Tengah di Mode 1)
 *
 * Modul ini TIDAK melakukan I/O langsung ke pin atau ke BLE stack - ia
 * hanya mengorkestrasi pemanggilan BleHandler & MpuHandler.
 * ----------------------------------------------------------------------
 */
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
