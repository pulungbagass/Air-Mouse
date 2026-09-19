/**
 * DebugConsole.h
 * ----------------------------------------------------------------------
 * Modul OPSIONAL untuk menguji perangkat tanpa sensor fisik terpasang
 * (MPU9250 dan/atau sensor sentuh belum ada). Membaca satu karakter dari
 * Serial Monitor dan menyuntikkan GestureEvent yang setara persis dengan
 * yang dihasilkan TouchHandler dari sentuhan jari sungguhan, langsung ke
 * ActionMapper - sehingga seluruh pipeline (termasuk logika Mode 1/Mode 2,
 * drag-lock, toggle mode, dst) ikut teruji, bukan cuma output BLE mentah.
 *
 * Aktif/nonaktifkan lewat ENABLE_DEBUG_CONSOLE di Config.h.
 * ----------------------------------------------------------------------
 */
#pragma once
#include <Arduino.h>
#include "AppState.h"
#include "ActionMapper.h"
#include "BleHandler.h"

class DebugConsole {
public:
    void begin(ActionMapper *am, BleHandler *bleRef);

    // Panggil setiap iterasi loop(); non-blocking (hanya memproses karakter
    // yang sudah tersedia di buffer Serial, tidak pernah menunggu).
    void update();

private:
    ActionMapper *actionMapper = nullptr;
    BleHandler   *ble          = nullptr;

    void printHelp();
    void handleChar(char c);
    void emit(GestureType type, uint8_t mask);
};
