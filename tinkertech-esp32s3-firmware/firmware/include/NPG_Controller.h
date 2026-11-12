#pragma once

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>

// ============================================================================
// NPG Lite BLE & Signal Processing Constants (from handler.py & main.py)
// ============================================================================
#define NPG_DEVICE_PREFIX       "NPG"
#define NPG_DATA_CHAR_UUID      "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define NPG_CONTROL_CHAR_UUID   "0000ff01-0000-1000-8000-00805f9b34fb"

#define NPG_SAMPLE_LEN          7
#define NPG_FS                  500.0f
#define NPG_LEFT_CH             0
#define NPG_RIGHT_CH            1

// Preconfigured thresholds and constants directly from handler.py
#define NPG_LEFT_LIMIT          0.045f
#define NPG_RIGHT_LIMIT         0.045f
#define NPG_OFF_RATIO           1.0f

#define NPG_SETTLE_SECONDS      1.0f
#define NPG_CALIB_SECONDS       3.0f

#define NPG_CLENCH_MIN_ON_S     0.04f   // 20 samples @ 500Hz
#define NPG_CLENCH_OFF_HANG_S   0.00f   // 50 samples @ 500Hz
#define NPG_CLENCH_HOLD_S       0.00f   // 150 ms
#define NPG_BASELINE_TAU_S      30.0f
#define NPG_ENV_TAU_S           0.05f

#define NPG_CALIB_MAX_SAMPLES   1200

enum NPGDecisionState {
    NPG_DECISION_NONE = 0,
    NPG_DECISION_LEFT,
    NPG_DECISION_RIGHT,
    NPG_DECISION_BOTH
};

// Arm channel tracking state
struct NPGChannelTracker {
    float limit;
    float base_alpha;
    int on_need;
    int off_need;

    float calib_buf[NPG_CALIB_MAX_SAMPLES];
    int calib_count;
    float mu;
    bool mu_valid;

    float on_th;
    float off_th;

    bool active;
    int on_count;
    int off_count;
    float last_active_t;

    float current_env;
    float current_delta;

    void init(float chLimit);
    void step(float v, float t, bool calibrated);
    bool isClenched(float t);
};

class NPG_Controller {
public:
    NPG_Controller();
    void begin();
    void update();

    bool isConnected() const { return m_connected; }
    bool isScanning() const { return m_doScan; }
    bool isCalibrated() const { return m_calibrated; }
    float getCalibProgress() const;

    NPGDecisionState getCurrentState() const { return m_state; }
    const char* getStateString() const;

    float getLeftEnv() const { return m_leftTracker.current_env; }
    float getRightEnv() const { return m_rightTracker.current_env; }
    float getLeftMu() const { return m_leftTracker.mu; }
    float getRightMu() const { return m_rightTracker.mu; }
    float getLeftLimit() const { return m_leftTracker.limit; }
    float getRightLimit() const { return m_rightTracker.limit; }

    // Internal notification handler
    void processPacket(uint8_t* data, size_t length);

private:
    bool connectToServer();
    void processSample(int16_t ch0, int16_t ch1);
    float filterChannel(int ch, float x);

    BLEAdvertisedDevice* m_targetDevice;
    BLEClient*           m_pClient;
    BLERemoteCharacteristic* m_pDataChar;
    BLERemoteCharacteristic* m_pControlChar;

    bool m_doConnect;
    bool m_connected;
    bool m_doScan;

    // Signal processing states
    float m_t;
    bool  m_calibrated;
    float m_scale;
    bool  m_scaleDetected;
    int   m_sampleCount;

    // DF2T Filter states (2 channels, 9 SOS stages)
    float m_z1[2][9];
    float m_z2[2][9];

    // Envelope states
    float m_envPower[2];

    NPGChannelTracker m_leftTracker;
    NPGChannelTracker m_rightTracker;
    NPGDecisionState  m_state;

    uint32_t m_lastScanTime;
    uint32_t m_lastPacketTime;

    friend class NPGAdvertisedDeviceCallbacks;
    friend class NPGClientCallback;
};

extern NPG_Controller NPGController;
