#include "MotionEngine.h"
#include "Config.h"
#include <math.h>

namespace {
    constexpr float DEG_TO_RAD_F = 0.01745329252f;
    constexpr float ACC_LIMIT_PX = 2000.0f;
    constexpr float SMOOTH_EPS   = 0.02f;

    inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
    inline float smooth01(float t) { t = clampf(t, 0.0f, 1.0f); return t * t * (3.0f - 2.0f * t); }

    inline Vec3f add(const Vec3f &a, const Vec3f &b) { return Vec3f{a.x + b.x, a.y + b.y, a.z + b.z}; }
    inline Vec3f sub(const Vec3f &a, const Vec3f &b) { return Vec3f{a.x - b.x, a.y - b.y, a.z - b.z}; }
    inline Vec3f mul(const Vec3f &a, float s) { return Vec3f{a.x * s, a.y * s, a.z * s}; }
    inline float dot(const Vec3f &a, const Vec3f &b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
    inline Vec3f cross(const Vec3f &a, const Vec3f &b) {
        return Vec3f{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
    }
    inline float norm(const Vec3f &a) { return sqrtf(dot(a, a)); }

    Vec3f axisVector(int selector) {
        switch (selector) {
            case AXIS_PLUS_X:  return Vec3f{ 1.0f, 0.0f, 0.0f};
            case AXIS_MINUS_X: return Vec3f{-1.0f, 0.0f, 0.0f};
            case AXIS_PLUS_Y:  return Vec3f{ 0.0f, 1.0f, 0.0f};
            case AXIS_MINUS_Y: return Vec3f{ 0.0f,-1.0f, 0.0f};
            case AXIS_PLUS_Z:  return Vec3f{ 0.0f, 0.0f, 1.0f};
            default:           return Vec3f{ 0.0f, 0.0f,-1.0f};
        }
    }
}

MotionParams MotionParams::fromConfig() {
    MotionParams p;
    p.forward = axisVector(MPU_AXIS_FORWARD);
    p.up      = axisVector(MPU_AXIS_UP);
    p.axisConfigValid = fabsf(dot(p.forward, p.up)) < 0.5f;
    if (!p.axisConfigValid) {
        p.forward = axisVector(AXIS_PLUS_X);
        p.up      = axisVector(AXIS_PLUS_Z);
    }
    p.sensitivityX     = MOUSE_SENSITIVITY;
    p.sensitivityY     = MOUSE_SENSITIVITY * MOUSE_Y_RATIO;
    p.invertX          = CURSOR_INVERT_X != 0;
    p.invertY          = CURSOR_INVERT_Y != 0;
    p.deadzoneDps      = GYRO_DEADZONE_DPS;
    p.accelStartDps    = CURSOR_ACCEL_START_DPS;
    p.accelFullDps     = CURSOR_ACCEL_FULL_DPS;
    p.accelMaxGain     = CURSOR_ACCEL_MAX_GAIN;
    p.smoothTauSlowSec = CURSOR_SMOOTH_TAU_SLOW_MS / 1000.0f;
    p.smoothTauFastSec = CURSOR_SMOOTH_TAU_FAST_MS / 1000.0f;
    p.smoothLowDps     = CURSOR_SMOOTH_LOW_DPS;
    p.smoothHighDps    = CURSOR_SMOOTH_HIGH_DPS;
    p.maxSpeedPxPerSec = CURSOR_MAX_SPEED_PX_S;
    p.tiltAccelTauSec  = TILT_ACCEL_TAU_S;
    p.tiltAccelMinG    = TILT_ACCEL_MIN_G;
    p.tiltAccelMaxG    = TILT_ACCEL_MAX_G;
    p.worldBlendFull   = POINT_WORLD_BLEND_FULL;
    p.worldBlendZero   = POINT_WORLD_BLEND_ZERO;
    p.stationaryEnterDps = GYRO_STATIONARY_ENTER_DPS;
    p.stationaryExitDps  = GYRO_STATIONARY_EXIT_DPS;
    p.stationaryHoldSec  = GYRO_STATIONARY_HOLD_MS / 1000.0f;
    p.biasTrackTauSec    = GYRO_BIAS_TRACK_TAU_S;
    return p;
}

MotionEngine::MotionEngine(const MotionParams &params)
    : p_(params), up_(params.up), bias_{0.0f, 0.0f, 0.0f},
      smoothX_(0.0f), smoothY_(0.0f), accX_(0.0f), accY_(0.0f),
      stationaryTime_(0.0f), stationary_(false), tel_() {
    tel_.up = up_;
    tel_.bias = bias_;
}

void MotionEngine::reseed(const Vec3f &accelG) {
    const float n = norm(accelG);
    up_ = (n > 0.5f && n < 1.5f) ? mul(accelG, 1.0f / n) : p_.up;
    smoothX_ = smoothY_ = 0.0f;
    accX_ = accY_ = 0.0f;
    stationaryTime_ = 0.0f;
    stationary_ = false;
}

void MotionEngine::setGyroBias(const Vec3f &bias) {
    bias_ = bias;
}

void MotionEngine::propagateGravity(const Vec3f &omegaDps, float dt) {
    const Vec3f omega = mul(omegaDps, DEG_TO_RAD_F);
    const float rate = norm(omega);
    const float theta = rate * dt;
    if (theta < 1e-6f) return;

    const Vec3f k = mul(omega, 1.0f / rate);
    const float c = cosf(theta);
    const float s = sinf(theta);
    const Vec3f kxv = cross(k, up_);
    const float kdv = dot(k, up_);
    up_ = add(sub(mul(up_, c), mul(kxv, s)), mul(k, kdv * (1.0f - c)));
}

void MotionEngine::correctGravity(const Vec3f &accelG, float dt) {
    const float an = norm(accelG);
    if (an > p_.tiltAccelMinG && an < p_.tiltAccelMaxG) {
        const float k = dt / (p_.tiltAccelTauSec + dt);
        const Vec3f measured = mul(accelG, 1.0f / an);
        up_ = add(mul(up_, 1.0f - k), mul(measured, k));
    }
    const float un = norm(up_);
    if (un < 1e-3f) {
        up_ = p_.up;
    } else {
        up_ = mul(up_, 1.0f / un);
    }
}

void MotionEngine::step(const Vec3f &gyroRawDps, const Vec3f &accelG, float dt, bool frozen) {
    Vec3f w = sub(gyroRawDps, bias_);
    const float wmag = norm(w);

    if (stationary_) {
        if (wmag > p_.stationaryExitDps) {
            stationary_ = false;
            stationaryTime_ = 0.0f;
        }
    } else if (wmag < p_.stationaryEnterDps) {
        stationaryTime_ += dt;
        if (stationaryTime_ >= p_.stationaryHoldSec) stationary_ = true;
    } else {
        stationaryTime_ = 0.0f;
    }

    if (stationary_) {
        const float k = dt / (p_.biasTrackTauSec + dt);
        bias_ = add(bias_, mul(sub(gyroRawDps, bias_), k));
        w = sub(gyroRawDps, bias_);
    }

    propagateGravity(w, dt);
    correctGravity(accelG, dt);

    const Vec3f f = p_.forward;
    const Vec3f fdot = cross(w, f);

    const Vec3f leftDevice = cross(p_.up, f);
    float vLeft = dot(fdot, leftDevice);
    float vUp   = dot(fdot, p_.up);

    const Vec3f lateral = cross(up_, f);
    const float lateralNorm = norm(lateral);
    const float span = p_.worldBlendFull - p_.worldBlendZero;
    const float weight = span > 1e-4f ? smooth01((lateralNorm - p_.worldBlendZero) / span) : 1.0f;

    if (weight > 0.0f && lateralNorm > 1e-4f) {
        const Vec3f lateralUnit = mul(lateral, 1.0f / lateralNorm);
        const Vec3f meridian = mul(sub(up_, mul(f, dot(up_, f))), 1.0f / lateralNorm);
        const float vLeftWorld = dot(fdot, lateralUnit);
        const float vUpWorld   = dot(fdot, meridian);
        vLeft = weight * vLeftWorld + (1.0f - weight) * vLeft;
        vUp   = weight * vUpWorld   + (1.0f - weight) * vUp;
    }

    float rx = -vLeft;
    float ry = -vUp;
    if (p_.invertX) rx = -rx;
    if (p_.invertY) ry = -ry;

    tel_.up = up_;
    tel_.bias = bias_;
    tel_.rateRight = rx;
    tel_.rateDown = ry;
    tel_.worldWeight = weight;
    tel_.stationary = stationary_;
    tel_.frozen = frozen;

    if (frozen || stationary_) {
        smoothX_ = smoothY_ = 0.0f;
        accX_ = accY_ = 0.0f;
        tel_.gain = 1.0f;
        return;
    }

    float mag = sqrtf(rx * rx + ry * ry);
    if (mag <= p_.deadzoneDps) {
        rx = ry = 0.0f;
        mag = 0.0f;
    } else {
        const float s = (mag - p_.deadzoneDps) / mag;
        rx *= s;
        ry *= s;
        mag -= p_.deadzoneDps;
    }

    const float lowHighSpan = p_.smoothHighDps - p_.smoothLowDps;
    const float speedT = lowHighSpan > 1e-4f ? smooth01((mag - p_.smoothLowDps) / lowHighSpan) : 1.0f;
    const float tau = p_.smoothTauSlowSec + (p_.smoothTauFastSec - p_.smoothTauSlowSec) * speedT;
    const float alpha = dt / (tau + dt);
    smoothX_ += alpha * (rx - smoothX_);
    smoothY_ += alpha * (ry - smoothY_);
    if (fabsf(smoothX_) < SMOOTH_EPS) smoothX_ = 0.0f;
    if (fabsf(smoothY_) < SMOOTH_EPS) smoothY_ = 0.0f;

    const float smoothMag = sqrtf(smoothX_ * smoothX_ + smoothY_ * smoothY_);
    const float accelSpan = p_.accelFullDps - p_.accelStartDps;
    const float accelT = accelSpan > 1e-4f ? smooth01((smoothMag - p_.accelStartDps) / accelSpan) : 1.0f;
    const float gain = 1.0f + (p_.accelMaxGain - 1.0f) * accelT;
    tel_.gain = gain;

    float pxX = smoothX_ * gain * p_.sensitivityX;
    float pxY = smoothY_ * gain * p_.sensitivityY;
    const float speed = sqrtf(pxX * pxX + pxY * pxY);
    if (speed > p_.maxSpeedPxPerSec) {
        const float limit = p_.maxSpeedPxPerSec / speed;
        pxX *= limit;
        pxY *= limit;
    }

    accX_ = clampf(accX_ + pxX * dt, -ACC_LIMIT_PX, ACC_LIMIT_PX);
    accY_ = clampf(accY_ + pxY * dt, -ACC_LIMIT_PX, ACC_LIMIT_PX);
}

void MotionEngine::takeDelta(int16_t &dx, int16_t &dy) {
    const int32_t ix = static_cast<int32_t>(accX_);
    const int32_t iy = static_cast<int32_t>(accY_);
    accX_ -= static_cast<float>(ix);
    accY_ -= static_cast<float>(iy);
    dx = static_cast<int16_t>(clampf(static_cast<float>(ix), -32000.0f, 32000.0f));
    dy = static_cast<int16_t>(clampf(static_cast<float>(iy), -32000.0f, 32000.0f));
}

void CalibrationAccumulator::reset() {
    n_ = 0;
    for (uint8_t i = 0; i < 3; i++) {
        gMean_[i] = 0.0f;
        gM2_[i] = 0.0f;
        aMean_[i] = 0.0f;
    }
}

void CalibrationAccumulator::add(const Vec3f &gyro, const Vec3f &accel) {
    const float g[3] = {gyro.x, gyro.y, gyro.z};
    const float a[3] = {accel.x, accel.y, accel.z};
    n_++;
    const float inv = 1.0f / static_cast<float>(n_);
    for (uint8_t i = 0; i < 3; i++) {
        const float delta = g[i] - gMean_[i];
        gMean_[i] += delta * inv;
        gM2_[i] += delta * (g[i] - gMean_[i]);
        aMean_[i] += (a[i] - aMean_[i]) * inv;
    }
}

Vec3f CalibrationAccumulator::gyroMean() const {
    return Vec3f{gMean_[0], gMean_[1], gMean_[2]};
}

Vec3f CalibrationAccumulator::accelMean() const {
    return Vec3f{aMean_[0], aMean_[1], aMean_[2]};
}

float CalibrationAccumulator::gyroSigma() const {
    if (n_ < 2) return 1e6f;
    const float inv = 1.0f / static_cast<float>(n_);
    return sqrtf((gM2_[0] + gM2_[1] + gM2_[2]) * inv);
}
