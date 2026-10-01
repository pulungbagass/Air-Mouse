#pragma once
#include <Arduino.h>
#include <atomic>
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>
#include "HidKeys.h"

class BleHidDevice : public NimBLEServerCallbacks {
public:
    using NimBLEServerCallbacks::onConnect;
    using NimBLEServerCallbacks::onDisconnect;

    void begin();
    bool isConnected() const { return connected_.load(std::memory_order_relaxed); }
    void clearBonds();
    void serviceHousekeeping();

    void mouseMove(int32_t dx, int32_t dy, int8_t wheel = 0, int8_t pan = 0);
    void mousePress(uint8_t button);
    void mouseRelease(uint8_t button);
    void mouseClick(uint8_t button);

    void keyPress(uint8_t key);
    void keyRelease(uint8_t key);
    void keyWrite(uint8_t key);
    void keyReleaseAll();

    void consumerTap(uint16_t usage);

    void onConnect(NimBLEServer *server, ble_gap_conn_desc *desc) override;
    void onDisconnect(NimBLEServer *server, ble_gap_conn_desc *desc) override;

private:
    struct KeyReport {
        uint8_t modifiers;
        uint8_t reserved;
        uint8_t keys[6];
    };

    NimBLEServer *server_ = nullptr;
    NimBLEHIDDevice *hid_ = nullptr;
    NimBLECharacteristic *inMouse_ = nullptr;
    NimBLECharacteristic *inKeyboard_ = nullptr;
    NimBLECharacteristic *outKeyboard_ = nullptr;
    NimBLECharacteristic *inConsumer_ = nullptr;

    std::atomic<bool> connected_{false};
    std::atomic<bool> resetRequested_{false};
    uint8_t mouseButtons_ = 0;
    KeyReport keyReport_ = {0, 0, {0, 0, 0, 0, 0, 0}};

    void applyProfileRevision();
    void sendMouse(int8_t x, int8_t y, int8_t wheel, int8_t pan);
    void sendKeyboard();
    void sendConsumer(uint16_t usage);
    bool resolveKey(uint8_t key, uint8_t &usage, uint8_t &modifierBits) const;
};
