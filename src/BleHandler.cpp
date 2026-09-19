#include "BleHandler.h"
#include "Config.h"

// Catatan: library "ESP32-BLE-Combo" menyediakan objek global siap pakai
// `Keyboard`, `Mouse`, dan `bleDevice` (dideklarasikan `extern` di header,
// didefinisikan sekali di dalam library itu sendiri) - sehingga kita TIDAK
// perlu (dan tidak boleh) membuat instance BleKeyboard/BleMouse sendiri.

void BleHandler::begin() {
    // Nama BLE harus di-set SEBELUM Keyboard.begin() dipanggil.
    bleDevice.setDeviceName(BLE_DEVICE_NAME);

    // Keyboard.begin() menginisialisasi SATU perangkat BLE HID gabungan yang
    // sekaligus melayani laporan (report) Mouse dan Media Keys - sesuai
    // desain library combo ini, cukup dipanggil sekali di sini.
    Keyboard.begin();
}

bool BleHandler::isConnected() {
    return bleDevice.isConnected();
}

// ---------------------------------------------------------------------------
// Mouse
// ---------------------------------------------------------------------------
void BleHandler::moveMouse(int16_t dx, int16_t dy) {
    Mouse.move((signed char)dx, (signed char)dy, 0);
}

void BleHandler::mouseScroll(int8_t amount) {
    Mouse.move(0, 0, amount);
}

void BleHandler::mouseClick(uint8_t button) {
    Mouse.click(button);
}

void BleHandler::mousePress(uint8_t button) {
    Mouse.press(button);
}

void BleHandler::mouseRelease(uint8_t button) {
    Mouse.release(button);
}

bool BleHandler::isMouseButtonPressed(uint8_t button) {
    return Mouse.isPressed(button);
}

// ---------------------------------------------------------------------------
// Keyboard (primitif mentah)
// ---------------------------------------------------------------------------
void BleHandler::tapKey(uint8_t key) {
    Keyboard.write(key);
}

void BleHandler::pressKey(uint8_t key) {
    Keyboard.press(key);
}

void BleHandler::releaseKey(uint8_t key) {
    Keyboard.release(key);
}

void BleHandler::releaseAllKeys() {
    Keyboard.releaseAll();
}

// ---------------------------------------------------------------------------
// Media / Consumer Control
// ---------------------------------------------------------------------------
void BleHandler::mediaPlayPause()     { Keyboard.write(KEY_MEDIA_PLAY_PAUSE); }
void BleHandler::mediaMute()          { Keyboard.write(KEY_MEDIA_MUTE); }
void BleHandler::mediaNextTrack()     { Keyboard.write(KEY_MEDIA_NEXT_TRACK); }
void BleHandler::mediaPreviousTrack() { Keyboard.write(KEY_MEDIA_PREVIOUS_TRACK); }
void BleHandler::mediaVolumeUp()      { Keyboard.write(KEY_MEDIA_VOLUME_UP); }
void BleHandler::mediaVolumeDown()    { Keyboard.write(KEY_MEDIA_VOLUME_DOWN); }
