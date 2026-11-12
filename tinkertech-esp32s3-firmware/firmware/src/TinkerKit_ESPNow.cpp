#include "TinkerKit_ESPNow.h"

TinkerKit_ESPNow *TinkerKit_ESPNow::instance = nullptr;

TinkerKit_ESPNow::TinkerKit_ESPNow() : _cmdCallback(nullptr), _newCmdFlag(false) {
    instance = this;
    memset(&_lastCmd, 0, sizeof(_lastCmd));
}

void TinkerKit_ESPNow::onDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
    if (!instance) return;

    if (len == sizeof(RobotControlPacket)) {
        memcpy(&instance->_lastCmd, incomingData, sizeof(RobotControlPacket));
        instance->_newCmdFlag = true;

        if (instance->_cmdCallback) {
            instance->_cmdCallback(instance->_lastCmd);
        }
    }
}

bool TinkerKit_ESPNow::begin(uint8_t wifiChannel) {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() != ESP_OK) {
        return false;
    }

    esp_now_register_recv_cb(esp_now_recv_cb_t(onDataRecv));

    // Add broadcast peer by default
    addPeer(_broadcastMac);

    return true;
}

bool TinkerKit_ESPNow::addPeer(const uint8_t *macAddr) {
    if (esp_now_is_peer_exist(macAddr)) {
        return true;
    }
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, macAddr, 6);
    peerInfo.channel = 0; // Same as current Wi-Fi channel
    peerInfo.encrypt = false;
    return (esp_now_add_peer(&peerInfo) == ESP_OK);
}

bool TinkerKit_ESPNow::sendTelemetry(const RobotTelemetryPacket &telem, const uint8_t *destMac) {
    const uint8_t *target = destMac ? destMac : _broadcastMac;
    addPeer(target);
    esp_err_t result = esp_now_send(target, (const uint8_t *)&telem, sizeof(telem));
    return (result == ESP_OK);
}

bool TinkerKit_ESPNow::sendCommand(const RobotControlPacket &cmd, const uint8_t *destMac) {
    const uint8_t *target = destMac ? destMac : _broadcastMac;
    addPeer(target);
    esp_err_t result = esp_now_send(target, (const uint8_t *)&cmd, sizeof(cmd));
    return (result == ESP_OK);
}

void TinkerKit_ESPNow::setCommandCallback(ESPNowCommandCallback cb) {
    _cmdCallback = cb;
}

bool TinkerKit_ESPNow::hasNewCommand() {
    return _newCmdFlag;
}

RobotControlPacket TinkerKit_ESPNow::getLastCommand() {
    _newCmdFlag = false;
    return _lastCmd;
}

String TinkerKit_ESPNow::getMyMac() {
    return WiFi.macAddress();
}
