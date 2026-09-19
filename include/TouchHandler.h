/**
 * TouchHandler.h
 * ----------------------------------------------------------------------
 * Modul KHUSUS state machine 4 tombol jari ("Ground Switch" via body
 * conduction ke pelat jempol yang di-GND-kan). Modul ini TIDAK mengetahui
 * apa pun soal mode aplikasi atau aksi BLE - ia murni menerjemahkan pola
 * tekan/lepas mentah menjadi GestureEvent tingkat tinggi:
 *
 *   - SINGLE_TAP      : 1x tekan-lepas singkat pada satu jari
 *   - DOUBLE_TAP       : dua tap berurutan pada jari yang SAMA dalam
 *                        DOUBLE_CLICK_MS
 *   - HOLD_TRIGGERED   : satu jari ditahan >= HOLD_DURATION_MS
 *   - HOLD_REPEAT       : jari yang sama masih ditahan, diulang tiap
 *                        HOLD_REPEAT_MS (untuk aksi kontinu, mis. seek)
 *   - COMBO_TAP        : >= 2 jari ditekan bersamaan dalam CHORD_WINDOW_MS
 *
 * Non-blocking sepenuhnya (berbasis millis()), dan dapat menghasilkan lebih
 * dari satu event dalam satu iterasi loop() - karena itu update() harus
 * dipanggil berulang (while) sampai mengembalikan false.
 * ----------------------------------------------------------------------
 */
#pragma once
#include <Arduino.h>
#include "AppState.h"

class TouchHandler {
public:
    void begin();

    // Panggil berulang di dalam loop() sampai mengembalikan false:
    //   GestureEvent evt;
    //   while (touchHandler.update(evt)) { actionMapper.handleGesture(evt); }
    bool update(GestureEvent &outEvent);

private:
    static const uint8_t NUM_FINGERS     = 4;
    static const uint8_t EVENT_QUEUE_SIZE = 8;

    uint8_t pins[NUM_FINGERS];

    // --- Debounce per jari ---
    bool          lastRawRead[NUM_FINGERS];
    unsigned long lastDebounceTime[NUM_FINGERS];
    bool          pressed[NUM_FINGERS];

    // --- Timing per jari ---
    unsigned long pressStartTime[NUM_FINGERS];
    unsigned long lastHoldRepeatTime[NUM_FINGERS];
    bool          holdFired[NUM_FINGERS];

    // Jari ini baru saja "dipakai" oleh sebuah COMBO_TAP yang sudah ditembakkan;
    // saat dilepas, jangan hasilkan SINGLE_TAP untuknya.
    bool suppressRelease[NUM_FINGERS];

    // --- Double-click pending (menunggu kemungkinan tap ke-2) ---
    bool          clickPending[NUM_FINGERS];
    unsigned long clickPendingSince[NUM_FINGERS];

    // --- Pengumpulan jendela chord/combo ---
    bool          groupActive;
    bool          groupLocked;
    unsigned long groupStartTime;
    uint8_t       groupMask;

    // --- Antrian event keluar (ring buffer sederhana) ---
    GestureEvent eventQueue[EVENT_QUEUE_SIZE];
    uint8_t queueHead;
    uint8_t queueTail;
    uint8_t queueCount;

    void pushEvent(GestureType type, uint8_t mask, unsigned long ts);
    void onFingerPressed(uint8_t f, unsigned long now);
    void onFingerReleased(uint8_t f, unsigned long now);
    void finalizeGroup(unsigned long now);
};
