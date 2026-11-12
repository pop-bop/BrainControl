#pragma once

#include <Arduino.h>
#include <Wire.h>
#include "TinkerKit_Pins.h"

// Line sensor color constants
enum LineColor {
    COLOR_UNKNOWN = -1,
    COLOR_GRAY    = -2,
    COLOR_RED     = 2,
    COLOR_GREEN   = 3,
    COLOR_BLUE    = 4,
    COLOR_YELLOW  = 5,
    COLOR_CYAN    = 6,
    COLOR_PURPLE  = 7,
    COLOR_BLACK   = 8,
    COLOR_WHITE   = 9
};

// ============================================================================
// Ultrasonic Distance Sensor + Built-in RGB LEDs (I2C1: 0x57)
// ============================================================================
class TinkerKit_Ultrasonic {
public:
    TinkerKit_Ultrasonic();
    bool begin(TwoWire &wire = Wire1);

    float getDistanceMM();
    float getDistanceCM();
    float getDistanceInch();
    uint32_t getRawDistance() const { return _lastRaw; }

    void setRGB(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness = 125);
    bool isAvailable() const { return _available; }

private:
    TwoWire *_wire;
    bool _available = false;
    uint32_t _lastRaw = 0;
};

// ============================================================================
// 4-Probe Line & Color Tracking Sensor (I2C1: 0x6F)
// ============================================================================
class TinkerKit_LineSensor {
public:
    TinkerKit_LineSensor();
    bool begin(TwoWire &wire = Wire1);

    bool isConnected();
    bool detectLine(uint8_t probeIndex); // probe 0..3: L2, L1, R1, R2
    uint8_t getGrayscale(uint8_t probeIndex);
    LineColor getColor(uint8_t probeIndex);

private:
    TwoWire *_wire;
};

// ============================================================================
// LSM6DS3 6-Axis IMU (I2C0: 0x6B)
// ============================================================================
class TinkerKit_IMU {
public:
    TinkerKit_IMU();
    bool begin(TwoWire &wire = Wire);

    bool readAcceleration(float &x, float &y, float &z);
    bool readGyro(float &gx, float &gy, float &gz);
    void getPitchRoll(float &pitch, float &roll);
    bool isAvailable() const { return _available; }

private:
    TwoWire *_wire;
    bool _available = false;
    int16_t toInt16(uint8_t low, uint8_t high);
};
