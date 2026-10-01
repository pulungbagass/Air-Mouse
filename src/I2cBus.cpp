#include "I2cBus.h"

namespace {
    constexpr uint8_t SCAN_FIRST_ADDRESS = 0x08;
    constexpr uint8_t SCAN_LAST_ADDRESS  = 0x77;
    constexpr uint8_t RECOVERY_CLOCKS    = 9;
    constexpr uint8_t RELEASE_WAIT_LOOPS = 40;

    inline void driveLow(int pin) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
    }

    inline void release(int pin) {
        pinMode(pin, INPUT_PULLUP);
    }
}

I2cLineState I2cBus::inspectLines(int sda, int scl) {
    pinMode(sda, INPUT_PULLUP);
    pinMode(scl, INPUT_PULLUP);
    delayMicroseconds(20);
    const bool sdaLow = digitalRead(sda) == LOW;
    const bool sclLow = digitalRead(scl) == LOW;
    if (sdaLow && sclLow) return I2cLineState::BOTH_LOW;
    if (sdaLow) return I2cLineState::SDA_LOW;
    if (sclLow) return I2cLineState::SCL_LOW;
    return I2cLineState::IDLE_OK;
}

bool I2cBus::recover(int sda, int scl) {
    release(sda);
    release(scl);
    delayMicroseconds(20);

    for (uint8_t i = 0; i < RELEASE_WAIT_LOOPS && digitalRead(scl) == LOW; i++) {
        delayMicroseconds(50);
    }

    for (uint8_t i = 0; i < RECOVERY_CLOCKS && digitalRead(sda) == LOW; i++) {
        driveLow(scl);
        delayMicroseconds(6);
        release(scl);
        delayMicroseconds(6);
    }

    driveLow(sda);
    delayMicroseconds(6);
    release(scl);
    delayMicroseconds(6);
    release(sda);
    delayMicroseconds(6);

    return digitalRead(sda) == HIGH && digitalRead(scl) == HIGH;
}

bool I2cBus::begin(int sda, int scl, uint32_t clockHz, uint16_t timeoutMs) {
    if (started_) end();
    sda_ = sda;
    scl_ = scl;
    timeoutMs_ = timeoutMs;
    if (!Wire.begin(sda, scl, clockHz)) return false;
    Wire.setTimeOut(timeoutMs);
    started_ = true;
    return true;
}

void I2cBus::end() {
    if (!started_) return;
    Wire.end();
    started_ = false;
}

void I2cBus::setClock(uint32_t clockHz) {
    if (started_) Wire.setClock(clockHz);
}

bool I2cBus::present(uint8_t address) {
    if (!started_) return false;
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
}

uint8_t I2cBus::scan(uint8_t *found, uint8_t capacity) {
    if (!started_) return 0;
    uint8_t count = 0;
    for (uint8_t address = SCAN_FIRST_ADDRESS; address <= SCAN_LAST_ADDRESS; address++) {
        if (present(address)) {
            if (count < capacity) found[count] = address;
            count++;
        }
    }
    return count < capacity ? count : capacity;
}

const char *I2cBus::describe(I2cLineState state) {
    switch (state) {
        case I2cLineState::IDLE_OK:  return "SDA/SCL HIGH (normal)";
        case I2cLineState::SDA_LOW:  return "SDA tertahan LOW";
        case I2cLineState::SCL_LOW:  return "SCL tertahan LOW";
        case I2cLineState::BOTH_LOW: return "SDA dan SCL tertahan LOW";
    }
    return "tidak diketahui";
}

const char *I2cBus::describeError(uint8_t wireError) {
    switch (wireError) {
        case 0: return "OK";
        case 1: return "data terlalu panjang";
        case 2: return "NACK saat alamat";
        case 3: return "NACK saat data";
        case 4: return "error lain";
        case 5: return "timeout";
        default: return "tidak diketahui";
    }
}
