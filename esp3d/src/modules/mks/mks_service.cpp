/*
  mks_service.cpp - MKS communication service functions class

  Copyright (c) 2014 Luc Lebosse. All rights reserved.

  This code is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This code is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with This code; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "../../include/esp3d_config.h"
#if COMMUNICATION_PROTOCOL == MKS_SERIAL
#include "../../core/esp3d_message.h"
#include "../../core/esp3d_log.h" // Added for enhanced logging
#include "../../core/esp3d_settings.h"
#include "../http/http_server.h"
#include "../network/netconfig.h"
#include "../serial/serial_service.h"
#include "../telnet/telnet_server.h"
#include "../wifi/wificonfig.h"
#include "mks_service.h"

// Flag Pins
#define ESP_FLAG_PIN 0
#define BOARD_FLAG_PIN 4
// Flag pins values
#define BOARD_READY_FLAG_VALUE LOW

// Frame offsets
#define MKS_FRAME_HEAD_OFFSET 0
#define MKS_FRAME_TYPE_OFFSET 1
#define MKS_FRAME_DATALEN_OFFSET 2
#define MKS_FRAME_DATA_OFFSET 4

// Frame flags
#define MKS_FRAME_HEAD_FLAG (char)0xa5
#define MKS_FRAME_TAIL_FLAG (char)0xfc

// Network states
#define MKS_FRAME_NETWORK_OK_STATE (char)0x0a
#define MKS_FRAME_NETWORK_FAIL_STATE (char)0x05
#define MKS_FRAME_NETWORK_ERROR_STATE (char)0x0e

// Network modes
#define MKS_FRAME_NETWORK_AP_MODE (char)0x01
#define MKS_FRAME_NETWORK_STA_MODE (char)0x02
#define MKS_FRAME_NETWORK_APSTA_MODE (char)0x03

// Cloud states
#define MKS_FRAME_CLOUD_BINDED_STATE (char)0x12
#define MKS_FRAME_CLOUD_NOT_BINDED_STATE (char)0x13
#define MKS_FRAME_CLOUD_DISCONNECTED_STATE (char)0x10
#define MKS_FRAME_CLOUD_DISABLED_STATE (char)0x00

// Data types
#define MKS_FRAME_DATA_NETWORK_TYPE (char)0x0
#define MKS_FRAME_DATA_COMMAND_TYPE (char)0x1
#define MKS_FRAME_DATA_FIRST_FRAGMENT_TYPE (char)0x2
#define MKS_FRAME_DATA_FRAGMENT_TYPE (char)0x3
#define MKS_FRAME_DATA_HOTSPOTS_LIST_TYPE (char)0x4
#define MKS_FRAME_DATA_STATIC_IP_TYPE (char)0x5

#define MKS_TYPE_NET (char)0x0
#define MKS_TYPE_PRINTER (char)0x1
#define MKS_TYPE_TRANSFER (char)0x2
#define MKS_TYPE_EXCEPTION (char)0x3
#define MKS_TYPE_CLOUD (char)0x4
#define MKS_TYPE_UNBIND (char)0x5
#define MKS_TYPE_WID (char)0x6
#define MKS_TYPE_SCAN_WIFI (char)0x7
#define MKS_TYPE_MANUAL_IP (char)0x8
#define MKS_TYPE_WIFI_CTRL (char)0x9

#define CONNECT_STA 0x1
#define DISCONNECT_STA 0x2
#define REMOVE_STA_INFO 0x3

#define UNKNOW_STATE 0x0
#define ERROR_STATE 0x1
#define SUCCESS_STATE 0x2

#define NB_HOTSPOT_MAX 15

// Timeouts
#define FRAME_WAIT_TO_SEND_TIMEOUT 2000
#define ACK_TIMEOUT 5000
#define NET_FRAME_REFRESH_TIME 10000

#define UPLOAD_BAUD_RATE 1958400

bool MKSService::_started = false;
uint8_t MKSService::_frame[MKS_FRAME_SIZE] = {0};
char MKSService::_moduleId[22] = {0};
uint8_t MKSService::_uploadStatus = UNKNOW_STATE;
long MKSService::_commandBaudRate = 115200;
bool MKSService::_uploadMode = false;

bool MKSService::isHead(const char c) { return (c == MKS_FRAME_HEAD_FLAG); }
bool MKSService::isTail(const char c) { return (c == MKS_FRAME_TAIL_FLAG); }
bool MKSService::isCommand(const char c) { return (c == MKS_TYPE_TRANSFER); }
bool MKSService::isFrame(const char c) {
  if ((c >= MKS_TYPE_NET) && (c <= MKS_TYPE_WIFI_CTRL)) {
    return true;
  }
  return false;
}

bool MKSService::dispatch(ESP3DMessage *message) {
  if (!message || !_started) {
    esp3d_log_e("Invalid message or service not started");
    return false;
  }
  if (message->size > 0 && message->data) {
    if (sendGcodeFrame((const char *)message->data)) {
      esp3d_message_manager.deleteMsg(message);
      return true;
    }
    esp3d_log_e("Failed to send G-code frame");
  }
  return false;
}

bool MKSService::begin() {
  esp3d_log("Starting MKS service");
  pinMode(BOARD_FLAG_PIN, INPUT);
  pinMode(ESP_FLAG_PIN, OUTPUT);
  _started = true;
  sprintf(_moduleId, "HJNLM000%02X%02X%02X%02X%02X%02X", WiFi.macAddress()[0],
          WiFi.macAddress()[1], WiFi.macAddress()[2], WiFi.macAddress()[3],
          WiFi.macAddress()[4], WiFi.macAddress()[5]);
  esp3d_log("Module ID: %s", _moduleId);
  commandMode(true);
  return true;
}

void MKSService::commandMode(bool fromSettings) {
  if (fromSettings) {
    _commandBaudRate = ESP3DSettings::readUint32(ESP_BAUD_RATE);
    esp3d_log("Setting command baud rate to %ld", _commandBaudRate);
  }
  esp3d_log("Switching to command mode");
  _uploadMode = false;
  esp3d_serial_service.updateBaudRate(_commandBaudRate);
}

void MKSService::uploadMode() {
  esp3d_log("Switching to upload mode");
  _uploadMode = true;
  esp3d_serial_service.updateBaudRate(UPLOAD_BAUD_RATE);
}

uint MKSService::getFragmentID(uint32_t fragmentNumber, bool isLast) {
  esp3d_log("Fragment: %d %s", fragmentNumber, isLast ? "is last" : "");
  if (isLast) {
    fragmentNumber |= (1 << 31);
  } else {
    fragmentNumber &= ~(1 << 31);
  }
  esp3d_log("Fragment ID: %d", fragmentNumber);
  return fragmentNumber;
}

bool MKSService::sendFirstFragment(const char *filename, size_t filesize) {
  uint fileNameLen = strlen(filename);
  uint dataLen = fileNameLen + 5;
  clearFrame();
  _frame[MKS_FRAME_HEAD_OFFSET] = MKS_FRAME_HEAD_FLAG;
  _frame[MKS_FRAME_TYPE_OFFSET] = MKS_FRAME_DATA_FIRST_FRAGMENT_TYPE;
  _frame[MKS_FRAME_DATALEN_OFFSET] = dataLen & 0xff;
  _frame[MKS_FRAME_DATALEN_OFFSET + 1] = dataLen >> 8;
  _frame[MKS_FRAME_DATA_OFFSET] = strlen(filename);
  _frame[MKS_FRAME_DATA_OFFSET + 1] = filesize & 0xff;
  _frame[MKS_FRAME_DATA_OFFSET + 2] = (filesize >> 8) & 0xff;
  _frame[MKS_FRAME_DATA_OFFSET + 3] = (filesize >> 16) & 0xff;
  _frame[MKS_FRAME_DATA_OFFSET + 4] = (filesize >> 24) & 0xff;
  strncpy((char *)&_frame[MKS_FRAME_DATA_OFFSET + 5], filename, fileNameLen);
  _frame[dataLen + 4] = MKS_FRAME_TAIL_FLAG;
  esp3d_log("Sending first fragment: Filename: %s, Filesize: %d", filename, filesize);
  _uploadStatus = UNKNOW_STATE;
  if (canSendFrame()) {
    if (esp3d_serial_service.writeBytes(_frame, dataLen + 5) == (dataLen + 5)) {
      esp3d_log("First fragment sent successfully");
      sendFrameDone();
      return true;
    }
    esp3d_log_e("Failed to send first fragment");
  } else {
    esp3d_log_e("Board not ready for first fragment");
  }
  sendFrameDone();
  return false;
}

bool MKSService::sendFragment(const uint8_t *dataFrame, const size_t dataSize,
                              uint fragmentID) {
  uint dataLen = dataSize + 4;
  esp3d_log("Sending fragment: datalen=%d, fragmentID=%d", dataSize, fragmentID);
  clearFrame();
  _frame[MKS_FRAME_HEAD_OFFSET] = MKS_FRAME_HEAD_FLAG;
  _frame[MKS_FRAME_TYPE_OFFSET] = MKS_FRAME_DATA_FRAGMENT_TYPE;
  _frame[MKS_FRAME_DATALEN_OFFSET] = dataLen & 0xff;
  _frame[MKS_FRAME_DATALEN_OFFSET + 1] = dataLen >> 8;
  _frame[MKS_FRAME_DATA_OFFSET] = fragmentID & 0xff;
  _frame[MKS_FRAME_DATA_OFFSET + 1] = (fragmentID >> 8) & 0xff;
  _frame[MKS_FRAME_DATA_OFFSET + 2] = (fragmentID >> 16) & 0xff;
  _frame[MKS_FRAME_DATA_OFFSET + 3] = (fragmentID >> 24) & 0xff;
  if ((dataSize > 0) && (dataFrame != nullptr)) {
    memcpy(&_frame[MKS_FRAME_DATA_OFFSET + 4], dataFrame, dataSize);
  }
  _frame[dataLen + 4] = MKS_FRAME_TAIL_FLAG;
  if (canSendFrame()) {
    _uploadStatus = UNKNOW_STATE;
    if (esp3d_serial_service.writeBytes(_frame, dataLen + 5) == (dataLen + 5)) {
      esp3d_log("Fragment sent successfully");
      sendFrameDone();
      return true;
    }
    esp3d_log_e("Failed to send fragment: incorrect size sent");
  } else {
    esp3d_log_e("Board not ready for fragment");
  }
  sendFrameDone();
  return false;
}

void MKSService::sendWifiHotspots() {
  uint8_t ssid_name_length;
  uint dataOffset = 1;
  uint8_t total_hotspots = 0;
  uint8_t currentmode = WiFi.getMode();
  esp3d_log("Starting WiFi scan for hotspots");
  if (currentmode == WIFI_AP) {
    esp3d_log("Switching to WIFI_AP_STA for scanning");
    if (!WiFi.mode(WIFI_AP_STA)) {
      esp3d_log_e("Failed to switch to WIFI_AP_STA");
      return;
    }
  }
  clearFrame();
  WiFi.scanDelete();
  int n = WiFi.scanNetworks();
  esp3d_log("Scan completed, found %d networks", n);
  if (n <= 0) {
    esp3d_log_e("No networks found or scan failed");
    _frame[MKS_FRAME_HEAD_OFFSET] = MKS_FRAME_HEAD_FLAG;
    _frame[MKS_FRAME_TYPE_OFFSET] = MKS_FRAME_DATA_HOTSPOTS_LIST_TYPE;
    _frame[MKS_FRAME_DATA_OFFSET] = 0; // No hotspots
    _frame[MKS_FRAME_DATALEN_OFFSET] = 1 & 0xff;
    _frame[MKS_FRAME_DATALEN_OFFSET + 1] = 1 >> 8;
    _frame[MKS_FRAME_DATA_OFFSET + 1] = MKS_FRAME_TAIL_FLAG;
    if (canSendFrame()) {
      if (esp3d_serial_service.writeBytes(_frame, 6) == 6) {
        esp3d_log("Sent empty hotspot list");
      } else {
        esp3d_log_e("Failed to send empty hotspot list");
      }
    } else {
      esp3d_log_e("Board not ready for hotspot list");
    }
    sendFrameDone();
    WiFi.mode((WiFiMode_t)currentmode);
    return;
  }
  _frame[MKS_FRAME_HEAD_OFFSET] = MKS_FRAME_HEAD_FLAG;
  _frame[MKS_FRAME_TYPE_OFFSET] = MKS_FRAME_DATA_HOTSPOTS_LIST_TYPE;
  String configuredSSID = ESP3DSettings::readString(ESP_STA_SSID);
  bool ssidFound = false;
  for (uint8_t i = 0; i < n && total_hotspots < NB_HOTSPOT_MAX; ++i) {
    int8_t signal_rssi = WiFi.RSSI(i);
    if (signal_rssi < MIN_RSSI) {
      esp3d_log("Ignoring %s: RSSI %d < MIN_RSSI %d", WiFi.SSID(i).c_str(), signal_rssi, MIN_RSSI);
      continue;
    }
    ssid_name_length = WiFi.SSID(i).length();
    if (ssid_name_length > MAX_SSID_LENGTH) {
      esp3d_log_e("Ignoring %s: SSID length %d > MAX_SSID_LENGTH %d", 
                  WiFi.SSID(i).c_str(), ssid_name_length, MAX_SSID_LENGTH);
      continue;
    }
    esp3d_log("Network %d: %s, RSSI: %d, %s", i + 1, WiFi.SSID(i).c_str(), signal_rssi,
              (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? "Open" : "Secure");
    _frame[MKS_FRAME_DATA_OFFSET + dataOffset] = ssid_name_length;
    for (uint8_t p = 0; p < ssid_name_length; p++) {
      _frame[MKS_FRAME_DATA_OFFSET + dataOffset + 1 + p] = WiFi.SSID(i)[p];
    }
    _frame[MKS_FRAME_DATA_OFFSET + dataOffset + ssid_name_length + 1] = WiFi.RSSI(i);
    dataOffset += ssid_name_length + 2;
    total_hotspots++;
    if (WiFi.SSID(i) == configuredSSID) {
      ssidFound = true;
    }
  }
  _frame[MKS_FRAME_DATA_OFFSET] = total_hotspots;
  _frame[MKS_FRAME_DATA_OFFSET + dataOffset] = MKS_FRAME_TAIL_FLAG;
  _frame[MKS_FRAME_DATALEN_OFFSET] = dataOffset & 0xff;
  _frame[MKS_FRAME_DATALEN_OFFSET + 1] = dataOffset >> 8;
  esp3d_log("Sending hotspot list with %d networks", total_hotspots);
  if (canSendFrame()) {
    if (esp3d_serial_service.writeBytes(_frame, dataOffset + 5) == (dataOffset + 5)) {
      esp3d_log("Hotspot list sent successfully");
    } else {
      esp3d_log_e("Failed to send hotspot list");
    }
  } else {
    esp3d_log_e("Board not ready for hotspot list");
  }
  sendFrameDone();
  WiFi.scanDelete();
  WiFi.mode((WiFiMode_t)currentmode);
  esp3d_log("Restored WiFi mode to %d", currentmode);
}

void MKSService::handleFrame(const uint8_t type, const uint8_t *dataFrame,
                             const size_t dataSize) {
  esp3d_log("Handling frame type: %d", type);
  switch (type) {
    case MKS_TYPE_NET:
      esp3d_log("Processing MKS_TYPE_NET");
      messageWiFiConfig(dataFrame, dataSize);
      break;
    case MKS_TYPE_PRINTER:
      esp3d_log("Ignoring MKS_TYPE_PRINTER (not supported)");
      break;
    case MKS_TYPE_TRANSFER:
      esp3d_log("Processing MKS_TYPE_TRANSFER (file transfer)");
      break;
    case MKS_TYPE_EXCEPTION:
      esp3d_log("Processing MKS_TYPE_EXCEPTION");
      messageException(dataFrame, dataSize);
      break;
    case MKS_TYPE_CLOUD:
      esp3d_log("Ignoring MKS_TYPE_CLOUD (not supported)");
      break;
    case MKS_TYPE_WID:
      esp3d_log("Ignoring MKS_TYPE_WID (not supported)");
      break;
    case MKS_TYPE_SCAN_WIFI:
      esp3d_log("Processing MKS_TYPE_SCAN_WIFI");
      sendWifiHotspots();
      break;
    case MKS_TYPE_MANUAL_IP:
      esp3d_log("Ignoring MKS_TYPE_MANUAL_IP (not supported)");
      break;
    case MKS_TYPE_WIFI_CTRL:
      esp3d_log("Processing MKS_TYPE_WIFI_CTRL");
      messageWiFiControl(dataFrame, dataSize);
      break;
    default:
      esp3d_log_e("Unknown frame type: %d", type);
  }
}

void MKSService::messageWiFiControl(const uint8_t *dataFrame, const size_t dataSize) {
  if (dataSize != 1) {
    esp3d_log_e("Invalid WiFi control frame size: %d", dataSize);
    return;
  }
  switch (dataFrame[0]) {
    case CONNECT_STA:
      esp3d_log("Processing CONNECT_STA");
      if (!NetConfig::started()) {
        uint8_t mode = ESP3DSettings::readByte(ESP_RADIO_MODE);
        if (mode == ESP_WIFI_APSTA) {
          esp3d_log("Starting AP+STA mode");
          if (!WiFi.mode(WIFI_AP_STA)) {
            esp3d_log_e("Failed to switch to WIFI_AP_STA");
            return;
          }
          String ap_ssid = ESP3DSettings::readString(ESP_AP_SSID);
          String ap_password = ESP3DSettings::readString(ESP_AP_PASSWORD);
          String sta_ssid = ESP3DSettings::readString(ESP_STA_SSID);
          String sta_password = ESP3DSettings::readString(ESP_STA_PASSWORD);
          WiFi.softAP(ap_ssid.c_str(), ap_password.c_str());
          esp3d_log("Starting STA connection to %s", sta_ssid.c_str());
          WiFi.begin(sta_ssid.c_str(), sta_password.c_str());
          uint32_t startTime = millis();
          while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < 10000) {
            ESP3DHal::wait(100);
          }
          esp3d_log("STA connection status: %s", WiFi.status() == WL_CONNECTED ? "Connected" : "Failed");
        } else {
          esp3d_log("Starting network in mode %d", mode);
          if (!NetConfig::begin()) {
            esp3d_log_e("Failed to start network in mode %d", mode);
          } else {
            esp3d_log("Network started, STA Connected: %s", WiFi.status() == WL_CONNECTED ? "yes" : "no");
          }
        }
      } else {
        esp3d_log("Network already started");
      }
      break;
    case DISCONNECT_STA:
      esp3d_log("Processing DISCONNECT_STA");
      if (NetConfig::started()) {
        esp3d_log("Disconnecting STA");
        WiFi.disconnect(true);
        NetConfig::end();
      }
      break;
    case REMOVE_STA_INFO:
      esp3d_log("Processing REMOVE_STA_INFO");
      if (NetConfig::started()) {
        esp3d_log("Disconnecting STA and AP");
        WiFi.disconnect(true);
        WiFi.softAPdisconnect(true);
        NetConfig::end();
      }
      ESP3DSettings::reset(true);
      esp3d_log("Settings reset");
      break;
    default:
      esp3d_log_e("Unsupported WiFi control flag: %d", dataFrame[0]);
  }
}

void MKSService::messageException(const uint8_t *dataFrame, const size_t dataSize) {
  if (dataSize != 1) {
    esp3d_log_e("Invalid exception frame size: %d", dataSize);
    return;
  }
  if (dataFrame[0] == ERROR_STATE || dataFrame[0] == SUCCESS_STATE) {
    _uploadStatus = dataFrame[0];
    esp3d_log("Transfer status: %s", dataFrame[0] == ERROR_STATE ? "Error" : "Success");
  } else {
    _uploadStatus = UNKNOW_STATE;
    esp3d_log_e("Unknown transfer state: %d", dataFrame[0]);
  }
}

void MKSService::messageWiFiConfig(const uint8_t *dataFrame, const size_t dataSize) {
  String ssid_sta, password_sta, ssid_ap, password_ap;
  bool needrestart = false;

  esp3d_log("Processing WiFi config frame, size=%d", dataSize);
  if (dataSize < 2) {
    esp3d_log_e("Invalid data size: %d", dataSize);
    return;
  }

  if (dataFrame[0] != MKS_FRAME_NETWORK_AP_MODE &&
      dataFrame[0] != MKS_FRAME_NETWORK_STA_MODE &&
      dataFrame[0] != MKS_FRAME_NETWORK_APSTA_MODE) {
    esp3d_log_e("Invalid mode: %d", dataFrame[0]);
    return;
  }

  uint8_t ssidSize = dataFrame[1];
  if (ssidSize == 0 || ssidSize > MAX_SSID_LENGTH || ssidSize > dataSize - 3) {
    esp3d_log_e("Invalid STA SSID size: %d", ssidSize);
    return;
  }

  uint8_t passwordSize = dataFrame[2 + ssidSize];
  if (passwordSize > MAX_PASSWORD_LENGTH || (uint)(ssidSize + passwordSize + 3) > dataSize) {
    esp3d_log_e("Invalid STA password size: %d", passwordSize);
    return;
  }

  for (uint8_t i = 0; i < ssidSize; i++) {
    ssid_sta += (char)dataFrame[2 + i];
  }
  for (uint8_t j = 0; j < passwordSize; j++) {
    password_sta += (char)dataFrame[3 + ssidSize + j];
  }
  esp3d_log("STA SSID: %s, Password length: %d", ssid_sta.c_str(), passwordSize);

  // Validate STA SSID against scan results
  bool ssidValid = false;
  if (WiFi.getMode() != WIFI_OFF) {
    esp3d_log("Validating STA SSID %s", ssid_sta.c_str());
    WiFi.scanDelete();
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; ++i) {
      if (WiFi.SSID(i) == ssid_sta && WiFi.RSSI(i) >= MIN_RSSI) {
        ssidValid = true;
        esp3d_log("STA SSID %s found with RSSI %d", ssid_sta.c_str(), WiFi.RSSI(i));
        break;
      }
    }
    WiFi.scanDelete();
    if (!ssidValid) {
      esp3d_log_e("STA SSID %s not found or signal too weak (< %d)", ssid_sta.c_str(), MIN_RSSI);
      return;
    }
  } else {
    esp3d_log("WiFi off, skipping SSID validation");
  }

  if (dataFrame[0] == MKS_FRAME_NETWORK_AP_MODE) {
    if (ESP3DSettings::readByte(ESP_RADIO_MODE) != ESP_WIFI_AP) {
      ESP3DSettings::writeByte(ESP_RADIO_MODE, ESP_WIFI_AP);
      needrestart = true;
      esp3d_log("Set ESP_RADIO_MODE to ESP_WIFI_AP");
    }
    String savedSsid = ESP3DSettings::readString(ESP_AP_SSID);
    String savedPassword = ESP3DSettings::readString(ESP_AP_PASSWORD);
    if (savedSsid != ssid_sta) {
      if (ESP3DSettings::writeString(ESP_AP_SSID, ssid_sta.c_str())) {
        esp3d_log("Set AP SSID to %s", ssid_sta.c_str());
        needrestart = true;
      } else {
        esp3d_log_e("Failed to set AP SSID");
      }
    }
    if (savedPassword != password_sta) {
      if (ESP3DSettings::writeString(ESP_AP_PASSWORD, password_sta.c_str())) {
        esp3d_log("Set AP Password");
        needrestart = true;
      } else {
        esp3d_log_e("Failed to set AP Password");
      }
    }
  } else if (dataFrame[0] == MKS_FRAME_NETWORK_STA_MODE) {
    if (ESP3DSettings::readByte(ESP_RADIO_MODE) != ESP_WIFI_STA) {
      ESP3DSettings::writeByte(ESP_RADIO_MODE, ESP_WIFI_STA);
      needrestart = true;
      esp3d_log("Set ESP_RADIO_MODE to ESP_WIFI_STA");
    }
    String savedSsid = ESP3DSettings::readString(ESP_STA_SSID);
    String savedPassword = ESP3DSettings::readString(ESP_STA_PASSWORD);
    if (savedSsid != ssid_sta) {
      if (ESP3DSettings::writeString(ESP_STA_SSID, ssid_sta.c_str())) {
        esp3d_log("Set STA SSID to %s", ssid_sta.c_str());
        needrestart = true;
      } else {
        esp3d_log_e("Failed to set STA SSID");
      }
    }
    if (savedPassword != password_sta) {
      if (ESP3DSettings::writeString(ESP_STA_PASSWORD, password_sta.c_str())) {
        esp3d_log("Set STA Password");
        needrestart = true;
      } else {
        esp3d_log_e("Failed to set STA Password");
      }
    }
    if (needrestart) {
      ESP3DSettings::writeByte(ESP_STA_IP_MODE, DHCP_MODE);
      esp3d_log("Set STA IP mode to DHCP");
    }
  } else if (dataFrame[0] == MKS_FRAME_NETWORK_APSTA_MODE) {
    uint8_t offset = 2 + ssidSize + passwordSize;
    if (offset + 1 >= dataSize) {
      esp3d_log_e("Missing AP SSID size");
      return;
    }
    uint8_t apSsidSize = dataFrame[offset];
    if (apSsidSize == 0 || apSsidSize > MAX_SSID_LENGTH || offset + 1 + apSsidSize >= dataSize) {
      esp3d_log_e("Invalid AP SSID size: %d", apSsidSize);
      return;
    }
    uint8_t apPasswordSize = dataFrame[offset + 1 + apSsidSize];
    if (apPasswordSize > MAX_PASSWORD_LENGTH || offset + 2 + apSsidSize + apPasswordSize > dataSize) {
      esp3d_log_e("Invalid AP password size: %d", apPasswordSize);
      return;
    }
    for (uint8_t i = 0; i < apSsidSize; i++) {
      ssid_ap += (char)dataFrame[offset + 1 + i];
    }
    for (uint8_t j = 0; j < apPasswordSize; j++) {
      password_ap += (char)dataFrame[offset + 2 + apSsidSize + j];
    }
    esp3d_log("AP SSID: %s, Password length: %d", ssid_ap.c_str(), apPasswordSize);
    if (ESP3DSettings::readByte(ESP_RADIO_MODE) != ESP_WIFI_APSTA) {
      ESP3DSettings::writeByte(ESP_RADIO_MODE, ESP_WIFI_APSTA);
      needrestart = true;
      esp3d_log("Set ESP_RADIO_MODE to ESP_WIFI_APSTA");
    }
    String savedSsid = ESP3DSettings::readString(ESP_STA_SSID);
    String savedPassword = ESP3DSettings::readString(ESP_STA_PASSWORD);
    if (savedSsid != ssid_sta) {
      if (ESP3DSettings::writeString(ESP_STA_SSID, ssid_sta.c_str())) {
        esp3d_log("Set STA SSID to %s", ssid_sta.c_str());
        needrestart = true;
      } else {
        esp3d_log_e("Failed to set STA SSID");
      }
    }
    if (savedPassword != password_sta) {
      if (ESP3DSettings::writeString(ESP_STA_PASSWORD, password_sta.c_str())) {
        esp3d_log("Set STA Password");
        needrestart = true;
      } else {
        esp3d_log_e("Failed to set STA Password");
      }
    }
    savedSsid = ESP3DSettings::readString(ESP_AP_SSID);
    savedPassword = ESP3DSettings::readString(ESP_AP_PASSWORD);
    if (savedSsid != ssid_ap) {
      if (ESP3DSettings::writeString(ESP_AP_SSID, ssid_ap.c_str())) {
        esp3d_log("Set AP SSID to %s", ssid_ap.c_str());
        needrestart = true;
      } else {
        esp3d_log_e("Failed to set AP SSID");
      }
    }
    if (savedPassword != password_ap) {
      if (ESP3DSettings::writeString(ESP_AP_PASSWORD, password_ap.c_str())) {
        esp3d_log("Set AP Password");
        needrestart = true;
      } else {
        esp3d_log_e("Failed to set AP Password");
      }
    }
    if (needrestart) {
      ESP3DSettings::writeByte(ESP_STA_IP_MODE, DHCP_MODE);
      esp3d_log("Set STA IP mode to DHCP");
    }
  }

  if (needrestart) {
    esp3d_log("Restarting network due to configuration changes");
    if (!NetConfig::begin()) {
      esp3d_log_e("Network setup failed, STA Connected: %s", 
                  WiFi.status() == WL_CONNECTED ? "yes" : "no");
    } else {
      esp3d_log("Network setup successful, STA Connected: %s", 
                WiFi.status() == WL_CONNECTED ? "yes" : "no");
    }
  }
}

bool MKSService::canSendFrame() {
  esp3d_log("Checking if board is ready for frame");
  digitalWrite(ESP_FLAG_PIN, BOARD_READY_FLAG_VALUE);
  uint32_t startTime = millis();
  while ((millis() - startTime) < FRAME_WAIT_TO_SEND_TIMEOUT) {
    if (digitalRead(BOARD_FLAG_PIN) == BOARD_READY_FLAG_VALUE) {
      esp3d_log("Board ready");
      return true;
    }
    ESP3DHal::wait(0);
  }
  esp3d_log_e("Board not ready after %d ms", FRAME_WAIT_TO_SEND_TIMEOUT);
  return false;
}

void MKSService::sendFrameDone() {
  digitalWrite(ESP_FLAG_PIN, !BOARD_READY_FLAG_VALUE);
  esp3d_log("Frame send completed");
}

bool MKSService::sendGcodeFrame(const char *cmd) {
  if (_uploadMode) {
    esp3d_log_e("Cannot send G-code in upload mode");
    return false;
  }
  String tmp = cmd;
  if (tmp.endsWith("\n")) {
    tmp[tmp.length() - 1] = '\0';
  }
  esp3d_log("Sending G-code: %s, size=%d", tmp.c_str(), strlen(tmp.c_str()));
  clearFrame();
  _frame[MKS_FRAME_HEAD_OFFSET] = MKS_FRAME_HEAD_FLAG;
  _frame[MKS_FRAME_TYPE_OFFSET] = MKS_FRAME_DATA_COMMAND_TYPE;
  for (uint i = 0; i < strlen(tmp.c_str()); i++) {
    _frame[MKS_FRAME_DATA_OFFSET + i] = tmp[i];
  }
  _frame[MKS_FRAME_DATA_OFFSET + strlen(tmp.c_str())] = '\r';
  _frame[MKS_FRAME_DATA_OFFSET + strlen(tmp.c_str()) + 1] = '\n';
  _frame[MKS_FRAME_DATA_OFFSET + strlen(tmp.c_str()) + 2] = MKS_FRAME_TAIL_FLAG;
  _frame[MKS_FRAME_DATALEN_OFFSET] = (strlen(tmp.c_str()) + 2) & 0xff;
  _frame[MKS_FRAME_DATALEN_OFFSET + 1] = ((strlen(tmp.c_str()) + 2) >> 8) & 0xff;
  if (canSendFrame()) {
    if (esp3d_serial_service.writeBytes(_frame, strlen(tmp.c_str()) + 7) == (strlen(tmp.c_str()) + 7)) {
      esp3d_log("G-code frame sent successfully");
      sendFrameDone();
      return true;
    }
    esp3d_log_e("Failed to send G-code frame: incorrect size sent");
  } else {
    esp3d_log_e("Board not ready for G-code frame");
  }
  sendFrameDone();
  return false;
}

bool MKSService::sendNetworkFrame() {
  size_t dataOffset = 0;
  String s;
  static uint32_t lastsend = 0;
  if (_uploadMode) {
    esp3d_log_e("Cannot send network frame in upload mode");
    return false;
  }
  if ((millis() - lastsend) < NET_FRAME_REFRESH_TIME) {
    return false;
  }
  lastsend = millis();
  esp3d_log("Preparing network frame");
  clearFrame();
  _frame[MKS_FRAME_HEAD_OFFSET] = MKS_FRAME_HEAD_FLAG;
  _frame[MKS_FRAME_TYPE_OFFSET] = MKS_FRAME_DATA_NETWORK_TYPE;
  uint8_t mode = NetConfig::getMode();
  bool staConnected = (WiFi.status() == WL_CONNECTED);
  if (mode == ESP_WIFI_STA) {
    esp3d_log("STA Mode, Connected: %s", staConnected ? "yes" : "no");
    IPAddress ip = staConnected ? NetConfig::localIPAddress() : IPAddress(0, 0, 0, 0);
    _frame[MKS_FRAME_DATA_OFFSET] = ip[0];
    _frame[MKS_FRAME_DATA_OFFSET + 1] = ip[1];
    _frame[MKS_FRAME_DATA_OFFSET + 2] = ip[2];
    _frame[MKS_FRAME_DATA_OFFSET + 3] = ip[3];
    esp3d_log("STA IP: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    _frame[MKS_FRAME_DATA_OFFSET + 6] = staConnected ? MKS_FRAME_NETWORK_OK_STATE : MKS_FRAME_NETWORK_FAIL_STATE;
    _frame[MKS_FRAME_DATA_OFFSET + 7] = MKS_FRAME_NETWORK_STA_MODE;
    s = ESP3DSettings::readString(ESP_STA_SSID);
    _frame[MKS_FRAME_DATA_OFFSET + 8] = s.length();
    dataOffset = MKS_FRAME_DATA_OFFSET + 9;
    strcpy((char *)&_frame[dataOffset], s.c_str());
    dataOffset += s.length();
    s = ESP3DSettings::readString(ESP_STA_PASSWORD);
    _frame[dataOffset] = s.length();
    dataOffset++;
    strcpy((char *)&_frame[dataOffset], s.c_str());
    dataOffset += s.length();
  } else if (mode == ESP_WIFI_AP || mode == ESP_AP_SETUP) {
    esp3d_log("AP Mode");
    IPAddress ip = NetConfig::localIPAddress();
    _frame[MKS_FRAME_DATA_OFFSET] = ip[0];
    _frame[MKS_FRAME_DATA_OFFSET + 1] = ip[1];
    _frame[MKS_FRAME_DATA_OFFSET + 2] = ip[2];
    _frame[MKS_FRAME_DATA_OFFSET + 3] = ip[3];
    esp3d_log("AP IP: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    _frame[MKS_FRAME_DATA_OFFSET + 6] = MKS_FRAME_NETWORK_OK_STATE;
    _frame[MKS_FRAME_DATA_OFFSET + 7] = MKS_FRAME_NETWORK_AP_MODE;
    s = ESP3DSettings::readString(ESP_AP_SSID);
    _frame[MKS_FRAME_DATA_OFFSET + 8] = s.length();
    dataOffset = MKS_FRAME_DATA_OFFSET + 9;
    strcpy((char *)&_frame[dataOffset], s.c_str());
    dataOffset += s.length();
    s = ESP3DSettings::readString(ESP_AP_PASSWORD);
    _frame[dataOffset] = s.length();
    dataOffset++;
    strcpy((char *)&_frame[dataOffset], s.c_str());
    dataOffset += s.length();
  } else if (mode == ESP_WIFI_APSTA) {
    esp3d_log("AP+STA Mode, STA Connected: %s", staConnected ? "yes" : "no");
    IPAddress ip = staConnected ? NetConfig::localIPAddress() : IPAddress(0, 0, 0, 0);
    _frame[MKS_FRAME_DATA_OFFSET] = ip[0];
    _frame[MKS_FRAME_DATA_OFFSET + 1] = ip[1];
    _frame[MKS_FRAME_DATA_OFFSET + 2] = ip[2];
    _frame[MKS_FRAME_DATA_OFFSET + 3] = ip[3];
    esp3d_log("STA IP: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    _frame[MKS_FRAME_DATA_OFFSET + 4] = (telnet_server.port()) & 0xff;
    _frame[MKS_FRAME_DATA_OFFSET + 5] = ((telnet_server.port()) >> 8) & 0xff;
    _frame[MKS_FRAME_DATA_OFFSET + 6] = staConnected ? MKS_FRAME_NETWORK_OK_STATE : MKS_FRAME_NETWORK_FAIL_STATE;
    _frame[MKS_FRAME_DATA_OFFSET + 7] = MKS_FRAME_NETWORK_APSTA_MODE;
    s = ESP3DSettings::readString(ESP_STA_SSID);
    _frame[MKS_FRAME_DATA_OFFSET + 8] = s.length();
    dataOffset = MKS_FRAME_DATA_OFFSET + 9;
    strcpy((char *)&_frame[dataOffset], s.c_str());
    dataOffset += s.length();
    s = ESP3DSettings::readString(ESP_STA_PASSWORD);
    _frame[dataOffset] = s.length();
    dataOffset++;
    strcpy((char *)&_frame[dataOffset], s.c_str());
    dataOffset += s.length();
    s = ESP3DSettings::readString(ESP_AP_SSID);
    _frame[dataOffset] = s.length();
    dataOffset++;
    strcpy((char *)&_frame[dataOffset], s.c_str());
    dataOffset += s.length();
    s = ESP3DSettings::readString(ESP_AP_PASSWORD);
    _frame[dataOffset] = s.length();
    dataOffset++;
    strcpy((char *)&_frame[dataOffset], s.c_str());
    dataOffset += s.length();
  } else {
    esp3d_log_e("Unsupported mode: %d", mode);
    return false;
  }
  _frame[MKS_FRAME_DATA_OFFSET + 4] = (telnet_server.port()) & 0xff;
  _frame[MKS_FRAME_DATA_OFFSET + 5] = ((telnet_server.port()) >> 8) & 0xff;
  esp3d_log("Telnet port: %d", telnet_server.port());
  _frame[dataOffset] = MKS_FRAME_CLOUD_DISABLED_STATE;
  dataOffset++;
  s = NetConfig::localIPAddress().toString();
  _frame[dataOffset] = s.length();
  dataOffset++;
  strcpy((char *)&_frame[dataOffset], s.c_str());
  dataOffset += s.length();
  _frame[dataOffset] = (HTTP_Server::port()) & 0xff;
  dataOffset++;
  _frame[dataOffset] = ((HTTP_Server::port()) >> 8) & 0xff;
  dataOffset++;
  esp3d_log("HTTP port: %d", HTTP_Server::port());
  _frame[dataOffset] = strlen(_moduleId);
  dataOffset++;
  strcpy((char *)&_frame[dataOffset], _moduleId);
  dataOffset += strlen(_moduleId);
  _frame[dataOffset] = strlen(FW_VERSION) + 6;
  dataOffset++;
  strcpy((char *)&_frame[dataOffset], "ESP3D_" FW_VERSION);
  dataOffset += strlen(FW_VERSION) + 6;
  _frame[dataOffset] = MKS_FRAME_TAIL_FLAG;
  _frame[MKS_FRAME_DATALEN_OFFSET] = (dataOffset - 4) & 0xff;
  _frame[MKS_FRAME_DATALEN_OFFSET + 1] = ((dataOffset - 4) >> 8) & 0xff;
  esp3d_log("Sending network frame, size=%d", dataOffset - 4);
  if (canSendFrame()) {
    if (esp3d_serial_service.writeBytes(_frame, dataOffset + 1) == (dataOffset + 1)) {
      esp3d_log("Network frame sent successfully");
      sendFrameDone();
      return true;
    }
    esp3d_log_e("Failed to send network frame: incorrect size sent");
  } else {
    esp3d_log_e("Board not ready for network frame");
  }
  sendFrameDone();
  return false;
}

void MKSService::clearFrame(uint start) {
  memset(&_frame[start], 0, sizeof(_frame) - start);
}

void MKSService::handle() {
  if (_started) {
    sendNetworkFrame();
  }
}

void MKSService::end() {
  esp3d_log("Stopping MKS service");
  _started = false;
}

#endif  // COMMUNICATION_PROTOCOL == MKS_SERIAL