#pragma once
#include <Arduino.h>
#include <Wire.h>

enum class I2cLineState : uint8_t {
    IDLE_OK = 0,
    SDA_LOW,
    SCL_LOW,
    BOTH_LOW
};

class I2cBus {
public:
    bool begin(int sda, int scl, uint32_t clockHz, uint16_t timeoutMs);
    void end();
    void setClock(uint32_t clockHz);
    bool isStarted() const { return started_; }
    bool present(uint8_t address);
    uint8_t scan(uint8_t *found, uint8_t capacity);
    TwoWire &wire() { return Wire; }

    static I2cLineState inspectLines(int sda, int scl);
    static bool recover(int sda, int scl);
    static const char *describe(I2cLineState state);
    static const char *describeError(uint8_t wireError);

private:
    bool started_ = false;
    int sda_ = -1;
    int scl_ = -1;
    uint16_t timeoutMs_ = 20;
};
