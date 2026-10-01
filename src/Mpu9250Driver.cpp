#include "Mpu9250Driver.h"
#include "Config.h"
#include "RtosUtil.h"
#include <math.h>

namespace {
    constexpr uint8_t REG_SMPLRT_DIV   = 0x19;
    constexpr uint8_t REG_CONFIG       = 0x1A;
    constexpr uint8_t REG_GYRO_CONFIG  = 0x1B;
    constexpr uint8_t REG_ACCEL_CONFIG = 0x1C;
    constexpr uint8_t REG_ACCEL_CFG2   = 0x1D;
    constexpr uint8_t REG_INT_PIN_CFG  = 0x37;
    constexpr uint8_t REG_ACCEL_XOUT_H = 0x3B;
    constexpr uint8_t REG_USER_CTRL    = 0x6A;
    constexpr uint8_t REG_PWR_MGMT_1   = 0x6B;
    constexpr uint8_t REG_PWR_MGMT_2   = 0x6C;
    constexpr uint8_t REG_WHO_AM_I     = 0x75;

    constexpr uint8_t AK8963_ADDRESS   = 0x0C;
    constexpr uint8_t AK8963_WIA       = 0x00;
    constexpr uint8_t AK8963_ID        = 0x48;

    constexpr uint8_t WHO_MPU6050      = 0x68;
    constexpr uint8_t WHO_MPU6500      = 0x70;
    constexpr uint8_t WHO_MPU9250      = 0x71;
    constexpr uint8_t WHO_MPU9255      = 0x73;
    constexpr uint8_t WHO_ICM20948     = 0xEA;

    constexpr float ACCEL_LSB_PER_G    = 8192.0f;
    constexpr float GYRO_LSB_PER_DPS   = 32.8f;
}

const char *Mpu9250Driver::chipName() const {
    switch (whoAmI_) {
        case WHO_MPU9250: return "MPU-9250";
        case WHO_MPU9255: return "MPU-9255";
        case WHO_MPU6500: return "MPU-6500";
        case WHO_MPU6050: return "MPU-6050 (atau klon berlabel MPU9250)";
        default:          return "chip tidak dikenal";
    }
}

bool Mpu9250Driver::writeReg(uint8_t reg, uint8_t value) {
    wire_->beginTransmission(address_);
    wire_->write(reg);
    wire_->write(value);
    return wire_->endTransmission() == 0;
}

bool Mpu9250Driver::readRegs(uint8_t reg, uint8_t *buffer, uint8_t length) {
    wire_->beginTransmission(address_);
    wire_->write(reg);
    if (wire_->endTransmission(false) != 0) return false;

    const uint8_t received = wire_->requestFrom(static_cast<uint16_t>(address_), length, true);
    if (received != length) {
        while (wire_->available()) wire_->read();
        return false;
    }
    for (uint8_t i = 0; i < length; i++) buffer[i] = static_cast<uint8_t>(wire_->read());
    return true;
}

bool Mpu9250Driver::readReg(uint8_t reg, uint8_t &value) {
    return readRegs(reg, &value, 1);
}

bool Mpu9250Driver::writeVerified(uint8_t reg, uint8_t value) {
    if (!writeReg(reg, value)) return false;
    uint8_t readBack = 0;
    if (!readReg(reg, readBack)) return false;
    return readBack == value;
}

bool Mpu9250Driver::isAcceptedWhoAmI(uint8_t id) const {
    if (id == WHO_MPU9250 || id == WHO_MPU9255 || id == WHO_MPU6500 || id == WHO_MPU6050) return true;
#if MPU_ACCEPT_UNKNOWN_WHOAMI
    if (id != 0x00 && id != 0xFF && id != WHO_ICM20948) return true;
#endif
    return false;
}

bool Mpu9250Driver::waitResetComplete() {
    const uint32_t start = millis();
    while (millis() - start < 300) {
        uint8_t value = 0x80;
        if (readReg(REG_PWR_MGMT_1, value) && (value & 0x80) == 0) return true;
        vTaskDelay(msToTicks(5));
    }
    return false;
}

bool Mpu9250Driver::validateStream() {
    ImuSample sample;
    for (uint8_t i = 0; i < 4; i++) {
        vTaskDelay(msToTicks(6));
        if (!read(sample)) {
            error_ = "gagal membaca burst data sensor";
            return false;
        }
        const float accelMagnitude = sqrtf(sample.ax * sample.ax + sample.ay * sample.ay + sample.az * sample.az);
        bool allZero = true;
        bool allFull = true;
        for (uint8_t k = 0; k < 6; k++) {
            if (sample.raw[k] != 0)  allZero = false;
            if (sample.raw[k] != -1) allFull = false;
        }
        if (allZero || allFull) {
            error_ = "data sensor nol/0xFF (sensor tidur atau bus rusak)";
            return false;
        }
        if (accelMagnitude < 0.2f || accelMagnitude > 3.5f) {
            error_ = "magnitudo akselerometer tidak masuk akal";
            return false;
        }
    }
    return true;
}

bool Mpu9250Driver::begin(TwoWire &wire, uint8_t address) {
    wire_ = &wire;
    address_ = address;
    error_ = "";
    magPresent_ = false;

    uint8_t who = 0;
    if (!readReg(REG_WHO_AM_I, who)) {
        error_ = "tidak ada ACK saat membaca WHO_AM_I";
        return false;
    }
    whoAmI_ = who;

    if (who == WHO_ICM20948) {
        error_ = "terdeteksi ICM-20948 (peta register berbeda, tidak didukung)";
        return false;
    }
    if (!isAcceptedWhoAmI(who)) {
        error_ = "nilai WHO_AM_I tidak valid (0x00/0xFF: bus atau daya bermasalah)";
        return false;
    }

    if (!writeReg(REG_PWR_MGMT_1, 0x80)) {
        error_ = "gagal mengirim perintah reset";
        return false;
    }
    vTaskDelay(msToTicks(100));
    if (!waitResetComplete()) {
        error_ = "sensor tidak selesai reset";
        return false;
    }

    if (!writeVerified(REG_PWR_MGMT_1, 0x01)) {
        error_ = "gagal membangunkan sensor (PWR_MGMT_1)";
        return false;
    }
    vTaskDelay(msToTicks(30));

    writeReg(REG_PWR_MGMT_2, 0x00);
    writeReg(REG_USER_CTRL, 0x01);
    vTaskDelay(msToTicks(15));

    if (!writeVerified(REG_SMPLRT_DIV, 4)) {
        error_ = "verifikasi SMPLRT_DIV gagal";
        return false;
    }
    if (!writeVerified(REG_CONFIG, 0x03)) {
        error_ = "verifikasi CONFIG (DLPF) gagal";
        return false;
    }
    if (!writeVerified(REG_GYRO_CONFIG, 0x10)) {
        error_ = "verifikasi GYRO_CONFIG gagal";
        return false;
    }
    if (!writeVerified(REG_ACCEL_CONFIG, 0x08)) {
        error_ = "verifikasi ACCEL_CONFIG gagal";
        return false;
    }
    if (who != WHO_MPU6050) writeReg(REG_ACCEL_CFG2, 0x03);

    writeReg(REG_INT_PIN_CFG, 0x02);
    vTaskDelay(msToTicks(10));
    wire_->beginTransmission(AK8963_ADDRESS);
    wire_->write(AK8963_WIA);
    if (wire_->endTransmission(false) == 0) {
        if (wire_->requestFrom(static_cast<uint16_t>(AK8963_ADDRESS), static_cast<uint8_t>(1), true) == 1) {
            magPresent_ = (wire_->read() == AK8963_ID);
        }
    }

    vTaskDelay(msToTicks(50));
    return validateStream();
}

bool Mpu9250Driver::read(ImuSample &out) {
    uint8_t buffer[14];
    if (!readRegs(REG_ACCEL_XOUT_H, buffer, sizeof(buffer))) return false;

    out.raw[0] = static_cast<int16_t>((buffer[0]  << 8) | buffer[1]);
    out.raw[1] = static_cast<int16_t>((buffer[2]  << 8) | buffer[3]);
    out.raw[2] = static_cast<int16_t>((buffer[4]  << 8) | buffer[5]);
    out.raw[3] = static_cast<int16_t>((buffer[8]  << 8) | buffer[9]);
    out.raw[4] = static_cast<int16_t>((buffer[10] << 8) | buffer[11]);
    out.raw[5] = static_cast<int16_t>((buffer[12] << 8) | buffer[13]);

    out.ax = out.raw[0] / ACCEL_LSB_PER_G;
    out.ay = out.raw[1] / ACCEL_LSB_PER_G;
    out.az = out.raw[2] / ACCEL_LSB_PER_G;
    out.gx = out.raw[3] / GYRO_LSB_PER_DPS;
    out.gy = out.raw[4] / GYRO_LSB_PER_DPS;
    out.gz = out.raw[5] / GYRO_LSB_PER_DPS;
    return true;
}
