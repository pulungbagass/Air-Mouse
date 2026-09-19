/**
 * MpuHandler.h
 * ----------------------------------------------------------------------
 * Modul KHUSUS untuk komunikasi I2C dengan sensor 9-axis MPU9250.
 * Tanggung jawab modul ini HANYA:
 *   1. Inisialisasi & kalibrasi bias gyro (termasuk re-center non-blocking).
 *   2. Membaca gyro secara berkala dan mengubahnya menjadi delta pergerakan
 *      kursor (dx, dy) yang siap dikirim ke BleHandler.
 *
 * Modul ini TIDAK mengetahui apa pun soal BLE, tombol, atau mode aplikasi -
 * itu adalah tanggung jawab ActionMapper / main.cpp.
 * ----------------------------------------------------------------------
 */
#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "MPU9250.h"

// Hasil satu kali pembacaan MpuHandler::update().
struct MouseDelta {
    int16_t dx = 0;
    int16_t dy = 0;
};

class MpuHandler {
public:
    // Inisialisasi I2C, sensor, dan kalibrasi bias awal (dipanggil di setup()).
    void begin();

    // Dipanggil setiap iterasi loop(). Mengembalikan true jika ada sampel gyro
    // baru yang berhasil diproses (delta bisa saja (0,0) jika di bawah deadzone
    // atau sedang dalam proses re-center).
    bool update(MouseDelta &out);

    // Memicu proses re-kalibrasi titik nol (bias gyro) secara NON-BLOCKING.
    // Selama proses ini berjalan (RECENTER_DURATION_MS), delta pergerakan
    // kursor akan bernilai (0,0).
    void startRecenter();
    bool isRecentering() const;

    // Reset referensi waktu delta-t internal. Wajib dipanggil setiap kali
    // pembacaan sensor sempat "dijeda" dalam waktu lama (mis. setelah keluar
    // dari Mode 2, atau setelah fitur pause/sleep sensor dinonaktifkan) agar
    // tidak terjadi lonjakan delta akibat dt yang membesar.
    void resetTimer();

    bool isReady() const { return ready; }

private:
    MPU9250 mpu;
    bool ready = false;
    unsigned long lastSampleTime = 0;

    // Bias gyro (derajat/detik) hasil kalibrasi, dikurangkan dari tiap sampel.
    float gyroBiasX = 0.0f;
    float gyroBiasY = 0.0f;

    // State proses re-center non-blocking.
    bool          recentering      = false;
    unsigned long recenterStartTime = 0;
    float         recenterSumX     = 0.0f;
    float         recenterSumY     = 0.0f;
    uint16_t      recenterSamples  = 0;

    void finishRecenter();
};
