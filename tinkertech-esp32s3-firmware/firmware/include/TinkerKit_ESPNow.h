#pragma once

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// ESP-NOW Command Types
enum ESPNowCmdType : uint8_t {
    CMD_DRIVE            = 0x01, // leftSpeed, rightSpeed
    CMD_STOP             = 0x02,
    CMD_SERVO            = 0x03, // servoId, angle
    CMD_RGB              = 0x04, // r, g, b, brightness
    CMD_REQUEST_TELEM    = 0x05,
    CMD_CUSTOM           = 0x0A
};

// Packed structure for remote control commands
struct __attribute__((packed)) RobotControlPacket {
    uint8_t  cmd;         // ESPNowCmdType
    int16_t  leftSpeed;   // -100 .. 100
    int16_t  rightSpeed;  // -100 .. 100
    uint8_t  servoId;     // 0 or 1
    uint8_t  servoAngle;  // 0 .. 180
    uint8_t  rgbR;        // 0 .. 255
    uint8_t  rgbG;        // 0 .. 255
    uint8_t  rgbB;        // 0 .. 255
    uint8_t  rgbBright;   // 0 .. 255
    uint32_t seq;         // Sequence counter
};

// Packed structure for robot telemetry returned to controller/remote
struct __attribute__((packed)) RobotTelemetryPacket {
    int8_t   batteryPct;      // 0..100%
    float    batteryVolts;    // e.g. 7.4V
    float    ultrasonicMM;    // Distance in mm
    uint8_t  lineSensorState; // Bits 0..3 for 4 probes
    float    imuPitch;        // Pitch in degrees
    float    imuRoll;         // Roll in degrees
    int32_t  motor0Pos;       // Encoder position
    int32_t  motor1Pos;
    uint32_t uptimeMs;
};

typedef void (*ESPNowCommandCallback)(const RobotControlPacket &pkt);

class TinkerKit_ESPNow {
public:
    TinkerKit_ESPNow();

    bool begin(uint8_t wifiChannel = 1);
    bool addPeer(const uint8_t *macAddr);
    bool sendTelemetry(const RobotTelemetryPacket &telem, const uint8_t *destMac = nullptr);
    bool sendCommand(const RobotControlPacket &cmd, const uint8_t *destMac = nullptr);

    void setCommandCallback(ESPNowCommandCallback cb);
    bool hasNewCommand();
    RobotControlPacket getLastCommand();

    String getMyMac();

    static TinkerKit_ESPNow *instance;

private:
    uint8_t _broadcastMac[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    ESPNowCommandCallback _cmdCallback;
    RobotControlPacket _lastCmd;
    volatile bool _newCmdFlag;

    static void onDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len);
};
