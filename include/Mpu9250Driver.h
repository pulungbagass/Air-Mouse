#pragma once
#include <Arduino.h>
#include <Wire.h>

struct ImuSample {
    float ax = 0.0f;
    float ay = 0.0f;
    float az = 0.0f;
    float gx = 0.0f;
    float gy = 0.0f;
    float gz = 0.0f;
    int16_t raw[6] = {0, 0, 0, 0, 0, 0};
};

class Mpu9250Driver {
public:
    bool begin(TwoWire &wire, uint8_t address);
    bool read(ImuSample &out);

    uint8_t address() const { return address_; }
    uint8_t whoAmI() const { return whoAmI_; }
    bool magnetometerPresent() const { return magPresent_; }
    const char *chipName() const;
    const char *lastError() const { return error_; }

private:
    TwoWire *wire_ = nullptr;
    uint8_t address_ = 0;
    uint8_t whoAmI_ = 0;
    bool magPresent_ = false;
    const char *error_ = "";

    bool writeReg(uint8_t reg, uint8_t value);
    bool readReg(uint8_t reg, uint8_t &value);
    bool readRegs(uint8_t reg, uint8_t *buffer, uint8_t length);
    bool writeVerified(uint8_t reg, uint8_t value);
    bool isAcceptedWhoAmI(uint8_t id) const;
    bool waitResetComplete();
    bool validateStream();
};
