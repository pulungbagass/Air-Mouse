#include "MpuHandler.h"
#include "Config.h"
#include "RtosUtil.h"

namespace {
    constexpr uint8_t MAX_SCAN_RESULTS = 24;
    constexpr uint8_t SCAN_LOG_EVERY   = 5;

    inline Vec3f gyroOf(const ImuSample &s)  { return Vec3f{s.gx, s.gy, s.gz}; }
    inline Vec3f accelOf(const ImuSample &s) { return Vec3f{s.ax, s.ay, s.az}; }

    uint32_t retryDelayMs(uint16_t attempt) {
        static const uint16_t schedule[] = {250, 500, 1000};
        if (attempt >= 1 && attempt <= 3) return schedule[attempt - 1];
        return MPU_RETRY_INTERVAL_MS;
    }
}

MpuHandler::MpuHandler() : engine_(MotionParams::fromConfig()) {}

const char *MpuHandler::stateName() const {
    switch (state_) {
        case MpuState::SEARCHING:   return "SEARCHING";
        case MpuState::CALIBRATING: return "CALIBRATING";
        case MpuState::RUNNING:     return "RUNNING";
    }
    return "?";
}

void MpuHandler::begin() {
    if (!MotionParams::fromConfig().axisConfigValid) {
        Serial.println(F("[MPU] Konfigurasi MPU_AXIS_FORWARD/UP tidak ortogonal, memakai +X/+Z."));
    }
    vTaskDelay(msToTicks(I2C_POWER_SETTLE_MS));
    state_ = MpuState::SEARCHING;
    attempt_ = 0;
    nextAttemptMs_ = millis();
    attemptInit();
}

void MpuHandler::attemptInit() {
    if (tryInitialize()) {
        attempt_ = 0;
        beginCalibration(true);
        return;
    }
    nextAttemptMs_ = millis() + retryDelayMs(attempt_);
}

bool MpuHandler::tryInitialize() {
    attempt_++;
    const bool verbose = (attempt_ == 1) || (attempt_ % SCAN_LOG_EVERY == 0);

    const I2cLineState before = I2cBus::inspectLines(PIN_MPU_SDA, PIN_MPU_SCL);
    const bool released = I2cBus::recover(PIN_MPU_SDA, PIN_MPU_SCL);
    const I2cLineState after = I2cBus::inspectLines(PIN_MPU_SDA, PIN_MPU_SCL);

    if (verbose || before != I2cLineState::IDLE_OK) {
        Serial.printf("[I2C] Percobaan #%u | sebelum: %s | recovery: %s | sesudah: %s\n",
                      attempt_, I2cBus::describe(before), released ? "bus bebas" : "bus TIDAK bebas",
                      I2cBus::describe(after));
    }
    if (after != I2cLineState::IDLE_OK) {
        Serial.println(F("[I2C] Jalur tertahan LOW: periksa VCC 3V3 modul, short SDA/SCL ke GND, atau pull-up."));
        return false;
    }

    if (!bus_.begin(PIN_MPU_SDA, PIN_MPU_SCL, I2C_CLOCK_SAFE_HZ, I2C_TIMEOUT_MS)) {
        Serial.println(F("[I2C] Wire.begin gagal."));
        return false;
    }
    clockHz_ = I2C_CLOCK_SAFE_HZ;

    if (verbose) {
        uint8_t found[MAX_SCAN_RESULTS];
        const uint8_t n = bus_.scan(found, MAX_SCAN_RESULTS);
        Serial.printf("[I2C] Scan SDA=%d SCL=%d: %u perangkat", PIN_MPU_SDA, PIN_MPU_SCL, n);
        for (uint8_t i = 0; i < n; i++) Serial.printf(" 0x%02X", found[i]);
        Serial.println();
    }

    uint8_t address = 0;
    if (bus_.present(MPU_ADDR_PRIMARY)) address = MPU_ADDR_PRIMARY;
    else if (bus_.present(MPU_ADDR_SECONDARY)) address = MPU_ADDR_SECONDARY;

    if (address == 0) {
        Serial.println(F("[MPU] Tidak ada ACK di 0x68/0x69. Cek: VCC 3V3, GND, SDA=GPIO8, SCL=GPIO9 (tidak tertukar), pin AD0, solder header."));
        return false;
    }

    if (!imu_.begin(bus_.wire(), address)) {
        Serial.printf("[MPU] Inisialisasi gagal @0x%02X (WHO_AM_I=0x%02X): %s\n",
                      address, imu_.whoAmI(), imu_.lastError());
        return false;
    }

    Serial.printf("[MPU] Terdeteksi @0x%02X | WHO_AM_I=0x%02X | %s | magnetometer AK8963: %s\n",
                  address, imu_.whoAmI(), imu_.chipName(), imu_.magnetometerPresent() ? "ada" : "tidak ada");

    if (!raiseClock()) {
        Serial.println(F("[I2C] 400 kHz tidak stabil, tetap 100 kHz."));
    }

    ImuSample seed;
    if (imu_.read(seed)) engine_.reseed(accelOf(seed));

    failCount_ = 0;
    staleCount_ = 0;
    resync_ = true;
    return true;
}

bool MpuHandler::raiseClock() {
    if (recoveries_ > 0) return false;
    bus_.setClock(I2C_CLOCK_FAST_HZ);
    ImuSample probe;
    for (uint8_t i = 0; i < 10; i++) {
        if (!imu_.read(probe)) {
            bus_.setClock(I2C_CLOCK_SAFE_HZ);
            return false;
        }
        vTaskDelay(msToTicks(2));
    }
    clockHz_ = I2C_CLOCK_FAST_HZ;
    return true;
}

void MpuHandler::beginCalibration(bool boot) {
    calib_.reset();
    calibBoot_ = boot;
    calibStartMs_ = millis();
    if (boot) {
        calibAttempts_ = 0;
        bestSigma_ = 1e6f;
    }
    state_ = MpuState::CALIBRATING;
    Serial.println(boot ? F("[MPU] Kalibrasi gyro: diamkan perangkat.")
                        : F("[MPU] Re-center: diamkan perangkat sejenak."));
}

void MpuHandler::runCalibration(uint32_t nowMs) {
    ImuSample sample;
    if (!readSample(sample)) return;
    calib_.add(gyroOf(sample), accelOf(sample));

    const uint32_t duration = calibBoot_ ? GYRO_CALIB_DURATION_MS : RECENTER_DURATION_MS;
    if (nowMs - calibStartMs_ >= duration) finishCalibrationWindow();
}

void MpuHandler::finishCalibrationWindow() {
    const float sigma = calib_.gyroSigma();
    const bool enough = calib_.count() >= 20;
    const float limit = calibBoot_ ? GYRO_CALIB_MAX_STD_DPS : GYRO_CALIB_MAX_STD_DPS * 2.0f;

    if (enough && sigma < bestSigma_) {
        bestSigma_ = sigma;
        bestBias_ = calib_.gyroMean();
        bestAccel_ = calib_.accelMean();
    }

    if (enough && sigma <= limit) {
        acceptCalibration(calib_.gyroMean(), calib_.accelMean());
        return;
    }

    if (calibBoot_) {
        calibAttempts_++;
        if (calibAttempts_ >= GYRO_CALIB_MAX_ATTEMPTS && bestSigma_ < 1e5f) {
            Serial.printf("[MPU] Kalibrasi memakai sampel terbaik (sigma=%.2f dps).\n", bestSigma_);
            acceptCalibration(bestBias_, bestAccel_);
            return;
        }
        Serial.printf("[MPU] Perangkat bergerak saat kalibrasi (sigma=%.2f dps), ulangi %u/%u.\n",
                      sigma, calibAttempts_, GYRO_CALIB_MAX_ATTEMPTS);
        calib_.reset();
        calibStartMs_ = millis();
        return;
    }

    Serial.printf("[MPU] Re-center dibatalkan, perangkat bergerak (sigma=%.2f dps).\n", sigma);
    if (enough) engine_.reseed(calib_.accelMean());
    resync_ = true;
    state_ = MpuState::RUNNING;
}

void MpuHandler::acceptCalibration(const Vec3f &bias, const Vec3f &accel) {
    engine_.setGyroBias(bias);
    engine_.reseed(accel);
    resync_ = true;
    state_ = MpuState::RUNNING;
    Serial.printf("[MPU] Siap. Bias gyro = %.3f, %.3f, %.3f dps | I2C %lu Hz\n",
                  bias.x, bias.y, bias.z, static_cast<unsigned long>(clockHz_));
}

bool MpuHandler::readSample(ImuSample &sample) {
    if (!imu_.read(sample)) {
        if (++failCount_ >= MPU_READ_FAIL_LIMIT) beginRecovery("pembacaan I2C gagal berturut-turut");
        return false;
    }
    failCount_ = 0;

    bool identical = true;
    for (uint8_t i = 0; i < 6; i++) {
        if (sample.raw[i] != lastRaw_[i]) identical = false;
        lastRaw_[i] = sample.raw[i];
    }
    staleCount_ = identical ? staleCount_ + 1 : 0;
    if (staleCount_ >= MPU_STALE_SAMPLE_LIMIT) {
        beginRecovery("data sensor beku");
        return false;
    }
    lastSample_ = sample;
    return true;
}

void MpuHandler::beginRecovery(const char *reason) {
    recoveries_++;
    Serial.printf("[MPU] Recovery #%u: %s. Reset bus & inisialisasi ulang.\n", recoveries_, reason);
    bus_.end();
    state_ = MpuState::SEARCHING;
    attempt_ = 0;
    failCount_ = 0;
    staleCount_ = 0;
    nextAttemptMs_ = millis() + 100;
}

bool MpuHandler::update(MouseDelta &out, bool motionFrozen) {
    out.dx = 0;
    out.dy = 0;
    const uint32_t now = millis();

    switch (state_) {
        case MpuState::SEARCHING:
            if (static_cast<int32_t>(now - nextAttemptMs_) >= 0) attemptInit();
            return false;
        case MpuState::CALIBRATING:
            runCalibration(now);
            return false;
        case MpuState::RUNNING:
            return runMotion(out, motionFrozen, now);
    }
    return false;
}

bool MpuHandler::runMotion(MouseDelta &out, bool frozen, uint32_t nowMs) {
    ImuSample sample;
    if (!readSample(sample)) return false;

    const uint32_t nowUs = micros();
    float dt = static_cast<float>(nowUs - lastMicros_) * 1e-6f;
    lastMicros_ = nowUs;

    if (resync_) {
        engine_.reseed(accelOf(sample));
        resync_ = false;
        lastReportMs_ = nowMs;
        return false;
    }
    if (dt <= 0.0f || dt > 0.1f) dt = 0.005f;

    engine_.step(gyroOf(sample), accelOf(sample), dt, frozen);

    if (nowMs - lastReportMs_ < CURSOR_REPORT_INTERVAL_MS) return false;
    lastReportMs_ = nowMs;

    engine_.takeDelta(out.dx, out.dy);
    return out.dx != 0 || out.dy != 0;
}

void MpuHandler::startRecenter() {
    if (state_ != MpuState::RUNNING) return;
    beginCalibration(false);
}

bool MpuHandler::isRecentering() const {
    return state_ == MpuState::CALIBRATING && !calibBoot_;
}

void MpuHandler::resetTimer() {
    resync_ = true;
    lastMicros_ = micros();
}

void MpuHandler::forceReinit() {
    beginRecovery("diminta manual");
}

void MpuHandler::runI2cScan() {
    if (!bus_.isStarted()) {
        I2cBus::recover(PIN_MPU_SDA, PIN_MPU_SCL);
        bus_.begin(PIN_MPU_SDA, PIN_MPU_SCL, I2C_CLOCK_SAFE_HZ, I2C_TIMEOUT_MS);
    }
    uint8_t found[MAX_SCAN_RESULTS];
    const uint8_t n = bus_.scan(found, MAX_SCAN_RESULTS);
    Serial.printf("[I2C] Scan SDA=%d SCL=%d: %u perangkat\n", PIN_MPU_SDA, PIN_MPU_SCL, n);
    for (uint8_t i = 0; i < n; i++) {
        const char *label = "tidak dikenal";
        if (found[i] == 0x68 || found[i] == 0x69) label = "MPU (gyro/accel)";
        else if (found[i] == 0x0C) label = "AK8963 (magnetometer)";
        Serial.printf("  0x%02X  %s\n", found[i], label);
    }
}

void MpuHandler::printStatus() {
    Serial.printf("[MPU] state=%s | addr=0x%02X | WHO_AM_I=0x%02X | %s | mag=%s | I2C=%lu Hz | recovery=%u\n",
                  stateName(), imu_.address(), imu_.whoAmI(), imu_.chipName(),
                  imu_.magnetometerPresent() ? "ya" : "tidak",
                  static_cast<unsigned long>(clockHz_), recoveries_);
    const Vec3f b = engine_.gyroBias();
    Serial.printf("[MPU] bias=%.3f,%.3f,%.3f dps | stationary=%s\n",
                  b.x, b.y, b.z, engine_.isStationary() ? "ya" : "tidak");
}

void MpuHandler::printTelemetry() {
    const MotionTelemetry &t = engine_.telemetry();
    Serial.printf("[IMU] g=%7.2f %7.2f %7.2f dps | a=%5.2f %5.2f %5.2f g | up=%5.2f %5.2f %5.2f | R=%7.2f D=%7.2f | gain=%.2f w=%.2f %s%s\n",
                  lastSample_.gx, lastSample_.gy, lastSample_.gz,
                  lastSample_.ax, lastSample_.ay, lastSample_.az,
                  t.up.x, t.up.y, t.up.z, t.rateRight, t.rateDown, t.gain, t.worldWeight,
                  t.stationary ? "[DIAM]" : "", t.frozen ? "[FREEZE]" : "");
}
