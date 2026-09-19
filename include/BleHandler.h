/**
 * BleHandler.h
 * ----------------------------------------------------------------------
 * Modul KHUSUS koneksi & output BLE HID. Ini adalah satu-satunya lapisan
 * yang bergantung langsung pada library "ESP32-BLE-Combo" (BleKeyboard.h /
 * BleMouse.h) sehingga jika suatu hari library diganti, hanya file .h/.cpp
 * ini yang perlu disentuh - ActionMapper cukup memanggil fungsi primitif
 * di bawah tanpa perlu tahu library apa yang dipakai di baliknya.
 *
 * Perangkat memancarkan SATU BLE HID device bernama "Air Mouse" yang
 * langsung dikenali sebagai Mouse + Keyboard (+ Media Keys) sekaligus oleh
 * Windows maupun Android, tanpa dongle tambahan.
 * ----------------------------------------------------------------------
 */
#pragma once
#include <Arduino.h>

class BleHandler {
public:
    // Set nama device BLE lalu mulai iklan (advertising) BLE HID.
    void begin();
    bool isConnected();

    // ---------------- Mouse ----------------
    void moveMouse(int16_t dx, int16_t dy);
    void mouseScroll(int8_t amount);          // + = scroll up, - = scroll down
    void mouseClick(uint8_t button);          // klik sesaat (press+release)
    void mousePress(uint8_t button);
    void mouseRelease(uint8_t button);
    bool isMouseButtonPressed(uint8_t button);

    // ---------------- Keyboard (primitif mentah) ----------------
    void tapKey(uint8_t key);                 // press+release satu key/char
    void pressKey(uint8_t key);
    void releaseKey(uint8_t key);
    void releaseAllKeys();

    // ---------------- Media / Consumer Control ----------------
    void mediaPlayPause();
    void mediaMute();
    void mediaNextTrack();
    void mediaPreviousTrack();
    void mediaVolumeUp();
    void mediaVolumeDown();
};
