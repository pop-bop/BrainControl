#include "TinkerKit_Mainboard.h"

TinkerKit_Mainboard::TinkerKit_Mainboard() : _wire(&Wire) {}

bool TinkerKit_Mainboard::begin(TwoWire &wire) {
    _wire = &wire;
    // Verify co-processor is alive
    _wire->beginTransmission(I2C_ADDR_MAINBOARD);
    _available = (_wire->endTransmission() == 0);
    return _available;
}

bool TinkerKit_Mainboard::writeReg(uint8_t reg, const uint8_t *data, size_t len) {
    _wire->beginTransmission(I2C_ADDR_MAINBOARD);
    _wire->write(reg);
    for (size_t i = 0; i < len; ++i) {
        _wire->write(data[i]);
    }
    return (_wire->endTransmission() == 0);
}

bool TinkerKit_Mainboard::writeRegByte(uint8_t reg, uint8_t val) {
    return writeReg(reg, &val, 1);
}

bool TinkerKit_Mainboard::readReg(uint8_t reg, uint8_t *data, size_t len) {
    _wire->beginTransmission(I2C_ADDR_MAINBOARD);
    _wire->write(reg);
    if (_wire->endTransmission(false) != 0) {
        return false;
    }
    size_t count = _wire->requestFrom((uint8_t)I2C_ADDR_MAINBOARD, (uint8_t)len);
    if (count != len) return false;
    for (size_t i = 0; i < len; ++i) {
        data[i] = _wire->read();
    }
    return true;
}

// ============================================================================
// Robot Dual-Drive Movement
// ============================================================================

void TinkerKit_Mainboard::setMotorPair(uint8_t leftId, uint8_t rightId) {
    uint8_t pair[2] = { leftId, rightId };
    writeReg(MB_REG_MOTOR_PAIR + 0x01, pair, 2);
}

void TinkerKit_Mainboard::drive(int16_t leftSpeed, int16_t rightSpeed) {
    bool l_fwd = (leftSpeed >= 0);
    bool r_fwd = (rightSpeed >= 0);

    uint16_t l_abs = constrain(abs(leftSpeed), 0, 100);
    uint16_t r_abs = constrain(abs(rightSpeed), 0, 100);

    setMotorPair(0, 1);
    uint8_t speedBuf[4];
    speedBuf[0] = (l_abs >> 8) & 0xFF;
    speedBuf[1] = l_abs & 0xFF;
    speedBuf[2] = (r_abs >> 8) & 0xFF;
    speedBuf[3] = r_abs & 0xFF;
    writeReg(MB_REG_MOTOR_PAIR + 0x02, speedBuf, 4);

    uint8_t dir = DIR_STOP;
    if (l_abs == 0 && r_abs == 0) {
        dir = DIR_STOP;
    } else if (l_fwd && r_fwd) {
        dir = DIR_FORWARD;  // 1: Forward
    } else if (!l_fwd && !r_fwd) {
        dir = DIR_BACKWARD; // 2: Backward
    } else if (!l_fwd && r_fwd) {
        dir = DIR_LEFT;     // 3: Turn Left (L backward, R forward)
    } else if (l_fwd && !r_fwd) {
        dir = DIR_RIGHT;    // 4: Turn Right (L forward, R backward)
    }
    writeRegByte(MB_REG_MOTOR_PAIR, dir);
}

void TinkerKit_Mainboard::move(RobotDirection dir, uint8_t speed) {
    speed = constrain(speed, 0, 100);
    setMotorPair(0, 1);
    uint8_t speedBuf[4] = { 0, speed, 0, speed };
    writeReg(MB_REG_MOTOR_PAIR + 0x02, speedBuf, 4);
    writeRegByte(MB_REG_MOTOR_PAIR, (uint8_t)dir);
}

void TinkerKit_Mainboard::moveFor(RobotDirection dir, uint8_t speed, uint16_t durationOrDist, MovementUnit unit) {
    speed = constrain(speed, 0, 100);
    uint8_t speedBuf[4] = { 0, speed, 0, speed };
    writeReg(MB_REG_MOTOR_PAIR + 0x02, speedBuf, 4);

    uint8_t offset = (uint8_t)unit + 3;
    if (unit > UNIT_SECONDS) {
        uint8_t buf[4] = {
            (uint8_t)((durationOrDist >> 8) & 0xFF),
            (uint8_t)(durationOrDist & 0xFF),
            (uint8_t)((durationOrDist >> 8) & 0xFF),
            (uint8_t)(durationOrDist & 0xFF)
        };
        writeReg(MB_REG_MOTOR_PAIR + offset, buf, 4);
    } else {
        uint8_t buf[2] = {
            (uint8_t)((durationOrDist >> 8) & 0xFF),
            (uint8_t)(durationOrDist & 0xFF)
        };
        writeReg(MB_REG_MOTOR_PAIR + offset, buf, 2);
    }

    uint8_t action = (uint8_t)dir + 4 + ((uint8_t)unit * 4);
    writeRegByte(MB_REG_MOTOR_PAIR, action);
    waitForMotorActionComplete();
}

void TinkerKit_Mainboard::stop() {
    writeRegByte(MB_REG_MOTOR_PAIR, 0);
    writeRegByte(MB_REG_MOTOR_0 + 0x04, 0);
    writeRegByte(MB_REG_MOTOR_1 + 0x04, 0);
}

bool TinkerKit_Mainboard::isMotorActionComplete() {
    uint8_t status = 0xFF;
    if (readReg(MB_REG_MOTOR_PAIR + 0x06, &status, 1)) {
        return (status == 0);
    }
    return false;
}

void TinkerKit_Mainboard::waitForMotorActionComplete(uint32_t timeoutMs) {
    delay(100);
    uint32_t start = millis();
    while (millis() - start < timeoutMs) {
        if (isMotorActionComplete()) break;
        delay(50);
    }
}

// ============================================================================
// Individual Motors
// ============================================================================

void TinkerKit_Mainboard::setMotorSpeed(uint8_t motorId, int16_t speed) {
    if (motorId >= 4) return;
    speed = constrain(abs(speed), 0, 100);
    _speedCache[motorId] = speed;
    uint8_t buf[2] = { (uint8_t)((speed >> 8) & 0xFF), (uint8_t)(speed & 0xFF) };
    writeReg(_mbMotorRegs[motorId] + 0x06, buf, 2);
}

void TinkerKit_Mainboard::startMotor(uint8_t motorId, bool forward) {
    if (motorId >= 4) return;
    setMotorSpeed(motorId, _speedCache[motorId]);
    delay(5);
    writeRegByte(_mbMotorRegs[motorId] + 0x04, forward ? 3 : 4);
}

void TinkerKit_Mainboard::startMotorRPM(uint8_t motorId, int16_t rpm) {
    if (motorId >= 4) return;
    uint8_t dir = (rpm >= 0) ? 1 : 2;
    uint16_t actual = constrain(abs(rpm), 0, 100) * 180 / 100;
    uint8_t buf[2] = { (uint8_t)((actual >> 8) & 0xFF), (uint8_t)(actual & 0xFF) };
    writeReg(_mbMotorRegs[motorId] + 0x05, buf, 2);
    writeRegByte(_mbMotorRegs[motorId] + 0x04, dir);
}

void TinkerKit_Mainboard::stopMotor(uint8_t motorId) {
    if (motorId >= 4) return;
    writeRegByte(_mbMotorRegs[motorId] + 0x04, 0);
}

void TinkerKit_Mainboard::runMotorFor(uint8_t motorId, bool forward, uint16_t amount, MotorRunMode mode) {
    if (motorId >= 4) return;
    uint8_t buf[2] = { (uint8_t)((amount >> 8) & 0xFF), (uint8_t)(amount & 0xFF) };

    const uint8_t regMap[4] = { 0x08, 0x09, 0x0A, 0x0B };
    writeReg(_mbMotorRegs[motorId] + regMap[(uint8_t)mode], buf, 2);

    const uint8_t baseAction[4] = { 7, 9, 11, 13 };
    uint8_t action = baseAction[(uint8_t)mode] + (forward ? 0 : 1);
    writeRegByte(_mbMotorRegs[motorId] + 0x04, action);

    // Wait until done
    delay(100);
    uint8_t status = 1;
    while (status != 0) {
        if (!readReg(_mbMotorRegs[motorId] + 0x0C, &status, 1)) break;
        delay(50);
    }
}

int32_t TinkerKit_Mainboard::getMotorPosition(uint8_t motorId) {
    if (motorId >= 4) return 0;
    uint8_t buf[4] = { 0 };
    if (readReg(_mbMotorRegs[motorId], buf, 4)) {
        return (int32_t)((buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3]);
    }
    return 0;
}

int16_t TinkerKit_Mainboard::getMotorSpeed(uint8_t motorId) {
    if (motorId >= 4) return 0;
    uint8_t buf[2] = { 0 };
    if (readReg(_mbMotorRegs[motorId] + 0x02, buf, 2)) {
        return (int16_t)((buf[0] << 8) | buf[1]);
    }
    return 0;
}

int16_t TinkerKit_Mainboard::getMotorRPM(uint8_t motorId) {
    if (motorId >= 4) return 0;
    uint8_t buf[2] = { 0 };
    if (readReg(_mbMotorRegs[motorId] + 0x01, buf, 2)) {
        return (int16_t)((buf[0] << 8) | buf[1]);
    }
    return 0;
}

// ============================================================================
// Servos (0..1)
// ============================================================================

void TinkerKit_Mainboard::setServoAngle(uint8_t servoId, uint8_t angleDeg) {
    if (servoId > 1) return;
    angleDeg = constrain(angleDeg, 0, 180);
    writeRegByte(MB_REG_SERVO + servoId, angleDeg);
}

void TinkerKit_Mainboard::setServoSpeed(uint8_t servoId, int8_t speed) {
    if (servoId > 1) return;
    speed = constrain(speed, -100, 100);
    writeRegByte(MB_REG_SERVO + 2 + servoId, (uint8_t)speed);
}

void TinkerKit_Mainboard::releaseServo(uint8_t servoId) {
    if (servoId > 1) return;
    writeRegByte(MB_REG_SERVO + 4 + servoId, 0x00);
}

// ============================================================================
// Ambient RGB Light
// ============================================================================

void TinkerKit_Mainboard::setLightColor(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
    setLightBrightness(brightness);
    uint8_t rgb[3] = { r, g, b };
    writeReg(MB_REG_AMBIENT_LIGHT + 1, rgb, 3);
}

void TinkerKit_Mainboard::setLightBrightness(uint8_t brightness) {
    writeRegByte(MB_REG_AMBIENT_LIGHT, brightness);
}

// ============================================================================
// Battery & Power System
// ============================================================================

int8_t TinkerKit_Mainboard::getBatteryPercentage() {
    uint8_t val = 0;
    if (readReg(MB_REG_BATTERY_CHARGE, &val, 1)) {
        return (int8_t)val;
    }
    return -1;
}

float TinkerKit_Mainboard::getBatteryVoltage() {
    uint8_t val = 0;
    if (readReg(MB_REG_BATTERY_CHARGE + 1, &val, 1)) {
        return val / 10.0f;
    }
    return 0.0f;
}

bool TinkerKit_Mainboard::isCharging() {
    uint8_t val = 0;
    if (readReg(MB_REG_BATTERY_CHARGE + 2, &val, 1)) {
        return (val != 0);
    }
    return false;
}

String TinkerKit_Mainboard::getMainboardVersion() {
    uint8_t ver[3] = { 0 };
    if (readReg(MB_REG_BATTERY_CHARGE + 4, ver, 3)) {
        return String(ver[0]) + "." + String(ver[1]) + "." + String(ver[2]);
    }
    return "Unknown";
}

// ============================================================================
// Voice Commands (0x28)
// ============================================================================

uint8_t TinkerKit_Mainboard::readVoiceCommand() {
    uint8_t cmd = 0;
    if (readReg(MB_REG_VOICE, &cmd, 1)) {
        if (cmd != 0) {
            // Acknowledge/clear
            writeRegByte(MB_REG_VOICE, 0);
        }
        return cmd;
    }
    return 0;
}

// ============================================================================
// Hardware RTC
// ============================================================================

void TinkerKit_Mainboard::setRTC(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second) {
    uint8_t timeBuf[3] = { hour, minute, second };
    writeReg(MB_REG_RTC, timeBuf, 3);

    uint8_t dateBuf[4] = { 0, month, day, (uint8_t)(year % 100) };
    writeReg(MB_REG_RTC + 1, dateBuf, 4);
}

void TinkerKit_Mainboard::getRTC(uint16_t &year, uint8_t &month, uint8_t &day, uint8_t &hour, uint8_t &minute, uint8_t &second) {
    uint8_t timeBuf[3] = { 0 };
    readReg(MB_REG_RTC, timeBuf, 3);
    hour = timeBuf[0];
    minute = timeBuf[1];
    second = timeBuf[2];

    uint8_t dateBuf[4] = { 0 };
    readReg(MB_REG_RTC + 1, dateBuf, 4);
    month = dateBuf[1];
    day = dateBuf[2];
    year = 2000 + dateBuf[3];
}
