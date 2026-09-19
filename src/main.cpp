/**
 * main.cpp - Air Mouse ESP32-S3 Super Mini
 * ----------------------------------------------------------------------
 * Tanggung jawab file ini HANYA:
 *   1. Inisialisasi semua modul (BLE, MPU9250, Touch, ActionMapper).
 *   2. Menjalankan main loop non-blocking yang:
 *        a. Mengalirkan (drain) semua GestureEvent yang tertunda ke
 *           ActionMapper terlebih dahulu, agar respons tombol tidak pernah
 *           tertunda oleh pemrosesan IMU.
 *        b. Membaca MPU9250 dan mengirim pergerakan kursor - HANYA saat
 *           Mode 1 aktif dan sensor tidak sedang dijeda, sesuai spesifikasi.
 *
 * TIDAK ADA logika bisnis (pemetaan gesture->aksi) atau akses register
 * sensor/BLE langsung di file ini - semua didelegasikan ke modul terkait.
 * ----------------------------------------------------------------------
 */
#include <Arduino.h>
#include "Config.h"
#include "AppState.h"
#include "BleHandler.h"
#include "MpuHandler.h"
#include "TouchHandler.h"
#include "ActionMapper.h"

static BleHandler   bleHandler;
static MpuHandler    mpuHandler;
static TouchHandler  touchHandler;
static ActionMapper  actionMapper;

// Heartbeat status koneksi (non-blocking, hanya untuk keperluan debug Serial).
static unsigned long lastStatusPrint = 0;
static const unsigned long STATUS_INTERVAL_MS = 5000UL;

void setup() {
    Serial.begin(115200);
    // Penantian singkat SEKALI di setup() agar port USB-CDC sempat
    // ter-enumerasi sebelum log pertama dicetak. Ini terjadi sebelum
    // loop() non-blocking berjalan, sehingga tidak melanggar aturan
    // "tanpa delay() yang memblokir pergerakan kursor".
    delay(200);

    Serial.println();
    Serial.println(F("=== Air Mouse (ESP32-S3 Super Mini) - booting ==="));

    bleHandler.begin();
    mpuHandler.begin();
    touchHandler.begin();
    actionMapper.begin(&bleHandler, &mpuHandler);

    Serial.println(F("Siap. Mode aktif: MODE 1 (Navigasi Kursor)"));
    Serial.println(F("Menunggu koneksi BLE ke Windows/Android sebagai 'Air Mouse'..."));
}

void loop() {
    // ------------------------------------------------------------------
    // 1) Prioritaskan semua gesture tombol yang tertunda. Satu iterasi
    //    loop() bisa saja menghasilkan lebih dari satu event (mis. sebuah
    //    SINGLE_TAP yang baru saja "expired" bersamaan dengan HOLD_REPEAT
    //    jari lain) - karena itu di-drain dengan while, bukan if.
    // ------------------------------------------------------------------
    GestureEvent evt;
    while (touchHandler.update(evt)) {
        actionMapper.handleGesture(evt);
    }

    // ------------------------------------------------------------------
    // 2) Streaming koordinat gerak - HANYA aktif di Mode 1 dan saat sensor
    //    tidak dijeda. Di Mode 2, pembacaan MPU9250 dimatikan sepenuhnya
    //    (tidak dipanggil sama sekali) sesuai spesifikasi.
    // ------------------------------------------------------------------
    if (actionMapper.getMode() == OperationMode::MODE_1_NAVIGATION &&
        !actionMapper.isMotionPaused()) {

        MouseDelta delta;
        if (mpuHandler.update(delta)) {
            if (delta.dx != 0 || delta.dy != 0) {
                bleHandler.moveMouse(delta.dx, delta.dy);
            }
        }
    }

    // ------------------------------------------------------------------
    // (Opsional) Heartbeat status koneksi setiap 5 detik, murni untuk debug.
    // ------------------------------------------------------------------
    unsigned long now = millis();
    if (now - lastStatusPrint >= STATUS_INTERVAL_MS) {
        lastStatusPrint = now;
        Serial.print(F("[Status] BLE: "));
        Serial.print(bleHandler.isConnected() ? F("Connected") : F("Advertising..."));
        Serial.print(F(" | Mode: "));
        Serial.print(actionMapper.getMode() == OperationMode::MODE_1_NAVIGATION ? "1" : "2");
        Serial.print(F(" | Sensor: "));
        Serial.println(actionMapper.isMotionPaused() ? F("Paused") : F("Active"));
    }

    // Tidak ada delay() di sini - seluruh timing (debounce, hold, chord,
    // double-click, sampling IMU) sepenuhnya berbasis millis() non-blocking.
}
