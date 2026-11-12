#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>

#include "TinkerKit_Pins.h"
#include "TinkerKit_Mainboard.h"
#include "TinkerKit_Sensors.h"
#include "TinkerKit_Display.h"
#include "TinkerKit_ESPNow.h"

class TinkerKitClass {
public:
    TinkerKit_Display    display;
    TinkerKit_Mainboard  mainboard;
    TinkerKit_Ultrasonic ultrasonic;
    TinkerKit_LineSensor lineSensor;
    TinkerKit_IMU        imu;
    TinkerKit_ESPNow     espnow;

    bool begin(bool initDisplay = true, bool initESPNow = true) {
        Serial.begin(115200);

        // Initialize Internal I2C Bus 0 (Mainboard MCU, IMU, Touch)
        Wire.begin(PIN_I2C0_SDA, PIN_I2C0_SCL, I2C0_FREQ);

        // Initialize External I2C Bus 1 (Ultrasonic, Line Tracker, K210)
        Wire1.begin(PIN_I2C1_SDA, PIN_I2C1_SCL, I2C1_FREQ);

        // Initialize Mainboard Co-processor
        mainboard.begin(Wire);

        // Set default robot drive motor pair: Left = 0, Right = 1
        mainboard.setMotorPair(0, 1);

        // Initialize Sensors
        imu.begin(Wire);
        ultrasonic.begin(Wire1);
        lineSensor.begin(Wire1);

        // Initialize Display & Touch
        if (initDisplay) {
            display.begin();
        }

        // Initialize ESP-NOW
        if (initESPNow) {
            espnow.begin(1);
        }

        return true;
    }

    // Configure user expansion pins (IO8, 9, 17, 18, 19, 20, 45, 46, 48)
    void pinModeExp(uint8_t pin, uint8_t mode) {
        pinMode(pin, mode);
    }
};

extern TinkerKitClass TinkerKit;
