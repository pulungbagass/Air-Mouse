#include "BleHidDevice.h"
#include "Config.h"
#include "RtosUtil.h"
#include <Preferences.h>

extern "C" int ble_svc_gap_device_appearance_set(uint16_t appearance);

namespace {
    constexpr uint8_t REPORT_ID_MOUSE    = 1;
    constexpr uint8_t REPORT_ID_KEYBOARD = 2;
    constexpr uint8_t REPORT_ID_CONSUMER = 3;

    const uint8_t kReportMap[] = {
        USAGE_PAGE(1), 0x01,
        USAGE(1), 0x02,
        COLLECTION(1), 0x01,
        REPORT_ID(1), REPORT_ID_MOUSE,
        USAGE(1), 0x01,
        COLLECTION(1), 0x00,
        USAGE_PAGE(1), 0x09,
        USAGE_MINIMUM(1), 0x01,
        USAGE_MAXIMUM(1), 0x05,
        LOGICAL_MINIMUM(1), 0x00,
        LOGICAL_MAXIMUM(1), 0x01,
        REPORT_SIZE(1), 0x01,
        REPORT_COUNT(1), 0x05,
        HIDINPUT(1), 0x02,
        REPORT_SIZE(1), 0x03,
        REPORT_COUNT(1), 0x01,
        HIDINPUT(1), 0x01,
        USAGE_PAGE(1), 0x01,
        USAGE(1), 0x30,
        USAGE(1), 0x31,
        USAGE(1), 0x38,
        LOGICAL_MINIMUM(1), 0x81,
        LOGICAL_MAXIMUM(1), 0x7F,
        REPORT_SIZE(1), 0x08,
        REPORT_COUNT(1), 0x03,
        HIDINPUT(1), 0x06,
        USAGE_PAGE(1), 0x0C,
        USAGE(2), 0x38, 0x02,
        LOGICAL_MINIMUM(1), 0x81,
        LOGICAL_MAXIMUM(1), 0x7F,
        REPORT_SIZE(1), 0x08,
        REPORT_COUNT(1), 0x01,
        HIDINPUT(1), 0x06,
        END_COLLECTION(0),
        END_COLLECTION(0),

        USAGE_PAGE(1), 0x01,
        USAGE(1), 0x06,
        COLLECTION(1), 0x01,
        REPORT_ID(1), REPORT_ID_KEYBOARD,
        USAGE_PAGE(1), 0x07,
        USAGE_MINIMUM(1), 0xE0,
        USAGE_MAXIMUM(1), 0xE7,
        LOGICAL_MINIMUM(1), 0x00,
        LOGICAL_MAXIMUM(1), 0x01,
        REPORT_SIZE(1), 0x01,
        REPORT_COUNT(1), 0x08,
        HIDINPUT(1), 0x02,
        REPORT_COUNT(1), 0x01,
        REPORT_SIZE(1), 0x08,
        HIDINPUT(1), 0x01,
        REPORT_COUNT(1), 0x05,
        REPORT_SIZE(1), 0x01,
        USAGE_PAGE(1), 0x08,
        USAGE_MINIMUM(1), 0x01,
        USAGE_MAXIMUM(1), 0x05,
        HIDOUTPUT(1), 0x02,
        REPORT_COUNT(1), 0x01,
        REPORT_SIZE(1), 0x03,
        HIDOUTPUT(1), 0x01,
        REPORT_COUNT(1), 0x06,
        REPORT_SIZE(1), 0x08,
        LOGICAL_MINIMUM(1), 0x00,
        LOGICAL_MAXIMUM(1), 0x65,
        USAGE_PAGE(1), 0x07,
        USAGE_MINIMUM(1), 0x00,
        USAGE_MAXIMUM(1), 0x65,
        HIDINPUT(1), 0x00,
        END_COLLECTION(0),

        USAGE_PAGE(1), 0x0C,
        USAGE(1), 0x01,
        COLLECTION(1), 0x01,
        REPORT_ID(1), REPORT_ID_CONSUMER,
        LOGICAL_MINIMUM(1), 0x00,
        LOGICAL_MAXIMUM(2), 0xFF, 0x03,
        USAGE_MINIMUM(1), 0x00,
        USAGE_MAXIMUM(2), 0xFF, 0x03,
        REPORT_SIZE(1), 0x10,
        REPORT_COUNT(1), 0x01,
        HIDINPUT(1), 0x00,
        END_COLLECTION(0)
    };

    bool asciiToUsage(uint8_t c, uint8_t &usage, bool &shift) {
        shift = false;
        if (c >= 'a' && c <= 'z') { usage = 0x04 + (c - 'a'); return true; }
        if (c >= 'A' && c <= 'Z') { usage = 0x04 + (c - 'A'); shift = true; return true; }
        if (c >= '1' && c <= '9') { usage = 0x1E + (c - '1'); return true; }
        if (c == '0') { usage = 0x27; return true; }
        if (c == ' ') { usage = 0x2C; return true; }
        return false;
    }

    inline int8_t clampStep(int32_t v) {
        return static_cast<int8_t>(v > 127 ? 127 : (v < -127 ? -127 : v));
    }
}

void BleHidDevice::applyProfileRevision() {
    Preferences prefs;
    if (!prefs.begin("airmouse", false)) return;
    if (prefs.getUChar("bleprof", 0) != BLE_PROFILE_REVISION) {
        NimBLEDevice::deleteAllBonds();
        prefs.putUChar("bleprof", BLE_PROFILE_REVISION);
        Serial.println(F("[BLE] Profil HID baru: bond lama dihapus. Hapus juga 'Air Mouse' di daftar Bluetooth host lalu pairing ulang."));
    }
    prefs.end();
}

void BleHidDevice::begin() {
    NimBLEDevice::init(BLE_DEVICE_NAME);
    ble_svc_gap_device_appearance_set(BLE_APPEARANCE_MOUSE);
    NimBLEDevice::setSecurityAuth(true, false, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
    applyProfileRevision();

    server_ = NimBLEDevice::createServer();
    server_->setCallbacks(this);
    server_->advertiseOnDisconnect(true);

    hid_ = new NimBLEHIDDevice(server_);
    inMouse_     = hid_->inputReport(REPORT_ID_MOUSE);
    inKeyboard_  = hid_->inputReport(REPORT_ID_KEYBOARD);
    outKeyboard_ = hid_->outputReport(REPORT_ID_KEYBOARD);
    inConsumer_  = hid_->inputReport(REPORT_ID_CONSUMER);

    hid_->manufacturer(std::string(BLE_MANUFACTURER_NAME));
    hid_->pnp(BLE_PNP_VENDOR_ID_SOURCE, BLE_PNP_VENDOR_ID, BLE_PNP_PRODUCT_ID, BLE_PNP_VERSION);
    hid_->hidInfo(0x00, 0x01);
    hid_->reportMap(const_cast<uint8_t *>(kReportMap), sizeof(kReportMap));
    hid_->startServices();
    hid_->setBatteryLevel(100);

    NimBLEAdvertising *advertising = server_->getAdvertising();
    advertising->setAppearance(BLE_APPEARANCE_MOUSE);
    advertising->addServiceUUID(hid_->hidService()->getUUID());
    advertising->setScanResponse(false);
    advertising->setMinInterval(BLE_ADV_INTERVAL_MIN);
    advertising->setMaxInterval(BLE_ADV_INTERVAL_MAX);
    advertising->start();
}

void BleHidDevice::clearBonds() {
    NimBLEDevice::deleteAllBonds();
    Serial.println(F("[BLE] Semua bond dihapus."));
}

void BleHidDevice::onConnect(NimBLEServer *server, ble_gap_conn_desc *desc) {
    connected_.store(true, std::memory_order_relaxed);
    server->updateConnParams(desc->conn_handle, BLE_CONN_INTERVAL_MIN, BLE_CONN_INTERVAL_MAX,
                             BLE_CONN_LATENCY, BLE_CONN_TIMEOUT);
}

void BleHidDevice::onDisconnect(NimBLEServer *server, ble_gap_conn_desc *desc) {
    (void)server;
    (void)desc;
    connected_.store(false, std::memory_order_relaxed);
    resetRequested_.store(true, std::memory_order_relaxed);
}

void BleHidDevice::serviceHousekeeping() {
    if (resetRequested_.exchange(false, std::memory_order_relaxed)) {
        mouseButtons_ = 0;
        for (uint8_t i = 0; i < 8; i++) reinterpret_cast<uint8_t *>(&keyReport_)[i] = 0;
    }
}

void BleHidDevice::sendMouse(int8_t x, int8_t y, int8_t wheel, int8_t pan) {
    if (!isConnected()) return;
    const uint8_t report[5] = {mouseButtons_, static_cast<uint8_t>(x), static_cast<uint8_t>(y),
                               static_cast<uint8_t>(wheel), static_cast<uint8_t>(pan)};
    inMouse_->setValue(report, sizeof(report));
    inMouse_->notify();
}

void BleHidDevice::sendKeyboard() {
    if (!isConnected()) return;
    inKeyboard_->setValue(reinterpret_cast<const uint8_t *>(&keyReport_), sizeof(keyReport_));
    inKeyboard_->notify();
    vTaskDelay(msToTicks(BLE_KEY_REPORT_GAP_MS));
}

void BleHidDevice::sendConsumer(uint16_t usage) {
    if (!isConnected()) return;
    const uint8_t report[2] = {static_cast<uint8_t>(usage & 0xFF), static_cast<uint8_t>(usage >> 8)};
    inConsumer_->setValue(report, sizeof(report));
    inConsumer_->notify();
    vTaskDelay(msToTicks(BLE_KEY_REPORT_GAP_MS));
}

void BleHidDevice::mouseMove(int32_t dx, int32_t dy, int8_t wheel, int8_t pan) {
    bool first = true;
    while (first || dx != 0 || dy != 0) {
        const int8_t sx = clampStep(dx);
        const int8_t sy = clampStep(dy);
        sendMouse(sx, sy, first ? wheel : 0, first ? pan : 0);
        dx -= sx;
        dy -= sy;
        if (dx != 0 || dy != 0) vTaskDelay(msToTicks(BLE_MOUSE_SPLIT_GAP_MS));
        first = false;
    }
}

void BleHidDevice::mousePress(uint8_t button) {
    mouseButtons_ |= button;
    sendMouse(0, 0, 0, 0);
}

void BleHidDevice::mouseRelease(uint8_t button) {
    mouseButtons_ &= static_cast<uint8_t>(~button);
    sendMouse(0, 0, 0, 0);
}

void BleHidDevice::mouseClick(uint8_t button) {
    mousePress(button);
    vTaskDelay(msToTicks(BLE_CLICK_GAP_MS));
    mouseRelease(button);
}

bool BleHidDevice::resolveKey(uint8_t key, uint8_t &usage, uint8_t &modifierBits) const {
    usage = 0;
    modifierBits = 0;
    if (key >= 0x88) {
        usage = static_cast<uint8_t>(key - 136);
        return true;
    }
    if (key >= 0x80) {
        modifierBits = static_cast<uint8_t>(1u << (key - 0x80));
        return true;
    }
    bool shift = false;
    if (!asciiToUsage(key, usage, shift)) return false;
    if (shift) modifierBits = 0x02;
    return true;
}

void BleHidDevice::keyPress(uint8_t key) {
    uint8_t usage, modifierBits;
    if (!resolveKey(key, usage, modifierBits)) return;
    keyReport_.modifiers |= modifierBits;
    if (usage != 0) {
        bool present = false;
        int8_t freeSlot = -1;
        for (int8_t i = 0; i < 6; i++) {
            if (keyReport_.keys[i] == usage) present = true;
            if (keyReport_.keys[i] == 0 && freeSlot < 0) freeSlot = i;
        }
        if (!present && freeSlot >= 0) keyReport_.keys[freeSlot] = usage;
    }
    sendKeyboard();
}

void BleHidDevice::keyRelease(uint8_t key) {
    uint8_t usage, modifierBits;
    if (!resolveKey(key, usage, modifierBits)) return;
    keyReport_.modifiers &= static_cast<uint8_t>(~modifierBits);
    if (usage != 0) {
        for (uint8_t i = 0; i < 6; i++) {
            if (keyReport_.keys[i] == usage) keyReport_.keys[i] = 0;
        }
    }
    sendKeyboard();
}

void BleHidDevice::keyWrite(uint8_t key) {
    keyPress(key);
    keyRelease(key);
}

void BleHidDevice::keyReleaseAll() {
    keyReport_.modifiers = 0;
    for (uint8_t i = 0; i < 6; i++) keyReport_.keys[i] = 0;
    sendKeyboard();
}

void BleHidDevice::consumerTap(uint16_t usage) {
    sendConsumer(usage);
    sendConsumer(0);
}
