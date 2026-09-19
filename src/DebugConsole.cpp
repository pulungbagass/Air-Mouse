#include "DebugConsole.h"

void DebugConsole::begin(ActionMapper *am, BleHandler *bleRef) {
    actionMapper = am;
    ble          = bleRef;
    printHelp();
}

void DebugConsole::update() {
    // Proses semua karakter yang SUDAH tersedia di buffer, lalu langsung
    // kembali - tidak pernah menunggu (Serial.available() bersifat non-blocking).
    while (Serial.available() > 0) {
        char c = (char)Serial.read();
        if (c == '\r' || c == '\n') continue; // abaikan enter/newline
        handleChar(c);
    }
}

void DebugConsole::emit(GestureType type, uint8_t mask) {
    if (!actionMapper) return;
    GestureEvent evt;
    evt.type       = type;
    evt.fingerMask = mask;
    evt.timestamp  = millis();
    actionMapper->handleGesture(evt); // menempuh jalur yang PERSIS sama dengan sentuhan asli
}

void DebugConsole::handleChar(char c) {
    switch (c) {
        // ----------------- Single Tap -----------------
        case '1': Serial.println(F("[TEST] SINGLE_TAP   Telunjuk"));
                  emit(GestureType::SINGLE_TAP, FingerMask::INDEX); break;
        case '2': Serial.println(F("[TEST] SINGLE_TAP   Tengah"));
                  emit(GestureType::SINGLE_TAP, FingerMask::MIDDLE); break;
        case '3': Serial.println(F("[TEST] SINGLE_TAP   Manis"));
                  emit(GestureType::SINGLE_TAP, FingerMask::RING); break;
        case '4': Serial.println(F("[TEST] SINGLE_TAP   Kelingking"));
                  emit(GestureType::SINGLE_TAP, FingerMask::PINKY); break;

        // ----------------- Double Tap -----------------
        case 'q': Serial.println(F("[TEST] DOUBLE_TAP   Telunjuk"));
                  emit(GestureType::DOUBLE_TAP, FingerMask::INDEX); break;
        case 'w': Serial.println(F("[TEST] DOUBLE_TAP   Tengah"));
                  emit(GestureType::DOUBLE_TAP, FingerMask::MIDDLE); break;
        case 'e': Serial.println(F("[TEST] DOUBLE_TAP   Manis"));
                  emit(GestureType::DOUBLE_TAP, FingerMask::RING); break;
        case 'r': Serial.println(F("[TEST] DOUBLE_TAP   Kelingking"));
                  emit(GestureType::DOUBLE_TAP, FingerMask::PINKY); break;

        // ----------------- Hold (>=3 detik, ditembakkan sekali) -----------------
        case 'a': Serial.println(F("[TEST] HOLD         Telunjuk (drag lock)"));
                  emit(GestureType::HOLD_TRIGGERED, FingerMask::INDEX); break;
        case 's': Serial.println(F("[TEST] HOLD         Tengah (re-center)"));
                  emit(GestureType::HOLD_TRIGGERED, FingerMask::MIDDLE); break;
        case 'd': Serial.println(F("[TEST] HOLD         Manis (TOGGLE MODE)"));
                  emit(GestureType::HOLD_TRIGGERED, FingerMask::RING); break;
        case 'f': Serial.println(F("[TEST] HOLD         Kelingking (pause sensor / Win+Tab)"));
                  emit(GestureType::HOLD_TRIGGERED, FingerMask::PINKY); break;

        // ----------------- Combo (2/3/4 jari) -----------------
        case 'z': Serial.println(F("[TEST] COMBO        Telunjuk+Tengah"));
                  emit(GestureType::COMBO_TAP, (uint8_t)(FingerMask::INDEX | FingerMask::MIDDLE)); break;
        case 'x': Serial.println(F("[TEST] COMBO        Telunjuk+Manis"));
                  emit(GestureType::COMBO_TAP, (uint8_t)(FingerMask::INDEX | FingerMask::RING)); break;
        case 'c': Serial.println(F("[TEST] COMBO        Tengah+Manis"));
                  emit(GestureType::COMBO_TAP, (uint8_t)(FingerMask::MIDDLE | FingerMask::RING)); break;
        case 'v': Serial.println(F("[TEST] COMBO        Tengah+Kelingking"));
                  emit(GestureType::COMBO_TAP, (uint8_t)(FingerMask::MIDDLE | FingerMask::PINKY)); break;
        case 'b': Serial.println(F("[TEST] COMBO        Telunjuk+Tengah+Manis"));
                  emit(GestureType::COMBO_TAP, (uint8_t)(FingerMask::INDEX | FingerMask::MIDDLE | FingerMask::RING)); break;
        case 'n': Serial.println(F("[TEST] COMBO        Telunjuk+Tengah+Kelingking"));
                  emit(GestureType::COMBO_TAP, (uint8_t)(FingerMask::INDEX | FingerMask::MIDDLE | FingerMask::PINKY)); break;
        case 'm': Serial.println(F("[TEST] COMBO        4 Jari (ALL)"));
                  emit(GestureType::COMBO_TAP, FingerMask::ALL); break;

        // ----------------- Gerak mouse manual (bypass MPU9250 sepenuhnya) -----------------
        case 'i': if (ble) ble->moveMouse(0, -10); Serial.println(F("[TEST] Mouse bergerak ke ATAS"));   break;
        case 'k': if (ble) ble->moveMouse(0,  10); Serial.println(F("[TEST] Mouse bergerak ke BAWAH"));  break;
        case 'j': if (ble) ble->moveMouse(-10, 0); Serial.println(F("[TEST] Mouse bergerak ke KIRI"));   break;
        case 'l': if (ble) ble->moveMouse( 10, 0); Serial.println(F("[TEST] Mouse bergerak ke KANAN"));  break;

        // ----------------- Util -----------------
        case 'p':
            Serial.print(F("[TEST] Status BLE: "));
            Serial.println((ble && ble->isConnected()) ? F("CONNECTED") : F("belum terhubung"));
            Serial.print(F("[TEST] Mode saat ini: "));
            Serial.println(actionMapper && actionMapper->getMode() == OperationMode::MODE_1_NAVIGATION ? "1" : "2");
            break;

        case 'h':
        case '?':
            printHelp();
            break;

        default:
            Serial.print(F("[TEST] Perintah tidak dikenal: '"));
            Serial.print(c);
            Serial.println(F("' - ketik 'h' untuk bantuan."));
            break;
    }
}

void DebugConsole::printHelp() {
    Serial.println(F("========================================================"));
    Serial.println(F(" MODE TEST SERIAL - Air Mouse (tanpa MPU9250 / sensor)"));
    Serial.println(F(" Ketik satu karakter di Serial Monitor lalu tekan Enter:"));
    Serial.println(F("--------------------------------------------------------"));
    Serial.println(F(" 1/2/3/4  Single Tap   Telunjuk/Tengah/Manis/Kelingking"));
    Serial.println(F(" q/w/e/r  Double Tap   Telunjuk/Tengah/Manis/Kelingking"));
    Serial.println(F(" a/s/d/f  Hold 3 detik Telunjuk/Tengah/Manis/Kelingking"));
    Serial.println(F(" z  Combo Telunjuk+Tengah        (Ctrl+C / Volume Up)"));
    Serial.println(F(" x  Combo Telunjuk+Manis         (Ctrl+V / Volume Down)"));
    Serial.println(F(" c  Combo Tengah+Manis           (Zoom In)"));
    Serial.println(F(" v  Combo Tengah+Kelingking      (Zoom Out / Close Window)"));
    Serial.println(F(" b  Combo 3 jari (Tlj+Tgh+Mns)   -> Win+Tab"));
    Serial.println(F(" n  Combo 3 jari (Tlj+Tgh+Klk)   -> Win+D"));
    Serial.println(F(" m  Combo 4 jari (ALL)           -> Win+L"));
    Serial.println(F(" i/k/j/l  Gerak mouse manual: atas/bawah/kiri/kanan"));
    Serial.println(F(" p  Cek status koneksi BLE & mode aktif"));
    Serial.println(F(" h atau ?  Tampilkan menu ini lagi"));
    Serial.println(F("========================================================"));
}
