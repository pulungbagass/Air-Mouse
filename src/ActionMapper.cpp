#include "ActionMapper.h"
#include <BleKeyboard.h>
#include <BleMouse.h>

void ActionMapper::begin(BleHandler *bleRef, MpuHandler *mpuRef) {
    ble = bleRef;
    mpu = mpuRef;
    currentMode    = OperationMode::MODE_1_NAVIGATION;
    dragLockActive = false;
    motionPaused   = false;
}

void ActionMapper::toggleMode() {
    if (currentMode == OperationMode::MODE_1_NAVIGATION) {
        currentMode = OperationMode::MODE_2_MEDIA;
        Serial.println(F("[Mode] -> MODE 2 (Media & Produktivitas)"));
    } else {
        currentMode = OperationMode::MODE_1_NAVIGATION;
        if (mpu) mpu->resetTimer(); // cegah lonjakan dt setelah sensor gerak aktif lagi
        Serial.println(F("[Mode] -> MODE 1 (Navigasi Kursor)"));
    }
}

void ActionMapper::handleGesture(const GestureEvent &event) {
    if (!ble) return;

    if (currentMode == OperationMode::MODE_1_NAVIGATION) {
        handleMode1(event);
    } else {
        handleMode2(event);
    }
}

// ============================================================================
// MODE 1 - NAVIGASI KURSOR UTAMA
// ============================================================================
void ActionMapper::handleMode1(const GestureEvent &event) {
    switch (event.type) {
        case GestureType::SINGLE_TAP:   handleMode1SingleTap(event.fingerMask); break;
        case GestureType::DOUBLE_TAP:   handleMode1DoubleTap(event.fingerMask); break;
        case GestureType::HOLD_TRIGGERED:
        case GestureType::HOLD_REPEAT:  handleMode1Hold(event); break;
        case GestureType::COMBO_TAP:    handleMode1Combo(event.fingerMask); break;
        default: break;
    }
}

void ActionMapper::handleMode1SingleTap(uint8_t mask) {
    switch (mask) {
        case FingerMask::INDEX:
            if (dragLockActive) {
                // Tap ke-2 setelah hold Telunjuk -> lepaskan Drag & Drop Lock.
                ble->mouseRelease(MOUSE_LEFT);
                dragLockActive = false;
                Serial.println(F("[Mode1] Drag lock DILEPAS"));
            } else {
                ble->mouseClick(MOUSE_LEFT);
            }
            break;

        case FingerMask::MIDDLE:
            ble->mouseClick(MOUSE_RIGHT);
            break;

        case FingerMask::RING:
            ble->mouseScroll(1);   // Scroll Up
            break;

        case FingerMask::PINKY:
            ble->mouseScroll(-1);  // Scroll Down
            break;

        default: break;
    }
}

void ActionMapper::handleMode1DoubleTap(uint8_t mask) {
    switch (mask) {
        case FingerMask::INDEX:
            // Buka Folder/Fungsi OS Default -> Enter (aksi "open" standar di
            // File Explorer Windows maupun pengelola berkas Android).
            ble->tapKey(KEY_RETURN);
            break;

        case FingerMask::MIDDLE:
            // Buka/Tutup Virtual Keyboard -> shortcut bawaan Windows 10/11
            // untuk toggle Touch Keyboard: Win + Ctrl + O.
            ble->pressKey(KEY_LEFT_GUI);
            ble->pressKey(KEY_LEFT_CTRL);
            ble->tapKey('o');
            ble->releaseKey(KEY_LEFT_CTRL);
            ble->releaseKey(KEY_LEFT_GUI);
            break;

        case FingerMask::RING:
            ble->tapKey(KEY_PAGE_UP);
            break;

        case FingerMask::PINKY:
            ble->tapKey(KEY_PAGE_DOWN);
            break;

        default: break;
    }
}

void ActionMapper::handleMode1Hold(const GestureEvent &event) {
    const bool isFirst = (event.type == GestureType::HOLD_TRIGGERED);

    switch (event.fingerMask) {
        case FingerMask::INDEX:
            // Drag & Drop Lock: tahan klik kiri hingga tap 1x lagi (ditangani
            // di handleMode1SingleTap) untuk melepas.
            if (isFirst && !dragLockActive) {
                ble->mousePress(MOUSE_LEFT);
                dragLockActive = true;
                Serial.println(F("[Mode1] Drag lock AKTIF"));
            }
            break;

        case FingerMask::MIDDLE:
            // Re-center / kalibrasi nol MPU9250 (non-blocking, lihat MpuHandler).
            if (isFirst && mpu) {
                mpu->startRecenter();
                Serial.println(F("[Mode1] Re-center MPU9250..."));
            }
            break;

        case FingerMask::RING:
            // TOGGLE KE MODE 2
            if (isFirst) toggleMode();
            break;

        case FingerMask::PINKY:
            // Pause/Sleep sensor gerak
            if (isFirst) {
                motionPaused = !motionPaused;
                if (!motionPaused && mpu) mpu->resetTimer();
                Serial.println(motionPaused ? F("[Mode1] Sensor gerak DIJEDA")
                                             : F("[Mode1] Sensor gerak AKTIF"));
            }
            break;

        default: break;
    }
}

void ActionMapper::handleMode1Combo(uint8_t mask) {
    switch (mask) {
        case (uint8_t)(FingerMask::INDEX | FingerMask::MIDDLE): // Ctrl+C
            ble->pressKey(KEY_LEFT_CTRL);
            ble->tapKey('c');
            ble->releaseKey(KEY_LEFT_CTRL);
            break;

        case (uint8_t)(FingerMask::INDEX | FingerMask::RING): // Ctrl+V
            ble->pressKey(KEY_LEFT_CTRL);
            ble->tapKey('v');
            ble->releaseKey(KEY_LEFT_CTRL);
            break;

        case (uint8_t)(FingerMask::MIDDLE | FingerMask::RING): // Ctrl+Scroll Up (Zoom In)
            ble->pressKey(KEY_LEFT_CTRL);
            ble->mouseScroll(1);
            ble->releaseKey(KEY_LEFT_CTRL);
            break;

        case (uint8_t)(FingerMask::MIDDLE | FingerMask::PINKY): // Ctrl+Scroll Down (Zoom Out)
            ble->pressKey(KEY_LEFT_CTRL);
            ble->mouseScroll(-1);
            ble->releaseKey(KEY_LEFT_CTRL);
            break;

        case (uint8_t)(FingerMask::INDEX | FingerMask::MIDDLE | FingerMask::RING): // Win+Tab
            ble->pressKey(KEY_LEFT_GUI);
            ble->tapKey(KEY_TAB);
            ble->releaseKey(KEY_LEFT_GUI);
            break;

        case (uint8_t)(FingerMask::INDEX | FingerMask::MIDDLE | FingerMask::PINKY): // Win+D
            ble->pressKey(KEY_LEFT_GUI);
            ble->tapKey('d');
            ble->releaseKey(KEY_LEFT_GUI);
            break;

        case FingerMask::ALL: // Win+L
            ble->pressKey(KEY_LEFT_GUI);
            ble->tapKey('l');
            ble->releaseKey(KEY_LEFT_GUI);
            break;

        default:
            break; // Kombinasi tidak terdefinisi pada Mode 1, diabaikan.
    }
}

// ============================================================================
// MODE 2 - MEDIA & PRODUKTIVITAS (sensor gerak MPU9250 nonaktif sepenuhnya)
// ============================================================================
void ActionMapper::handleMode2(const GestureEvent &event) {
    switch (event.type) {
        case GestureType::SINGLE_TAP:   handleMode2SingleTap(event.fingerMask); break;
        case GestureType::DOUBLE_TAP:   handleMode2DoubleTap(event.fingerMask); break;
        case GestureType::HOLD_TRIGGERED:
        case GestureType::HOLD_REPEAT:  handleMode2Hold(event); break;
        case GestureType::COMBO_TAP:    handleMode2Combo(event.fingerMask); break;
        default: break;
    }
}

void ActionMapper::handleMode2SingleTap(uint8_t mask) {
    switch (mask) {
        case FingerMask::INDEX:  ble->mediaPlayPause();     break;
        case FingerMask::MIDDLE: ble->mediaMute();          break;
        case FingerMask::RING:   ble->mediaNextTrack();     break;
        case FingerMask::PINKY:  ble->mediaPreviousTrack(); break;
        default: break;
    }
}

void ActionMapper::handleMode2DoubleTap(uint8_t mask) {
    switch (mask) {
        case FingerMask::INDEX:
            ble->tapKey(KEY_F11); // Full Screen
            break;

        case FingerMask::MIDDLE:
            // Win+D - Show Desktop
            ble->pressKey(KEY_LEFT_GUI);
            ble->tapKey('d');
            ble->releaseKey(KEY_LEFT_GUI);
            break;

        case FingerMask::RING:
            // Next Virtual Desktop: Ctrl+Win+Right Arrow
            ble->pressKey(KEY_LEFT_CTRL);
            ble->pressKey(KEY_LEFT_GUI);
            ble->tapKey(KEY_RIGHT_ARROW);
            ble->releaseKey(KEY_LEFT_GUI);
            ble->releaseKey(KEY_LEFT_CTRL);
            break;

        case FingerMask::PINKY:
            // Previous Virtual Desktop: Ctrl+Win+Left Arrow
            ble->pressKey(KEY_LEFT_CTRL);
            ble->pressKey(KEY_LEFT_GUI);
            ble->tapKey(KEY_LEFT_ARROW);
            ble->releaseKey(KEY_LEFT_GUI);
            ble->releaseKey(KEY_LEFT_CTRL);
            break;

        default: break;
    }
}

void ActionMapper::handleMode2Hold(const GestureEvent &event) {
    switch (event.fingerMask) {
        case FingerMask::INDEX:
            // Fast Forward: kirim Right Arrow berulang (HOLD_TRIGGERED lalu
            // setiap HOLD_REPEAT) - kompatibel dengan skip-seek di YouTube,
            // VLC, dan sebagian besar pemutar video/musik populer.
            ble->tapKey(KEY_RIGHT_ARROW);
            break;

        case FingerMask::MIDDLE:
            // Rewind
            ble->tapKey(KEY_LEFT_ARROW);
            break;

        case FingerMask::RING:
            // TOGGLE KEMBALI KE MODE 1 (hanya sekali saat threshold tercapai)
            if (event.type == GestureType::HOLD_TRIGGERED) toggleMode();
            break;

        case FingerMask::PINKY:
            // Win+Tab - Task View
            if (event.type == GestureType::HOLD_TRIGGERED) {
                ble->pressKey(KEY_LEFT_GUI);
                ble->tapKey(KEY_TAB);
                ble->releaseKey(KEY_LEFT_GUI);
            }
            break;

        default: break;
    }
}

void ActionMapper::handleMode2Combo(uint8_t mask) {
    switch (mask) {
        case (uint8_t)(FingerMask::INDEX | FingerMask::MIDDLE): // Volume Up
            ble->mediaVolumeUp();
            break;

        case (uint8_t)(FingerMask::INDEX | FingerMask::RING): // Volume Down
            ble->mediaVolumeDown();
            break;

        case (uint8_t)(FingerMask::MIDDLE | FingerMask::PINKY): // Close Window (Alt+F4)
            ble->pressKey(KEY_LEFT_ALT);
            ble->tapKey(KEY_F4);
            ble->releaseKey(KEY_LEFT_ALT);
            break;

        default:
            break; // Kombinasi tidak terdefinisi pada Mode 2, diabaikan.
    }
}
