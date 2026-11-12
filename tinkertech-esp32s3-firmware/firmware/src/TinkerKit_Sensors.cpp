#include "TinkerKit_Sensors.h"

// ============================================================================
// Ultrasonic Distance Sensor (0x57 on I2C1)
// ============================================================================
TinkerKit_Ultrasonic::TinkerKit_Ultrasonic() : _wire(&Wire1) {}

bool TinkerKit_Ultrasonic::begin(TwoWire &wire) {
    _wire = &wire;
    _wire->beginTransmission(I2C_ADDR_ULTRASONIC);
    _available = (_wire->endTransmission() == 0);
    return _available;
}

float TinkerKit_Ultrasonic::getDistanceCM() {
    _wire->beginTransmission(I2C_ADDR_ULTRASONIC);
    _wire->write(0x01);
    if (_wire->endTransmission(false) != 0) return -1.0f;

    if (_wire->requestFrom((uint8_t)I2C_ADDR_ULTRASONIC, (uint8_t)3) == 3) {
        uint8_t b0 = _wire->read();
        uint8_t b1 = _wire->read();
        uint8_t b2 = _wire->read();
        _lastRaw = ((uint32_t)b0 << 16) | ((uint32_t)b1 << 8) | b2;
        float cm = (float)_lastRaw / 10000.0f;
        if (cm < 0.5f || cm > 450.0f) return -1.0f;
        return cm;
    }
    return -1.0f;
}

float TinkerKit_Ultrasonic::getDistanceMM() {
    float cm = getDistanceCM();
    return (cm > 0) ? (cm * 10.0f) : -1.0f;
}

float TinkerKit_Ultrasonic::getDistanceInch() {
    float cm = getDistanceCM();
    return (cm > 0) ? (cm / 2.54f) : -1.0f;
}

void TinkerKit_Ultrasonic::setRGB(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
    _wire->beginTransmission(I2C_ADDR_ULTRASONIC);
    _wire->write(0x02);
    _wire->write(brightness);
    _wire->write(r);
    _wire->write(g);
    _wire->write(b);
    _wire->endTransmission();
}

// ============================================================================
// 4-Probe Line Sensor (0x6F on I2C1)
// ============================================================================
TinkerKit_LineSensor::TinkerKit_LineSensor() : _wire(&Wire1) {}

bool TinkerKit_LineSensor::begin(TwoWire &wire) {
    _wire = &wire;
    return isConnected();
}

bool TinkerKit_LineSensor::isConnected() {
    _wire->beginTransmission(I2C_ADDR_LINE_SENSOR);
    return (_wire->endTransmission() == 0);
}

bool TinkerKit_LineSensor::detectLine(uint8_t probeIndex) {
    if (probeIndex >= 4) return false;
    _wire->beginTransmission(I2C_ADDR_LINE_SENSOR);
    _wire->write(0x03);
    _wire->endTransmission();
    delay(5);

    if (_wire->requestFrom((uint8_t)I2C_ADDR_LINE_SENSOR, (uint8_t)5) == 5) {
        uint8_t data[5];
        for (int i = 0; i < 5; ++i) data[i] = _wire->read();
        return (data[probeIndex] != 0);
    }
    return false;
}

uint8_t TinkerKit_LineSensor::getGrayscale(uint8_t probeIndex) {
    if (probeIndex >= 4) return 0;
    _wire->beginTransmission(I2C_ADDR_LINE_SENSOR);
    _wire->write(0x02);
    _wire->endTransmission();
    delay(5);

    if (_wire->requestFrom((uint8_t)I2C_ADDR_LINE_SENSOR, (uint8_t)5) == 5) {
        uint8_t data[5];
        for (int i = 0; i < 5; ++i) data[i] = _wire->read();
        return data[probeIndex];
    }
    return 0;
}

LineColor TinkerKit_LineSensor::getColor(uint8_t probeIndex) {
    if (probeIndex >= 4) return COLOR_UNKNOWN;
    _wire->beginTransmission(I2C_ADDR_LINE_SENSOR);
    _wire->write(0x01);
    _wire->endTransmission();
    delay(5);

    if (_wire->requestFrom((uint8_t)I2C_ADDR_LINE_SENSOR, (uint8_t)5) == 5) {
        uint8_t data[5];
        for (int i = 0; i < 5; ++i) data[i] = _wire->read();
        uint8_t val = data[probeIndex] + 1;
        if (val == 1) return COLOR_UNKNOWN;
        return (LineColor)val;
    }
    return COLOR_UNKNOWN;
}

// ============================================================================
// LSM6DS3 IMU (0x6B on I2C0)
// ============================================================================
TinkerKit_IMU::TinkerKit_IMU() : _wire(&Wire) {}

int16_t TinkerKit_IMU::toInt16(uint8_t low, uint8_t high) {
    return (int16_t)((high << 8) | low);
}

bool TinkerKit_IMU::begin(TwoWire &wire) {
    _wire = &wire;
    _wire->beginTransmission(I2C_ADDR_IMU_LSM6DS3);
    _wire->write(0x0F); // WHO_AM_I
    if (_wire->endTransmission(false) != 0) return false;

    if (_wire->requestFrom((uint8_t)I2C_ADDR_IMU_LSM6DS3, (uint8_t)1) != 1) return false;
    uint8_t who = _wire->read();
    if (who != 0x69 && who != 0x6A) return false;

    _available = true;

    // CTRL1_XL: Accel 104Hz, +-2g
    _wire->beginTransmission(I2C_ADDR_IMU_LSM6DS3);
    _wire->write(0x10);
    _wire->write(0x40);
    _wire->endTransmission();

    // CTRL2_G: Gyro 104Hz, 2000 dps
    _wire->beginTransmission(I2C_ADDR_IMU_LSM6DS3);
    _wire->write(0x11);
    _wire->write(0x4C);
    _wire->endTransmission();

    // CTRL3_C: IF_INC (auto-increment register pointer)
    _wire->beginTransmission(I2C_ADDR_IMU_LSM6DS3);
    _wire->write(0x12);
    _wire->write(0x04);
    _wire->endTransmission();

    return true;
}

bool TinkerKit_IMU::readAcceleration(float &x, float &y, float &z) {
    _wire->beginTransmission(I2C_ADDR_IMU_LSM6DS3);
    _wire->write(0x28); // OUTX_L_XL
    if (_wire->endTransmission(false) != 0) return false;

    if (_wire->requestFrom((uint8_t)I2C_ADDR_IMU_LSM6DS3, (uint8_t)6) == 6) {
        uint8_t xl = _wire->read();
        uint8_t xh = _wire->read();
        uint8_t yl = _wire->read();
        uint8_t yh = _wire->read();
        uint8_t zl = _wire->read();
        uint8_t zh = _wire->read();
        // +-2g sensitivity = ~0.061 mg/LSB
        x = toInt16(xl, xh) * 0.061f / 1000.0f;
        y = toInt16(yl, yh) * 0.061f / 1000.0f;
        z = toInt16(zl, zh) * 0.061f / 1000.0f;
        return true;
    }
    return false;
}

bool TinkerKit_IMU::readGyro(float &gx, float &gy, float &gz) {
    _wire->beginTransmission(I2C_ADDR_IMU_LSM6DS3);
    _wire->write(0x22); // OUTX_L_G
    if (_wire->endTransmission(false) != 0) return false;

    if (_wire->requestFrom((uint8_t)I2C_ADDR_IMU_LSM6DS3, (uint8_t)6) == 6) {
        uint8_t xl = _wire->read();
        uint8_t xh = _wire->read();
        uint8_t yl = _wire->read();
        uint8_t yh = _wire->read();
        uint8_t zl = _wire->read();
        uint8_t zh = _wire->read();
        // 2000 dps sensitivity = ~70 mdps/LSB
        gx = toInt16(xl, xh) * 70.0f / 1000.0f;
        gy = toInt16(yl, yh) * 70.0f / 1000.0f;
        gz = toInt16(zl, zh) * 70.0f / 1000.0f;
        return true;
    }
    return false;
}

void TinkerKit_IMU::getPitchRoll(float &pitch, float &roll) {
    float ax, ay, az;
    if (readAcceleration(ax, ay, az)) {
        pitch = atan2(-ax, sqrt(ay * ay + az * az)) * 180.0f / PI;
        roll  = atan2(ay, az) * 180.0f / PI;
    } else {
        pitch = 0.0f;
        roll  = 0.0f;
    }
}
