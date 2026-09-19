/**
 * Config.h
 * ----------------------------------------------------------------------
 * Pusat konfigurasi perangkat keras & parameter waktu "Air Mouse".
 * Semua nilai yang mungkin perlu di-tuning (sensitivitas, timing, pin)
 * SENGAJA dikumpulkan di satu file ini agar mudah di-maintain tanpa perlu
 * menyentuh logika di file .cpp manapun.
 * ----------------------------------------------------------------------
 */
#pragma once

// ============================================================================
// IDENTITAS PERANGKAT BLE
// ============================================================================
#define BLE_DEVICE_NAME "Air Mouse"

// ============================================================================
// PIN MAPPING - SENSOR GERAK (MPU9250 via I2C)
// ============================================================================
#define PIN_MPU_SDA      8
#define PIN_MPU_SCL      9
#define MPU_I2C_ADDRESS  0x68   // alamat default saat pin AD0 MPU9250 = GND

// ============================================================================
// PIN MAPPING - SENSOR SENTUH JARI ("Ground Switch")
// ----------------------------------------------------------------------------
// Pelat tembaga di JEMPOL disambung permanen ke GND (bukan ke GPIO).
// Pin di bawah ini WAJIB mode INPUT_PULLUP: saat idle pin akan HIGH, dan
// begitu jari (yang sedang menyentuh pelat jempol/GND) menyentuh pelat pada
// jari lain, rangkaian tertutup melalui tubuh pengguna sehingga pin tersebut
// terbaca LOW. Jadi: LOW = jari sedang menyentuh / ditekan.
// ============================================================================
#define PIN_FINGER_INDEX   1   // Telunjuk
#define PIN_FINGER_MIDDLE  2   // Tengah
#define PIN_FINGER_RING    3   // Manis
#define PIN_FINGER_PINKY   4   // Kelingking

// ============================================================================
// PARAMETER WAKTU (semua berbasis millis(), TIDAK ADA delay() yang memblokir)
// ============================================================================
#define DEBOUNCE_MS          40UL   // 30 - 50 ms
#define CHORD_WINDOW_MS       80UL  // 50 - 100 ms, toleransi tap bersamaan (combo)
#define DOUBLE_CLICK_MS      300UL  // jendela deteksi double click
#define HOLD_DURATION_MS    3000UL  // >= 3000 ms untuk aksi "hold"
#define HOLD_REPEAT_MS       400UL  // interval pengulangan aksi hold kontinu
                                     // (dipakai misalnya oleh Fast Forward/Rewind)

// ============================================================================
// PARAMETER GERAKAN KURSOR (MPU9250)
// ============================================================================
// MOUSE_SENSITIVITY : pengali dari (derajat/detik) gyro -> piksel pergerakan.
//                      Naikkan nilai ini jika kursor terasa terlalu lambat,
//                      turunkan jika terlalu liar/cepat.
#define MOUSE_SENSITIVITY     14.0f

// GYRO_DEADZONE_DPS : ambang batas noise; gerakan gyro di bawah nilai ini
//                      (derajat/detik) akan diabaikan agar kursor tidak
//                      "gemetar" saat tangan diam.
#define GYRO_DEADZONE_DPS      1.2f

// MAX_MOUSE_DELTA   : batas maksimum perpindahan piksel per satu update,
//                      mencegah lonjakan kursor akibat gerakan tersentak.
#define MAX_MOUSE_DELTA          30

// RECENTER_DURATION_MS : lama sampling non-blocking saat fitur re-center /
//                         kalibrasi ulang titik nol MPU9250 dijalankan.
#define RECENTER_DURATION_MS  400UL
