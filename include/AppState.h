/**
 * AppState.h
 * ----------------------------------------------------------------------
 * Definisi tipe data & enum bersama (shared) yang dipakai lintas modul:
 * mode operasi perangkat, identitas jari, jenis gesture, dan struct event
 * hasil deteksi TouchHandler yang dikonsumsi oleh ActionMapper.
 * ----------------------------------------------------------------------
 */
#pragma once
#include <Arduino.h>

// ============================================================================
// MODE OPERASI PERANGKAT
// ============================================================================
enum class OperationMode : uint8_t {
    MODE_1_NAVIGATION = 1,   // Navigasi kursor utama (default saat boot)
    MODE_2_MEDIA      = 2    // Media & produktivitas (sensor gerak nonaktif)
};

// ============================================================================
// IDENTITAS JARI
// ============================================================================
enum class FingerID : uint8_t {
    INDEX  = 0,   // Telunjuk
    MIDDLE = 1,   // Tengah
    RING   = 2,   // Manis
    PINKY  = 3,   // Kelingking
    COUNT  = 4
};

// Bitmask tetap untuk tiap jari, dipakai oleh GestureEvent::fingerMask dan
// oleh tabel pemetaan combo di ActionMapper.
namespace FingerMask {
    constexpr uint8_t INDEX  = 0x01;
    constexpr uint8_t MIDDLE = 0x02;
    constexpr uint8_t RING   = 0x04;
    constexpr uint8_t PINKY  = 0x08;
    constexpr uint8_t ALL    = 0x0F;
}

// ============================================================================
// JENIS GESTURE YANG DIHASILKAN OLEH TouchHandler
// ============================================================================
enum class GestureType : uint8_t {
    NONE = 0,
    SINGLE_TAP,       // 1x tap singkat pada satu jari
    DOUBLE_TAP,       // double click pada satu jari
    HOLD_TRIGGERED,   // dipicu SATU KALI tepat saat ambang hold (>=3s) tercapai
    HOLD_REPEAT,      // dipicu berulang setiap HOLD_REPEAT_MS selama masih ditahan
    COMBO_TAP         // 2/3/4 jari ditekan bersamaan (dalam CHORD_WINDOW_MS)
};

// Event tunggal yang di-emit oleh TouchHandler dan dikonsumsi oleh ActionMapper.
struct GestureEvent {
    GestureType   type       = GestureType::NONE;
    uint8_t       fingerMask = 0;   // kombinasi bit FingerMask::*
    unsigned long timestamp  = 0;
};
