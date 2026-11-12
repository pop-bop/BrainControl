#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "TinkerKit.h"

// WiFi Credentials provided by user
const char* WIFI_SSID = "0xMakers_Dev";
const char* WIFI_PASS = "Welc0me@123";

// SoftAP Fallback
const char* AP_SSID = "TinkerBot";
const char* AP_PASS = "12345678";

// UDP Server on Port 8888
WiFiUDP udp;
const uint16_t UDP_PORT = 8888;
char packetBuffer[256];

// Wireless Remote & Failsafe
uint32_t lastRemoteCmdTime = 0;
bool isRemoteActive = false;
String lastAction = "IDLE (Ready)";
String lastSource = "INIT";

// Police Strobe Flasher State
bool policeLights = true;
uint8_t lastPoliceState = 0xFF; // Cache to avoid I2C bus flooding

// Robotic Arm Servos (Servo 0 = Lift / Shoulder, Servo 1 = Claw / Gripper)
uint8_t armLiftAngle = 90;
uint8_t armClawAngle = 90;

// UI update timers
uint32_t lastUiUpdate = 0;

void executeMotion(RobotDirection dir, uint8_t speed, const char* sourceLabel) {
    lastSource = sourceLabel;
    if (dir == DIR_STOP || speed == 0) {
        TinkerKit.mainboard.stop();
        lastAction = "STOPPED";
        isRemoteActive = false;
        Serial.printf("[ROBOT] %s STOP\n", sourceLabel);
    } else {
        isRemoteActive = true;
        lastRemoteCmdTime = millis();
        if (dir == DIR_FORWARD) {
            TinkerKit.mainboard.move(DIR_FORWARD, speed);
            lastAction = "FORWARD (" + String(speed) + "%)";
            Serial.printf("[ROBOT] %s FORWARD speed=%d\n", sourceLabel, speed);
        } else if (dir == DIR_BACKWARD) {
            TinkerKit.mainboard.move(DIR_BACKWARD, speed);
            lastAction = "BACKWARD (" + String(speed) + "%)";
            Serial.printf("[ROBOT] %s BACKWARD speed=%d\n", sourceLabel, speed);
        } else if (dir == DIR_LEFT) {
            TinkerKit.mainboard.move(DIR_LEFT, speed);
            lastAction = "TURN LEFT (" + String(speed) + "%)";
            Serial.printf("[ROBOT] %s TURN LEFT speed=%d\n", sourceLabel, speed);
        } else if (dir == DIR_RIGHT) {
            TinkerKit.mainboard.move(DIR_RIGHT, speed);
            lastAction = "TURN RIGHT (" + String(speed) + "%)";
            Serial.printf("[ROBOT] %s TURN RIGHT speed=%d\n", sourceLabel, speed);
        }
    }
}

void executeCommand(const String& cmd, const char* source = "[REMOTE]") {
    if (cmd == "stop") {
        executeMotion(DIR_STOP, 0, source);
    } else if (cmd.startsWith("steer ")) {
        // steer <throttle> <turn> where throttle/turn are -100..100
        int throttle = 0, turn = 0;
        if (sscanf(cmd.c_str(), "steer %d %d", &throttle, &turn) == 2) {
            int l = constrain(throttle + turn, -100, 100);
            int r = constrain(throttle - turn, -100, 100);
            lastSource = source;
            if (l == 0 && r == 0) {
                TinkerKit.mainboard.stop();
                lastAction = "STOPPED";
                isRemoteActive = false;
            } else {
                TinkerKit.mainboard.drive(l, r);
                char actBuf[32];
                snprintf(actBuf, sizeof(actBuf), "STEER T:%d T:%d (L:%d R:%d)", throttle, turn, l, r);
                lastAction = String(actBuf);
                isRemoteActive = true;
                lastRemoteCmdTime = millis();
            }
        }
    } else if (cmd.startsWith("drive ")) {
        // drive <leftSpeed> <rightSpeed> (-100..100 each)
        int l = 0, r = 0;
        if (sscanf(cmd.c_str(), "drive %d %d", &l, &r) == 2) {
            l = constrain(l, -100, 100);
            r = constrain(r, -100, 100);
            lastSource = source;
            if (l == 0 && r == 0) {
                TinkerKit.mainboard.stop();
                lastAction = "STOPPED";
                isRemoteActive = false;
            } else {
                TinkerKit.mainboard.drive(l, r);
                char actBuf[32];
                snprintf(actBuf, sizeof(actBuf), "DRIVE L:%d R:%d", l, r);
                lastAction = String(actBuf);
                isRemoteActive = true;
                lastRemoteCmdTime = millis();
            }
        }
    } else if (cmd.startsWith("fwd")) {
        int speed = cmd.substring(4).toInt();
        if (speed <= 0) speed = 60;
        executeMotion(DIR_FORWARD, speed, source);
    } else if (cmd.startsWith("rev")) {
        int speed = cmd.substring(4).toInt();
        if (speed <= 0) speed = 60;
        executeMotion(DIR_BACKWARD, speed, source);
    } else if (cmd.startsWith("left")) {
        int speed = cmd.substring(5).toInt();
        if (speed <= 0) speed = 60;
        executeMotion(DIR_LEFT, speed, source);
    } else if (cmd.startsWith("right")) {
        int speed = cmd.substring(6).toInt();
        if (speed <= 0) speed = 60;
        executeMotion(DIR_RIGHT, speed, source);
    } else if (cmd.startsWith("servo ")) {
        int id = 0, angle = 90;
        if (sscanf(cmd.c_str(), "servo %d %d", &id, &angle) == 2) {
            angle = constrain(angle, 0, 180);
            if (id == 0) {
                armLiftAngle = angle;
                TinkerKit.mainboard.setServoAngle(0, armLiftAngle);
            } else if (id == 1) {
                armClawAngle = angle;
                TinkerKit.mainboard.setServoAngle(1, armClawAngle);
            }
            char actBuf[32];
            snprintf(actBuf, sizeof(actBuf), "SERVO %d -> %d deg", id, angle);
            lastAction = String(actBuf);
            lastSource = source;
            Serial.printf("[ARM] Servo %d set to %d deg\n", id, angle);
        }
    } else if (cmd.startsWith("arm ")) {
        int lift = 90, claw = 90;
        if (sscanf(cmd.c_str(), "arm %d %d", &lift, &claw) == 2) {
            armLiftAngle = constrain(lift, 0, 180);
            armClawAngle = constrain(claw, 0, 180);
            TinkerKit.mainboard.setServoAngle(0, armLiftAngle);
            TinkerKit.mainboard.setServoAngle(1, armClawAngle);
            char actBuf[32];
            snprintf(actBuf, sizeof(actBuf), "ARM L:%d C:%d", armLiftAngle, armClawAngle);
            lastAction = String(actBuf);
            lastSource = source;
            Serial.printf("[ARM] Lift: %d deg, Claw: %d deg\n", armLiftAngle, armClawAngle);
        }
    } else if (cmd.startsWith("lift")) {
        String val = cmd.substring(4);
        val.trim();
        if (val == "up" || val == "+") {
            armLiftAngle = min(180, (int)armLiftAngle + 10);
        } else if (val == "down" || val == "-") {
            armLiftAngle = max(0, (int)armLiftAngle - 10);
        } else if (val.length() > 0) {
            armLiftAngle = constrain(val.toInt(), 0, 180);
        }
        TinkerKit.mainboard.setServoAngle(0, armLiftAngle);
        lastAction = "LIFT: " + String(armLiftAngle) + " deg";
        lastSource = source;
        Serial.printf("[ARM] Lift set to %d deg\n", armLiftAngle);
    } else if (cmd.startsWith("claw")) {
        String val = cmd.substring(4);
        val.trim();
        if (val == "open") {
            armClawAngle = 140;
        } else if (val == "close") {
            armClawAngle = 40;
        } else if (val == "+" || val == "up") {
            armClawAngle = min(180, (int)armClawAngle + 10);
        } else if (val == "-" || val == "down") {
            armClawAngle = max(0, (int)armClawAngle - 10);
        } else if (val.length() > 0) {
            armClawAngle = constrain(val.toInt(), 0, 180);
        }
        TinkerKit.mainboard.setServoAngle(1, armClawAngle);
        lastAction = "CLAW: " + String(armClawAngle) + " deg";
        lastSource = source;
        Serial.printf("[ARM] Claw set to %d deg\n", armClawAngle);
    } else if (cmd == "release" || cmd == "arm release") {
        TinkerKit.mainboard.releaseServo(0);
        TinkerKit.mainboard.releaseServo(1);
        lastAction = "ARM RELEASED";
        lastSource = source;
        Serial.println("[ARM] Servos Released");
    } else if (cmd == "dist") {
        float d = TinkerKit.ultrasonic.getDistanceCM();
        uint32_t raw = TinkerKit.ultrasonic.getRawDistance();
        Serial.printf("[ULTRASONIC] Distance: %.1f cm (raw: %u)\n", d, raw);
    } else if (cmd == "ip") {
        Serial.printf("STA IP: %s | AP IP: %s | Status: %d\n", WiFi.localIP().toString().c_str(), WiFi.softAPIP().toString().c_str(), WiFi.status());
    } else if (cmd == "police" || cmd == "police on") {
        policeLights = true;
        Serial.println("[LIGHTS] Police Strobe ENABLED");
    } else if (cmd == "police off") {
        policeLights = false;
        TinkerKit.mainboard.setLightColor(0, 0, 0, 0);
        TinkerKit.ultrasonic.setRGB(0, 0, 0, 0);
        Serial.println("[LIGHTS] Police Strobe DISABLED");
    } else if (cmd.startsWith("color")) {
        policeLights = false;
        int r = 0, g = 0, b = 0;
        sscanf(cmd.c_str(), "color %d %d %d", &r, &g, &b);
        TinkerKit.mainboard.setLightColor(r, g, b, 80);
        TinkerKit.ultrasonic.setRGB(r, g, b, 80);
    }
}

void updatePoliceLights() {
    if (!policeLights) return;

    // Authentic emergency cruiser strobe: 480ms cycle
    // 0..240ms: Mainboard RED burst (3 fast pulses) + Ultrasonic BLUE burst
    // 240..480ms: Mainboard BLUE burst (3 fast pulses) + Ultrasonic RED burst
    uint32_t t = millis() % 480;
    uint8_t state = 0; // 0: Dark, 1: Red Phase, 2: Blue Phase

    if (t < 240) {
        if ((t < 40) || (t >= 80 && t < 120) || (t >= 160 && t < 200)) {
            state = 1;
        }
    } else {
        uint32_t tb = t - 240;
        if ((tb < 40) || (tb >= 80 && tb < 120) || (tb >= 160 && tb < 200)) {
            state = 2;
        }
    }

    if (state != lastPoliceState) {
        lastPoliceState = state;
        if (state == 1) {
            TinkerKit.mainboard.setLightColor(255, 0, 0, 200);   // Mainboard Red
            TinkerKit.ultrasonic.setRGB(0, 50, 255, 200);        // Ultrasonic Blue
        } else if (state == 2) {
            TinkerKit.mainboard.setLightColor(0, 50, 255, 200);   // Mainboard Blue
            TinkerKit.ultrasonic.setRGB(255, 0, 0, 200);         // Ultrasonic Red
        } else {
            TinkerKit.mainboard.setLightColor(0, 0, 0, 0);
            TinkerKit.ultrasonic.setRGB(0, 0, 0, 0);
        }
    }
}

void handleUdp() {
    int packetSize = udp.parsePacket();
    if (packetSize) {
        int len = udp.read(packetBuffer, sizeof(packetBuffer) - 1);
        if (len > 0) {
            packetBuffer[len] = '\0';
            String cmd = String(packetBuffer);
            cmd.trim();

            executeCommand(cmd, "[REMOTE]");

            // Quick ACK with current telemetry including arm positions
            float d = TinkerKit.ultrasonic.getDistanceCM();
            udp.beginPacket(udp.remoteIP(), udp.remotePort());
            udp.printf("ACK: %s | DIST: %.1f cm | ARM: L:%d C:%d | ACT: %s\n", cmd.c_str(), d, armLiftAngle, armClawAngle, lastAction.c_str());
            udp.endPacket();
        }
    }
}

void handleSerialCommands() {
    if (!Serial.available()) return;
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() > 0) {
        executeCommand(cmd, "[SERIAL]");
        Serial.println("OK: " + cmd);
    }
}

void drawDashboard() {
    auto &gfx = TinkerKit.display.gfx;

    // Header bar (y: 0..24)
    gfx.fillRect(0, 0, 320, 24, gfx.color565(20, 25, 45));
    gfx.setTextSize(1);
    gfx.setTextColor(TFT_CYAN, gfx.color565(20, 25, 45));
    gfx.drawString("TINKERTECH NPG POLICE BOT", 8, 6);

    // Battery percentage & voltage
    int8_t batt = -1;
    float volts = 0.0f;
    if (TinkerKit.mainboard.isAvailable()) {
        batt = TinkerKit.mainboard.getBatteryPercentage();
        volts = TinkerKit.mainboard.getBatteryVoltage();
    }
    char battStr[32];
    if (batt >= 0) snprintf(battStr, sizeof(battStr), "BAT: %d%% %.1fV", batt, volts);
    else snprintf(battStr, sizeof(battStr), "BAT: --");
    gfx.setTextColor(TFT_GREENYELLOW, gfx.color565(20, 25, 45));
    gfx.drawString(battStr, 220, 6);

    // Network & Connection Card (y: 28..74)
    gfx.drawRoundRect(4, 28, 312, 46, 4, TFT_DARKCYAN);
    gfx.fillRect(5, 29, 310, 44, gfx.color565(12, 18, 28));
    gfx.setTextColor(TFT_WHITE, gfx.color565(12, 18, 28));

    String ipStr = "Connecting to " + String(WIFI_SSID) + "...";
    if (WiFi.status() == WL_CONNECTED) {
        ipStr = "STA: " + WiFi.localIP().toString() + " : " + String(UDP_PORT);
    } else {
        ipStr = "AP: " + WiFi.softAPIP().toString() + " (TinkerBot:8888)";
    }
    gfx.drawString("WIFI UDP TARGET:", 10, 33);
    gfx.setTextColor(TFT_GREEN, gfx.color565(12, 18, 28));
    gfx.drawString(ipStr.c_str(), 10, 48);

    // Police Strobe Flashing Badge
    if (policeLights) {
        bool isRedPhase = (lastPoliceState == 1);
        uint16_t strobeBadgeCol = isRedPhase ? gfx.color565(230, 30, 30) : gfx.color565(30, 100, 255);
        gfx.fillRoundRect(200, 33, 110, 22, 3, strobeBadgeCol);
        gfx.setTextColor(TFT_WHITE, strobeBadgeCol);
        gfx.drawString("POLICE ACTIVE", 206, 38);
    }

    // Active Remote Command Banner (y: 78..130)
    uint16_t bannerBg = isRemoteActive ? gfx.color565(40, 25, 10) : gfx.color565(15, 20, 25);
    uint16_t bannerBorder = isRemoteActive ? TFT_YELLOW : TFT_DARKGRAY;
    gfx.drawRoundRect(4, 78, 312, 52, 4, bannerBorder);
    gfx.fillRect(5, 79, 310, 50, bannerBg);

    gfx.setTextColor(TFT_LIGHTGRAY, bannerBg);
    char srcHdr[64];
    snprintf(srcHdr, sizeof(srcHdr), "ACTIVE MOTION COMMAND (%s):", lastSource.c_str());
    gfx.drawString(srcHdr, 10, 83);

    gfx.setTextSize(2);
    gfx.setTextColor(isRemoteActive ? TFT_GREENYELLOW : TFT_WHITE, bannerBg);
    gfx.drawString(lastAction.c_str(), 10, 100);
    gfx.setTextSize(1);

    // Section 1: Ultrasonic Distance (y: 134..186, left half)
    float distCm = -1.0f;
    uint32_t rawDist = 0;
    if (TinkerKit.ultrasonic.isAvailable()) {
        distCm = TinkerKit.ultrasonic.getDistanceCM();
        rawDist = TinkerKit.ultrasonic.getRawDistance();
    }
    gfx.drawRoundRect(4, 134, 153, 52, 4, TFT_DARKGRAY);
    gfx.fillRect(5, 135, 151, 50, gfx.color565(15, 18, 22));
    gfx.setTextColor(TFT_CYAN, gfx.color565(15, 18, 22));
    gfx.drawString("ULTRASONIC SENSOR", 10, 138);

    char distStr[24];
    if (distCm > 0) snprintf(distStr, sizeof(distStr), "%.1f cm", distCm);
    else snprintf(distStr, sizeof(distStr), "--.- cm");
    gfx.setTextSize(2);
    gfx.setTextColor(TFT_WHITE, gfx.color565(15, 18, 22));
    gfx.drawString(distStr, 10, 152);
    gfx.setTextSize(1);
    char rawStr[32];
    snprintf(rawStr, sizeof(rawStr), "raw: %u", rawDist);
    gfx.setTextColor(TFT_DARKGRAY, gfx.color565(15, 18, 22));
    gfx.drawString(rawStr, 10, 172);

    // Section 2: IMU (y: 134..186, right half)
    float pitch = 0, roll = 0;
    if (TinkerKit.imu.isAvailable()) {
        TinkerKit.imu.getPitchRoll(pitch, roll);
    }
    gfx.drawRoundRect(163, 134, 153, 52, 4, TFT_DARKGRAY);
    gfx.fillRect(164, 135, 151, 50, gfx.color565(15, 18, 22));
    gfx.setTextColor(TFT_MAGENTA, gfx.color565(15, 18, 22));
    gfx.drawString("ORIENTATION (IMU)", 170, 138);
    char imuStr[32];
    snprintf(imuStr, sizeof(imuStr), "P:%.0f  R:%.0f", pitch, roll);
    gfx.setTextSize(2);
    gfx.setTextColor(TFT_WHITE, gfx.color565(15, 18, 22));
    gfx.drawString(imuStr, 170, 152);
    gfx.setTextSize(1);

    // Section 3: Robotic Arm (y: 190..236)
    gfx.drawRoundRect(4, 190, 312, 46, 4, TFT_DARKCYAN);
    gfx.fillRect(5, 191, 310, 44, gfx.color565(12, 22, 30));
    gfx.setTextColor(TFT_CYAN, gfx.color565(12, 22, 30));
    gfx.drawString("ROBOTIC ARM (SERVOS 0 & 1):", 10, 195);
    char armStr[48];
    snprintf(armStr, sizeof(armStr), "LIFT: %d deg   CLAW: %d deg", armLiftAngle, armClawAngle);
    gfx.setTextSize(2);
    gfx.setTextColor(TFT_YELLOW, gfx.color565(12, 22, 30));
    gfx.drawString(armStr, 10, 210);
    gfx.setTextSize(1);
}

void setup() {
    Serial.begin(115200);
    delay(500);

    // Initialize hardware & display
    TinkerKit.begin(true, false);

    // Initialize robotic arm servos to default center (90 deg)
    if (TinkerKit.mainboard.isAvailable()) {
        TinkerKit.mainboard.setServoAngle(0, armLiftAngle);
        TinkerKit.mainboard.setServoAngle(1, armClawAngle);
    }

    // Start WiFi in Dual AP + Station mode
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID, AP_PASS);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    // Start UDP Server
    udp.begin(UDP_PORT);

    Serial.println("\n==========================================");
    Serial.println("TinkerTech NPG Police Bot Ready");
    Serial.printf("Connecting to SSID: %s\n", WIFI_SSID);
    Serial.printf("UDP Listening Port: %d\n", UDP_PORT);
    Serial.println("Police Strobe Lights: ACTIVE");
    Serial.println("==========================================");
}

void loop() {
    static bool wasConnected = false;
    if (WiFi.status() == WL_CONNECTED && !wasConnected) {
        wasConnected = true;
        Serial.printf("\n[WiFi] Connected to %s! IP: %s\n", WIFI_SSID, WiFi.localIP().toString().c_str());
    }

    // High-speed non-blocking police strobe
    updatePoliceLights();

    // Process incoming UDP packets (Teleop & NPG Lite)
    handleUdp();

    // Process serial console commands
    handleSerialCommands();

    // Auto-stop failsafe: stop after 600ms without fresh command
    if (isRemoteActive && (millis() - lastRemoteCmdTime > 600)) {
        TinkerKit.mainboard.stop();
        isRemoteActive = false;
        lastAction = "STOPPED (Failsafe)";
    }

    // Refresh display at ~10 Hz
    if (millis() - lastUiUpdate > 100) {
        lastUiUpdate = millis();
        drawDashboard();
    }

    delay(2);
}
