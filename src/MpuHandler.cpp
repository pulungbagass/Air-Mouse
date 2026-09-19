#include "MpuHandler.h"
#include "Config.h"
#include <math.h>

void MpuHandler::begin() {
    Wire.begin(PIN_MPU_SDA, PIN_MPU_SCL);
    Wire.setClock(400000UL); // Fast Mode I2C, cukup untuk sample rate sensor ini

    MPU9250Setting setting;
    setting.accel_fs_sel     = ACCEL_FS_SEL::A4G;
    setting.gyro_fs_sel      = GYRO_FS_SEL::G1000DPS;   // rentang gerak tangan wajar
    setting.mag_output_bits  = MAG_OUTPUT_BITS::M16BITS;
    setting.fifo_sample_rate = FIFO_SAMPLE_RATE::SMPL_200HZ;
    setting.gyro_fchoice     = 0x03;
    setting.gyro_dlpf_cfg    = GYRO_DLPF_CFG::DLPF_41HZ;
    setting.accel_fchoice    = 0x01;
    setting.accel_dlpf_cfg   = ACCEL_DLPF_CFG::DLPF_45HZ;

    ready = mpu.setup(MPU_I2C_ADDRESS, setting);
    if (!ready) {
        Serial.println(F("[MpuHandler] MPU9250 TIDAK terdeteksi! Periksa wiring SDA/SCL/GND/VCC."));
        return;
    }

    // Air Mouse hanya membutuhkan LAJU gyro mentah (dps) untuk menghasilkan
    // delta pergerakan kursor relatif, bukan orientasi absolut. Karena itu,
    // fusi sensor (AHRS/quaternion) dimatikan untuk menghemat CPU - getGyroX/Y
    // tetap berfungsi normal terlepas dari pengaturan ini.
    mpu.selectFilter(QuatFilterSel::NONE);
    mpu.ahrs(false);

    // Kalibrasi bias awal: perangkat diasumsikan diam sesaat setelah boot.
    // Ini berjalan SEKALI di dalam setup() (sebelum loop() non-blocking mulai),
    // sehingga penantian singkat berbasis millis() di sini TIDAK melanggar
    // aturan "no delay() yang memblokir pergerakan kursor" pada loop utama.
    unsigned long t0 = millis();
    float sumX = 0.0f, sumY = 0.0f;
    uint16_t n = 0;
    while (millis() - t0 < 800UL) {
        if (mpu.update()) {
            sumX += mpu.getGyroX();
            sumY += mpu.getGyroY();
            n++;
        }
    }
    if (n > 0) {
        gyroBiasX = sumX / (float)n;
        gyroBiasY = sumY / (float)n;
    }

    lastSampleTime = millis();
    Serial.println(F("[MpuHandler] MPU9250 siap & terkalibrasi."));
}

bool MpuHandler::update(MouseDelta &out) {
    out.dx = 0;
    out.dy = 0;

    if (!ready) return false;
    if (!mpu.update()) return false; // belum ada sampel baru, tidak menunggu/blocking

    unsigned long now = millis();
    float dt = (now - lastSampleTime) / 1000.0f;
    lastSampleTime = now;
    // Guard terhadap dt tidak wajar (sampel pertama, atau baru resume dari jeda panjang)
    if (dt <= 0.0f || dt > 0.25f) dt = 0.01f;

    // --- Mode re-center: kumpulkan sampel untuk bias baru, jangan gerakkan kursor ---
    if (recentering) {
        recenterSumX += mpu.getGyroX();
        recenterSumY += mpu.getGyroY();
        recenterSamples++;
        if (now - recenterStartTime >= RECENTER_DURATION_MS) {
            finishRecenter();
        }
        return true;
    }

    float gx = mpu.getGyroX() - gyroBiasX; // sumbu ini dipetakan ke gerak vertikal (pitch)
    float gy = mpu.getGyroY() - gyroBiasY; // sumbu ini dipetakan ke gerak horizontal (roll)

    if (fabsf(gx) < GYRO_DEADZONE_DPS) gx = 0.0f;
    if (fabsf(gy) < GYRO_DEADZONE_DPS) gy = 0.0f;

    int16_t dx = (int16_t)(gy * MOUSE_SENSITIVITY * dt);
    int16_t dy = (int16_t)(-gx * MOUSE_SENSITIVITY * dt);

    dx = constrain(dx, -MAX_MOUSE_DELTA, MAX_MOUSE_DELTA);
    dy = constrain(dy, -MAX_MOUSE_DELTA, MAX_MOUSE_DELTA);

    out.dx = dx;
    out.dy = dy;
    return true;
}

void MpuHandler::startRecenter() {
    recentering       = true;
    recenterStartTime = millis();
    recenterSumX      = 0.0f;
    recenterSumY      = 0.0f;
    recenterSamples   = 0;
}

bool MpuHandler::isRecentering() const {
    return recentering;
}

void MpuHandler::finishRecenter() {
    if (recenterSamples > 0) {
        gyroBiasX = recenterSumX / (float)recenterSamples;
        gyroBiasY = recenterSumY / (float)recenterSamples;
    }
    recentering = false;
    lastSampleTime = millis(); // cegah lonjakan dt setelah proses re-center selesai
}

void MpuHandler::resetTimer() {
    lastSampleTime = millis();
}
