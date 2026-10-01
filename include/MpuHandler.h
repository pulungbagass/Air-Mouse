#pragma once
#include <Arduino.h>
#include "I2cBus.h"
#include "Mpu9250Driver.h"
#include "MotionEngine.h"

struct MouseDelta {
    int16_t dx = 0;
    int16_t dy = 0;
};

enum class MpuState : uint8_t {
    SEARCHING = 0,
    CALIBRATING,
    RUNNING
};

class MpuHandler {
public:
    MpuHandler();

    void begin();
    bool update(MouseDelta &out, bool motionFrozen);

    void startRecenter();
    bool isRecentering() const;
    void resetTimer();
    bool isReady() const { return state_ == MpuState::RUNNING; }
    MpuState state() const { return state_; }

    void forceReinit();
    void runI2cScan();
    void printStatus();
    void printTelemetry();

private:
    I2cBus bus_;
    Mpu9250Driver imu_;
    MotionEngine engine_;
    CalibrationAccumulator calib_;
    MpuState state_ = MpuState::SEARCHING;

    uint16_t attempt_ = 0;
    uint16_t recoveries_ = 0;
    uint32_t nextAttemptMs_ = 0;
    uint32_t clockHz_ = 0;

    uint8_t failCount_ = 0;
    uint16_t staleCount_ = 0;
    int16_t lastRaw_[6] = {0, 0, 0, 0, 0, 0};

    bool resync_ = true;
    uint32_t lastMicros_ = 0;
    uint32_t lastReportMs_ = 0;
    ImuSample lastSample_;

    bool calibBoot_ = true;
    uint32_t calibStartMs_ = 0;
    uint8_t calibAttempts_ = 0;
    float bestSigma_ = 1e6f;
    Vec3f bestBias_ = {0.0f, 0.0f, 0.0f};
    Vec3f bestAccel_ = {0.0f, 0.0f, 1.0f};

    void attemptInit();
    bool tryInitialize();
    bool raiseClock();
    void beginCalibration(bool boot);
    void runCalibration(uint32_t nowMs);
    void finishCalibrationWindow();
    void acceptCalibration(const Vec3f &bias, const Vec3f &accel);
    bool runMotion(MouseDelta &out, bool frozen, uint32_t nowMs);
    bool readSample(ImuSample &sample);
    void beginRecovery(const char *reason);
    const char *stateName() const;
};
