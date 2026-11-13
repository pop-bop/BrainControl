#include "NPG_Controller.h"
#include <algorithm>
#include <cmath>

NPG_Controller NPGController;

// 9-Stage Second Order Sections (SOS) Biquad filter coefficients from scipy in handler.py:
// 1 DC Highpass (0.5 Hz) + 4 Notch (50Hz, 100Hz, 150Hz, 200Hz, Q=30) + 4th-order Bandpass (20-200 Hz)
static const float SOS_COEFFS[9][6] = {
    {9.9556697202e-01f, -1.9911339440e+00f, 9.9556697202e-01f, 1.0000000000e+00f, -1.9911142922e+00f, 9.9115359587e-01f},
    {9.8963617536e-01f, -1.6012649682e+00f, 9.8963617536e-01f, 1.0000000000e+00f, -1.6012649682e+00f, 9.7927235073e-01f},
    {9.7948276098e-01f, -6.0535363768e-01f, 9.7948276098e-01f, 1.0000000000e+00f, -6.0535363768e-01f, 9.5896552196e-01f},
    {9.6953125291e-01f, 5.9920326745e-01f, 9.6953125291e-01f, 1.0000000000e+00f, 5.9920326745e-01f, 9.3906250582e-01f},
    {9.5977356895e-01f, 1.5529462561e+00f, 9.5977356895e-01f, 1.0000000000e+00f, 1.5529462561e+00f, 9.1954713791e-01f},
    {3.0239179204e-01f, 6.0478358407e-01f, 3.0239179204e-01f, 1.0000000000e+00f, 9.9831023420e-01f, 2.7632581897e-01f},
    {1.0000000000e+00f, -2.0000000000e+00f, 1.0000000000e+00f, 1.0000000000e+00f, -1.5507491526e+00f, 6.0913190982e-01f},
    {1.0000000000e+00f, 2.0000000000e+00f, 1.0000000000e+00f, 1.0000000000e+00f, 1.3300538838e+00f, 6.5152624133e-01f},
    {1.0000000000e+00f, -2.0000000000e+00f, 1.0000000000e+00f, 1.0000000000e+00f, -1.7769466016e+00f, 8.3583028020e-01f},
};

// ============================================================================
// Arm Channel Tracker Implementation
// ============================================================================

void NPGChannelTracker::init(float chLimit) {
    limit = chLimit;
    base_alpha = 1.0f - expf(-1.0f / (NPG_FS * NPG_BASELINE_TAU_S));
    on_need = max(1, (int)(NPG_CLENCH_MIN_ON_S * NPG_FS));
    off_need = max(1, (int)(NPG_CLENCH_OFF_HANG_S * NPG_FS));

    calib_count = 0;
    mu = 0.0f;
    noise_sigma = 0.0f;
    mu_valid = false;

    on_th = chLimit;
    off_th = chLimit * NPG_OFF_RATIO;

    active = false;
    on_count = 0;
    off_count = 0;
    last_active_t = -1e9f;

    current_env = 0.0f;
    current_delta = 0.0f;
}

void NPGChannelTracker::step(float v, float t, bool calibrated) {
    current_env = v;

    if (t < NPG_SETTLE_SECONDS) {
        return;
    }

    if (!calibrated) {
        if (calib_count < NPG_CALIB_MAX_SAMPLES) {
            calib_buf[calib_count++] = v;
        }
        return;
    }

    if (!mu_valid) {
        if (calib_count > 0) {
            std::nth_element(calib_buf, calib_buf + calib_count / 2, calib_buf + calib_count);
            mu = calib_buf[calib_count / 2];

            // Calculate standard deviation / noise spread
            float var_sum = 0.0f;
            for (int i = 0; i < calib_count; ++i) {
                float diff = calib_buf[i] - mu;
                var_sum += diff * diff;
            }
            noise_sigma = sqrtf(var_sum / calib_count);
        } else {
            mu = 0.0f;
            noise_sigma = 0.0f;
        }
        mu_valid = true;
    }

    // Adaptive threshold: nominal limit protected by 3.5 * sigma noise floor
    float eff_limit = fmaxf(limit, 3.5f * noise_sigma);
    float delta = fmaxf(0.0f, v - mu);
    current_delta = delta;
    on_th = eff_limit;
    off_th = eff_limit * NPG_OFF_RATIO;

    if (active) {
        off_count = (delta < off_th) ? (off_count + 1) : 0;
        if (off_count >= off_need) {
            active = false;
            on_count = 0;
        }
    } else {
        on_count = (delta >= on_th) ? (on_count + 1) : 0;
        if (on_count >= on_need) {
            active = true;
            off_count = 0;
        }
    }

    if (active) {
        last_active_t = t;
    } else if (delta < off_th) {
        mu += base_alpha * (v - mu);
    }
}

bool NPGChannelTracker::isClenched(float t) {
    if (!mu_valid) return false;
    return active || ((NPG_CLENCH_HOLD_S > 0.0f) && ((t - last_active_t) < NPG_CLENCH_HOLD_S));
}

// ============================================================================
// BLE Callbacks
// ============================================================================

class NPGAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) override {
        String name = advertisedDevice.getName().c_str();
        name.toUpperCase();
        if (name.startsWith(NPG_DEVICE_PREFIX)) {
            Serial.printf("[NPG BLE] Discovered candidate: %s [%s]\n",
                          advertisedDevice.getName().c_str(),
                          advertisedDevice.getAddress().toString().c_str());
            BLEDevice::getScan()->stop();
            NPGController.m_targetDevice = new BLEAdvertisedDevice(advertisedDevice);
            NPGController.m_doConnect = true;
            NPGController.m_doScan = false;
        }
    }
};

static void npgNotifyCallback(
    BLERemoteCharacteristic* pBLERemoteCharacteristic,
    uint8_t* pData,
    size_t length,
    bool isNotify
) {
    NPGController.processPacket(pData, length);
}

class NPGClientCallback : public BLEClientCallbacks {
    void onConnect(BLEClient* pclient) override {
        Serial.println("[NPG BLE] Client Connected!");
    }

    void onDisconnect(BLEClient* pclient) override {
        Serial.println("[NPG BLE] Client Disconnected. Will rescan...");
        NPGController.m_connected = false;
        NPGController.m_doScan = true;
    }
};

// ============================================================================
// NPG Controller Main Implementation
// ============================================================================

NPG_Controller::NPG_Controller()
    : m_targetDevice(nullptr),
      m_pClient(nullptr),
      m_pDataChar(nullptr),
      m_pControlChar(nullptr),
      m_doConnect(false),
      m_connected(false),
      m_doScan(true),
      m_t(0.0f),
      m_calibrated(false),
      m_scale(2048.0f),
      m_scaleDetected(false),
      m_sampleCount(0),
      m_state(NPG_DECISION_NONE),
      m_inForwardMode(false),
      m_lastScanTime(0),
      m_lastPacketTime(0)
{
    memset(m_z1, 0, sizeof(m_z1));
    memset(m_z2, 0, sizeof(m_z2));
    m_envPower[0] = 0.0f;
    m_envPower[1] = 0.0f;

    m_leftTracker.init(NPG_LEFT_LIMIT);
    m_rightTracker.init(NPG_RIGHT_LIMIT);
}

void NPG_Controller::begin() {
    Serial.println("[NPG] Initializing BLE Client subsystem...");
    BLEDevice::init("TinkerBot_NPG");
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new NPGAdvertisedDeviceCallbacks());
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(true);

    m_doScan = true;
    m_inForwardMode = false;
    m_lastScanTime = millis();
}

void NPG_Controller::update() {
    if (m_doConnect) {
        if (connectToServer()) {
            Serial.println("[NPG] Successfully linked to NPG Lite!");
        } else {
            Serial.println("[NPG] Failed to link. Rescanning...");
            m_doScan = true;
        }
        m_doConnect = false;
    }

    if (m_doScan && !m_connected) {
        if (millis() - m_lastScanTime > 3000) {
            m_lastScanTime = millis();
            Serial.println("[NPG] Scanning for NPG Lite device...");
            BLEDevice::getScan()->start(5, false);
        }
    }
}

bool NPG_Controller::connectToServer() {
    if (!m_targetDevice) return false;

    Serial.printf("[NPG BLE] Connecting to %s...\n", m_targetDevice->getAddress().toString().c_str());
    m_pClient = BLEDevice::createClient();
    m_pClient->setClientCallbacks(new NPGClientCallback());

    if (!m_pClient->connect(m_targetDevice)) {
        return false;
    }

    // Connect to Remote Service & Characteristics
    BLERemoteService* pRemoteService = nullptr;
    auto* map = m_pClient->getServices();
    if (map) {
        for (auto& entry : *map) {
            pRemoteService = entry.second;
            if (pRemoteService->getCharacteristic(NPG_DATA_CHAR_UUID) != nullptr) {
                break;
            }
        }
    }

    if (!pRemoteService) {
        Serial.println("[NPG BLE] Target service not found.");
        m_pClient->disconnect();
        return false;
    }

    m_pDataChar = pRemoteService->getCharacteristic(NPG_DATA_CHAR_UUID);
    m_pControlChar = pRemoteService->getCharacteristic(NPG_CONTROL_CHAR_UUID);

    if (!m_pDataChar) {
        Serial.println("[NPG BLE] Data characteristic missing.");
        m_pClient->disconnect();
        return false;
    }

    // Register Notifications
    if (m_pDataChar->canNotify()) {
        m_pDataChar->registerForNotify(npgNotifyCallback);
        Serial.println("[NPG BLE] Data notifications registered.");
    }

    // Write "START" to Control Characteristic
    if (m_pControlChar && m_pControlChar->canWrite()) {
        const uint8_t startCmd[] = {'S', 'T', 'A', 'R', 'T'};
        m_pControlChar->writeValue((uint8_t*)startCmd, sizeof(startCmd), true);
        Serial.println("[NPG BLE] Sent START command to NPG Lite.");
    }

    m_connected = true;
    m_lastPacketTime = millis();
    return true;
}

float NPG_Controller::getCalibProgress() const {
    if (m_calibrated) return 1.0f;
    if (m_t <= NPG_SETTLE_SECONDS) return 0.0f;
    return constrain((m_t - NPG_SETTLE_SECONDS) / (NPG_CALIB_SECONDS - NPG_SETTLE_SECONDS), 0.0f, 1.0f);
}

const char* NPG_Controller::getStateString() const {
    switch (m_state) {
        case NPG_DECISION_BOTH:  return "FORWARD (Both Arms Clenched)";
        case NPG_DECISION_LEFT:  return "TURN LEFT (Left Arm Clenched)";
        case NPG_DECISION_RIGHT: return "TURN RIGHT (Right Arm Clenched)";
        case NPG_DECISION_NONE:
        default:
            return m_calibrated ? "STOPPED (Arms Relaxed)" : "CALIBRATING (Relax Arms)";
    }
}

float NPG_Controller::filterChannel(int ch, float x) {
    float curr = x;
    for (int s = 0; s < 9; s++) {
        float b0 = SOS_COEFFS[s][0];
        float b1 = SOS_COEFFS[s][1];
        float b2 = SOS_COEFFS[s][2];
        float a1 = SOS_COEFFS[s][4];
        float a2 = SOS_COEFFS[s][5];

        float y = b0 * curr + m_z1[ch][s];
        m_z1[ch][s] = b1 * curr - a1 * y + m_z2[ch][s];
        m_z2[ch][s] = b2 * curr - a2 * y;
        curr = y;
    }
    return curr;
}

void NPG_Controller::processSample(int16_t ch0, int16_t ch1) {
    m_sampleCount++;

    // Scale auto-detection (12-bit / 2048 vs 16-bit / 32768)
    if (!m_scaleDetected) {
        int16_t peak = max(abs(ch0), abs(ch1));
        if (peak > 4096) {
            m_scale = 32768.0f;
            m_scaleDetected = true;
        } else if (m_sampleCount > 300) {
            m_scale = 2048.0f;
            m_scaleDetected = true;
        }
    }

    // Normalize
    float norm_l = (float)ch0 / m_scale;
    float norm_r = (float)ch1 / m_scale;

    // Filter through 9-section SOS chain
    float emg_l = filterChannel(0, norm_l);
    float emg_r = filterChannel(1, norm_r);

    // Envelope filter (exponential power smoothing + sqrt)
    // alpha = 1 - exp(-1 / (500 * 0.05)) = 0.03921056f
    static const float al = 1.0f - expf(-1.0f / (NPG_FS * NPG_ENV_TAU_S));
    m_envPower[0] = (1.0f - al) * m_envPower[0] + al * (emg_l * emg_l);
    m_envPower[1] = (1.0f - al) * m_envPower[1] + al * (emg_r * emg_r);

    float env_l = sqrtf(fmaxf(0.0f, m_envPower[0]));
    float env_r = sqrtf(fmaxf(0.0f, m_envPower[1]));

    m_t += 1.0f / NPG_FS;
    if (m_t >= NPG_CALIB_SECONDS) {
        m_calibrated = true;
    }

    m_leftTracker.step(env_l, m_t, m_calibrated);
    m_rightTracker.step(env_r, m_t, m_calibrated);

    // State Decision
    if (!m_calibrated) {
        m_state = NPG_DECISION_NONE;
        return;
    }

    bool left_clench = m_leftTracker.isClenched(m_t);
    bool right_clench = m_rightTracker.isClenched(m_t);
    float dl = m_leftTracker.current_delta;
    float dr = m_rightTracker.current_delta;

    if (m_inForwardMode) {
        // While moving straight, only allow turning after detecting no signal.
        // Otherwise, as long as any signal is present, continue moving forward!
        if (!left_clench && !right_clench) {
            m_inForwardMode = false;
            m_state = NPG_DECISION_NONE;
        } else {
            m_state = NPG_DECISION_BOTH;
        }
    } else {
        if (left_clench && right_clench) {
            // Crosstalk & Dominance filter: prevent accidental forward lunges during turns
            if (dl > 1.8f * dr) {
                m_state = NPG_DECISION_LEFT;
            } else if (dr > 1.8f * dl) {
                m_state = NPG_DECISION_RIGHT;
            } else {
                m_state = NPG_DECISION_BOTH;
                m_inForwardMode = true; // Lock into forward mode
            }
        } else if (left_clench) {
            m_state = NPG_DECISION_LEFT;
        } else if (right_clench) {
            m_state = NPG_DECISION_RIGHT;
        } else {
            m_state = NPG_DECISION_NONE;
        }
    }
}

void NPG_Controller::processPacket(uint8_t* data, size_t length) {
    m_lastPacketTime = millis();
    for (size_t i = 0; i + NPG_SAMPLE_LEN <= length; i += NPG_SAMPLE_LEN) {
        uint8_t* chunk = data + i;
        // Big-endian 16-bit signed
        int16_t ch0 = (int16_t)(((uint16_t)chunk[1] << 8) | chunk[2]);
        int16_t ch1 = (int16_t)(((uint16_t)chunk[3] << 8) | chunk[4]);
        processSample(ch0, ch1);
    }
}
