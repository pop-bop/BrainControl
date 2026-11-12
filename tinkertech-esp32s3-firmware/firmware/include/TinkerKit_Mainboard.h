#pragma once

#include <Arduino.h>
#include <Wire.h>
#include "TinkerKit_Pins.h"

// Movement Directions (Standard 0xC8 register values: 1=Forward, 2=Backward, 3=Left, 4=Right)
enum RobotDirection {
    DIR_STOP     = 0,
    DIR_FORWARD  = 1,
    DIR_BACKWARD = 2,
    DIR_LEFT     = 3,
    DIR_RIGHT    = 4
};

// Movement distance/time units
enum MovementUnit {
    UNIT_SECONDS     = 0,
    UNIT_ROTATIONS   = 1,
    UNIT_CENTIMETERS = 2
};

// Motor run mode
enum MotorRunMode {
    MOTOR_MODE_ROTATIONS   = 0,
    MOTOR_MODE_DEGREES     = 1,
    MOTOR_MODE_SECONDS     = 2,
    MOTOR_MODE_CENTIMETERS = 3
};

class TinkerKit_Mainboard {
public:
    TinkerKit_Mainboard();

    bool begin(TwoWire &wire = Wire);

    // ==========================================
    // Robot Dual-Drive Movement
    // ==========================================
    void setMotorPair(uint8_t leftId, uint8_t rightId);
    void drive(int16_t leftSpeed, int16_t rightSpeed);
    void move(RobotDirection dir, uint8_t speed = 50);
    void moveFor(RobotDirection dir, uint8_t speed, uint16_t durationOrDist, MovementUnit unit);
    void stop();
    bool isMotorActionComplete();
    void waitForMotorActionComplete(uint32_t timeoutMs = 10000);

    // ==========================================
    // Individual Motor Control (id: 0..3)
    // ==========================================
    void setMotorSpeed(uint8_t motorId, int16_t speed);
    void startMotor(uint8_t motorId, bool forward);
    void startMotorRPM(uint8_t motorId, int16_t rpm);
    void stopMotor(uint8_t motorId);
    void runMotorFor(uint8_t motorId, bool forward, uint16_t amount, MotorRunMode mode);
    int32_t getMotorPosition(uint8_t motorId);
    int16_t getMotorSpeed(uint8_t motorId);
    int16_t getMotorRPM(uint8_t motorId);

    // ==========================================
    // Servos (id: 0..1)
    // ==========================================
    void setServoAngle(uint8_t servoId, uint8_t angleDeg);
    void setServoSpeed(uint8_t servoId, int8_t speed);
    void releaseServo(uint8_t servoId);

    // ==========================================
    // Ambient RGB LED
    // ==========================================
    void setLightColor(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness = 50);
    void setLightBrightness(uint8_t brightness);

    // ==========================================
    // Battery & Power System
    // ==========================================
    int8_t getBatteryPercentage();
    float getBatteryVoltage();
    bool isCharging();
    String getMainboardVersion();

    // ==========================================
    // Offline Voice Recognition (0x28)
    // ==========================================
    uint8_t readVoiceCommand();

    // ==========================================
    // Hardware RTC
    // ==========================================
    void setRTC(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second);
    void getRTC(uint16_t &year, uint8_t &month, uint8_t &day, uint8_t &hour, uint8_t &minute, uint8_t &second);

    bool isAvailable() const { return _available; }

private:
    TwoWire *_wire;
    bool _available = false;
    const uint8_t _mbMotorRegs[4] = { MB_REG_MOTOR_0, MB_REG_MOTOR_1, MB_REG_MOTOR_2, MB_REG_MOTOR_3 };
    int16_t _speedCache[4] = { 50, 50, 50, 50 };

    bool writeReg(uint8_t reg, const uint8_t *data, size_t len);
    bool writeRegByte(uint8_t reg, uint8_t val);
    bool readReg(uint8_t reg, uint8_t *data, size_t len);
};
