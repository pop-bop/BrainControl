#pragma once

#include <Arduino.h>

// ============================================================================
// TinkerTech / S4S ESP32-S3 Pin Definitions
// Reverse-engineered from official firmware dumps & LittleFS driver s4s.py
// ============================================================================

// --- Display (ST7789 320x240 LCD via SPI) ---
#define PIN_LCD_MOSI        40
#define PIN_LCD_MISO        38
#define PIN_LCD_SCK         39
#define PIN_LCD_DC          5
#define PIN_LCD_CS          4
#define PIN_LCD_RST         6
#define PIN_LCD_BL          15  // Active HIGH

// --- MicroSD Card (SPI Slot sharing bus with LCD) ---
#define PIN_SD_CS           3

// --- I2C Bus 0 (Internal: Mainboard MCU, LSM6DS3 IMU, FT6x36 Touch) ---
#define PIN_I2C0_SDA        42
#define PIN_I2C0_SCL        41
#define I2C0_FREQ           100000

// --- I2C Bus 1 (External Sensor Hub: Ultrasonic, Line Tracker, K210 Vision) ---
#define PIN_I2C1_SDA        1
#define PIN_I2C1_SCL        2
#define I2C1_FREQ           100000

// --- Expansion Header GPIOs ---
#define PIN_EXP_IO8         8
#define PIN_EXP_IO9         9
#define PIN_EXP_IO17        17
#define PIN_EXP_IO18        18
#define PIN_EXP_IO19        19
#define PIN_EXP_IO20        20
#define PIN_EXP_IO45        45
#define PIN_EXP_IO46        46
#define PIN_EXP_IO48        48

// --- I2C Addresses ---
#define I2C_ADDR_MAINBOARD      0x0F  // Motors, Servos, RGB, Battery, RTC, Voice
#define I2C_ADDR_TOUCH          0x38  // FT6x36 capacitive touch (or 0x48)
#define I2C_ADDR_IMU_LSM6DS3    0x6B  // 6-axis Accel/Gyro
#define I2C_ADDR_ULTRASONIC     0x57  // Ultrasonic Distance + RGB
#define I2C_ADDR_LINE_SENSOR    0x6F  // 4-probe Line/Grayscale/Color Sensor
#define I2C_ADDR_K210_VISION    0x24  // K210 AI Vision co-processor

// --- Mainboard Sub-Registers & Modules (I2C 0x0F) ---
#define MB_REG_BATTERY_CHARGE   0x00
#define MB_REG_AMBIENT_LIGHT    0x0A
#define MB_REG_RTC              0x14
#define MB_REG_SERVO            0x1E
#define MB_REG_VOICE            0x28
#define MB_REG_MOTOR_0          0x50
#define MB_REG_MOTOR_1          0x6E
#define MB_REG_MOTOR_2          0x8C
#define MB_REG_MOTOR_3          0xAA
#define MB_REG_MOTOR_PAIR       0xC8
