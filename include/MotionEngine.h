#pragma once
#include <stdint.h>

struct Vec3f {
    float x;
    float y;
    float z;
};

struct MotionParams {
    Vec3f forward;
    Vec3f up;
    bool  axisConfigValid;
    float sensitivityX;
    float sensitivityY;
    bool  invertX;
    bool  invertY;
    float deadzoneDps;
    float accelStartDps;
    float accelFullDps;
    float accelMaxGain;
    float smoothTauSlowSec;
    float smoothTauFastSec;
    float smoothLowDps;
    float smoothHighDps;
    float maxSpeedPxPerSec;
    float tiltAccelTauSec;
    float tiltAccelMinG;
    float tiltAccelMaxG;
    float worldBlendFull;
    float worldBlendZero;
    float stationaryEnterDps;
    float stationaryExitDps;
    float stationaryHoldSec;
    float biasTrackTauSec;

    static MotionParams fromConfig();
};

struct MotionTelemetry {
    Vec3f up;
    Vec3f bias;
    float rateRight;
    float rateDown;
    float gain;
    float worldWeight;
    bool  stationary;
    bool  frozen;
};

class MotionEngine {
public:
    explicit MotionEngine(const MotionParams &params);

    void reseed(const Vec3f &accelG);
    void setGyroBias(const Vec3f &bias);
    Vec3f gyroBias() const { return bias_; }

    void step(const Vec3f &gyroRawDps, const Vec3f &accelG, float dtSec, bool frozen);
    void takeDelta(int16_t &dx, int16_t &dy);

    bool isStationary() const { return stationary_; }
    const MotionTelemetry &telemetry() const { return tel_; }

private:
    MotionParams p_;
    Vec3f up_;
    Vec3f bias_;
    float smoothX_;
    float smoothY_;
    float accX_;
    float accY_;
    float stationaryTime_;
    bool  stationary_;
    MotionTelemetry tel_;

    void propagateGravity(const Vec3f &omegaDps, float dt);
    void correctGravity(const Vec3f &accelG, float dt);
};

class CalibrationAccumulator {
public:
    void reset();
    void add(const Vec3f &gyro, const Vec3f &accel);
    uint32_t count() const { return n_; }
    Vec3f gyroMean() const;
    Vec3f accelMean() const;
    float gyroSigma() const;

private:
    uint32_t n_ = 0;
    float gMean_[3] = {0, 0, 0};
    float gM2_[3]   = {0, 0, 0};
    float aMean_[3] = {0, 0, 0};
};
