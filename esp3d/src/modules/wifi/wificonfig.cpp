/*
  wificonfig.cpp - WiFi functions implementation

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
  License along with this code; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "../../include/esp3d_config.h"
#if defined(WIFI_FEATURE)
#ifdef ARDUINO_ARCH_ESP32
#include <esp_wifi.h>
#if ESP_ARDUINO_VERSION_MAJOR == 3
#include <esp_wifi_ap_get_sta_list.h>
#endif  // ESP_ARDUINO_VERSION_MAJOR == 3
#endif  // ARDUINO_ARCH_ESP32
#ifdef ARDUINO_ARCH_ESP8266
#include <ESP8266WiFi.h>
#endif  // ARDUINO_ARCH_ESP8266
#include "../../core/esp3d_commands.h"
#include "../../core/esp3d_settings.h"
#include "../network/netconfig.h"
#include "../wifi/wificonfig.h"

#if defined(ARDUINO_ARCH_ESP32)
esp_netif_t *get_esp_interface_netif(esp_interface_t interface);
#endif  // ARDUINO_ARCH_ESP32

const uint8_t DEFAULT_AP_MASK_VALUE[] = {255, 255, 255, 0};

IPAddress WiFiConfig::_ap_gateway;
IPAddress WiFiConfig::_ap_subnet;

const char* WiFiConfig::hostname() {
  static String tmp;
#if defined(ARDUINO_ARCH_ESP8266)
  if (WiFi.getMode() == WIFI_AP) {
    tmp = NetConfig::hostname(true); // Use NetConfig for AP hostname
  } else {
    tmp = WiFi.hostname();
  }
#elif defined(ARDUINO_ARCH_ESP32)
  if (WiFi.getMode() == WIFI_AP) {
    tmp = WiFi.softAPgetHostname();
  } else {
    tmp = WiFi.getHostname();
  }
#endif  // ARDUINO_ARCH_ESP8266
  return tmp.c_str();
}

int32_t WiFiConfig::getSignal(int32_t RSSI, bool filter) {
  if (RSSI < MIN_RSSI && filter) {
    return 0;
  }
  if (RSSI <= -100 && !filter) {
    return 0;
  }
  if (RSSI >= -50) {
    return 100;
  }
  return (2 * (RSSI + 100));
}

bool WiFiConfig::ConnectSTA2AP() {
  String msg, msg_out;
  uint8_t count = 0;
  uint8_t dot = 0;
  wl_status_t status = WiFi.status();
  esp3d_log("Connecting");
#if COMMUNICATION_PROTOCOL != MKS_SERIAL
  if (!ESP3DSettings::isVerboseBoot()) {
    const char* connect_msg = "Connecting";
    esp3d_commands.dispatch((uint8_t*)connect_msg, strlen(connect_msg), ESP3DClientType::all_clients,
                            no_id, ESP3DMessageType::unique, ESP3DClientType::system,
                            ESP3DAuthenticationLevel::admin);
  }
#endif  // COMMUNICATION_PROTOCOL != MKS_SERIAL
  while (status != WL_CONNECTED && count < 40) {
    switch (status) {
      case WL_NO_SSID_AVAIL:
        msg = "No SSID";
        break;
      case WL_CONNECT_FAILED:
        msg = "Connection failed";
        break;
      case WL_CONNECTED:
        break;
      default:
        if ((dot > 3) || (dot == 0)) {
          dot = 0;
          msg_out = "Connecting";
        }
        msg_out += ".";
        msg = msg_out;
        esp3d_log("...");
        dot++;
        break;
    }
    if (ESP3DSettings::isVerboseBoot()) {
      esp3d_commands.dispatch((uint8_t*)msg.c_str(), msg.length(), ESP3DClientType::all_clients,
                              no_id, ESP3DMessageType::unique, ESP3DClientType::system,
                              ESP3DAuthenticationLevel::admin);
    }
    ESP3DHal::wait(500);
    count++;
    status = WiFi.status();
  }
  return (status == WL_CONNECTED);
}

bool WiFiConfig::StartSTA() {
  esp3d_log("StartSTA");

  // Disconnect AP if active
  if ((WiFi.getMode() == WIFI_AP) || (WiFi.getMode() == WIFI_AP_STA)) {
    WiFi.softAPdisconnect(true);
  }

  // Enable STA mode only if not already active
  if (WiFi.getMode() != WIFI_STA) {
    WiFi.enableAP(false);
    WiFi.enableSTA(true);
    WiFi.mode(WIFI_STA);
  }

#if defined(ARDUINO_ARCH_ESP32)
  esp_wifi_start();
  WiFi.setMinSecurity(WIFI_AUTH_WEP);
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
#if defined(ESP32_WIFI_TX_POWER)
  WiFi.setTxPower(ESP32_WIFI_TX_POWER);
  delay(100);
#endif  // ESP32_WIFI_TX_POWER
#endif  // ARDUINO_ARCH_ESP32

  // Get parameters for STA
  String SSID = ESP3DSettings::readString(static_cast<int>(ESP3DSettingIndex::esp3d_sta_ssid));
  String password = ESP3DSettings::readString(static_cast<int>(ESP3DSettingIndex::esp3d_sta_password));

  if (SSID.isEmpty()) {
    esp3d_log_e("SSID is empty, cannot connect");
    return false;
  }

  // Apply static IP if configured
  if (ESP3DSettings::readByte(static_cast<int>(ESP3DSettingIndex::esp3d_sta_ip_mode)) != DHCP_MODE) {
    IPAddress ip(ESP3DSettings::read_IP(static_cast<int>(ESP3DSettingIndex::esp3d_sta_ip)));
    IPAddress gateway(ESP3DSettings::read_IP(static_cast<int>(ESP3DSettingIndex::esp3d_sta_gateway)));
    IPAddress mask(ESP3DSettings::read_IP(static_cast<int>(ESP3DSettingIndex::esp3d_sta_mask)));
    IPAddress dns(ESP3DSettings::read_IP(static_cast<int>(ESP3DSettingIndex::esp3d_sta_dns)));
    WiFi.config(ip, gateway, mask, dns);
  }

  // Verbose boot message
  if (ESP3DSettings::isVerboseBoot()) {
    String stmp = "Connecting to '" + SSID + "'";
    esp3d_commands.dispatch((uint8_t*)stmp.c_str(), stmp.length(), ESP3DClientType::all_clients,
                            no_id, ESP3DMessageType::unique, ESP3DClientType::system,
                            ESP3DAuthenticationLevel::admin);
  }

  // Begin connection
  if (WiFi.begin(SSID.c_str(), (password.length() > 0) ? password.c_str() : nullptr)) {
#if defined(ARDUINO_ARCH_ESP8266)
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    WiFi.hostname(NetConfig::hostname(true));

    WiFi.onStationModeConnected([](const WiFiEventStationModeConnected& evt) {
      esp3d_log("ESP8266 connected to SSID: %s", evt.ssid.c_str());
    });
    WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& evt) {
      esp3d_log("ESP8266 disconnected from SSID: %s", evt.ssid.c_str());
    });
    WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP& evt) {
      esp3d_log("ESP8266 got IP: %s", evt.ip.toString().c_str());
    });
#endif  // ARDUINO_ARCH_ESP8266

#if defined(ARDUINO_ARCH_ESP32)
    WiFi.setSleep(false);
    WiFi.setHostname(NetConfig::hostname(true));

    WiFi.onEvent([](WiFiEvent_t event) {
      switch (event) {
        case SYSTEM_EVENT_STA_CONNECTED:
          esp3d_log("ESP32 STA connected");
          break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
          esp3d_log("ESP32 STA disconnected");
          break;
        case SYSTEM_EVENT_STA_GOT_IP:
          esp3d_log("ESP32 STA got IP");
          break;
        default:
          break;
      }
    });
#endif  // ARDUINO_ARCH_ESP32

    // Wait for connection
    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
      delay(500);
    }

    if (WiFi.status() == WL_CONNECTED) {
      esp3d_log("Connected to %s, IP: %s", SSID.c_str(), WiFi.localIP().toString().c_str());
      return ConnectSTA2AP();
    } else {
      esp3d_log_e("Connection timeout");
      return false;
    }
  } else {
    esp3d_log_e("Starting client failed");
    return false;
  }
}

bool WiFiConfig::StartAP(bool setupMode) {
  esp3d_log("StartAP");

  // Disconnect STA if active
  if ((WiFi.getMode() == WIFI_STA) || (WiFi.getMode() == WIFI_AP_STA)) {
    if (WiFi.isConnected()) {
      WiFi.disconnect();
    }
  }

  // Disconnect AP if already active
  if ((WiFi.getMode() == WIFI_AP) || (WiFi.getMode() == WIFI_AP_STA)) {
    WiFi.softAPdisconnect(true);
  }

  // Switch to AP mode
  WiFi.enableAP(true);
  WiFi.enableSTA(false);
  WiFi.mode(WIFI_AP);

  // Sleep mode settings
#if defined(ARDUINO_ARCH_ESP8266)
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
#endif

#if defined(ESP32_WIFI_TX_POWER) && defined(ARDUINO_ARCH_ESP32)
  WiFi.setTxPower(ESP32_WIFI_TX_POWER);
  delay(100);
#endif

  // Read AP configuration
  String SSID = ESP3DSettings::readString(static_cast<int>(ESP3DSettingIndex::esp3d_ap_ssid));
  String password = ESP3DSettings::readString(static_cast<int>(ESP3DSettingIndex::esp3d_ap_password));
  int8_t channel = ESP3DSettings::readByte(static_cast<int>(ESP3DSettingIndex::esp3d_ap_channel));
  IPAddress ip(ESP3DSettings::read_IP(static_cast<int>(ESP3DSettingIndex::esp3d_ap_ip)));
  IPAddress gw(0, 0, 0, 0);
  IPAddress mask(DEFAULT_AP_MASK_VALUE[0], DEFAULT_AP_MASK_VALUE[1], DEFAULT_AP_MASK_VALUE[2], DEFAULT_AP_MASK_VALUE[3]);

  // Validate SSID
  if (SSID.isEmpty()) {
    esp3d_log_e("AP SSID is empty, cannot start");
    const char* msg = "AP SSID is empty";
    esp3d_commands.dispatch((uint8_t*)msg, strlen(msg), ESP3DClientType::all_clients,
                            no_id, ESP3DMessageType::unique, ESP3DClientType::system,
                            ESP3DAuthenticationLevel::admin);
    return false;
  }

#if defined(ARDUINO_ARCH_ESP8266)
  esp3d_log("ESP8266 AP IP config: IP=%s, GW=%s, MASK=%s",
            ip.toString().c_str(), ip.toString().c_str(), mask.toString().c_str());

  if (!WiFi.softAPConfig(ip, setupMode ? ip : gw, mask)) {
    esp3d_log_e("Set IP to AP failed");
    const char* msg = "Set IP to AP failed";
    esp3d_commands.dispatch((uint8_t*)msg, strlen(msg), ESP3DClientType::all_clients,
                            no_id, ESP3DMessageType::unique, ESP3DClientType::system,
                            ESP3DAuthenticationLevel::admin);
  } else {
    esp3d_commands.dispatch((uint8_t*)ip.toString().c_str(), ip.toString().length(), ESP3DClientType::all_clients,
                            no_id, ESP3DMessageType::unique, ESP3DClientType::system,
                            ESP3DAuthenticationLevel::admin);
  }
  _ap_gateway = setupMode ? ip : gw;
  _ap_subnet = mask;

  WiFi.onSoftAPModeStationConnected([](const WiFiEventSoftAPModeStationConnected& evt) {
    esp3d_log("ESP8266 AP client connected: MAC=%02X:%02X:%02X:%02X:%02X:%02X",
              evt.mac[0], evt.mac[1], evt.mac[2], evt.mac[3], evt.mac[4], evt.mac[5]);
  });
  WiFi.onSoftAPModeStationDisconnected([](const WiFiEventSoftAPModeStationDisconnected& evt) {
    esp3d_log("ESP8266 AP client disconnected: MAC=%02X:%02X:%02X:%02X:%02X:%02X",
              evt.mac[0], evt.mac[1], evt.mac[2], evt.mac[3], evt.mac[4], evt.mac[5]);
  });
#endif  // ARDUINO_ARCH_ESP8266

  // Start AP
  if (WiFi.softAP(SSID.c_str(),
                  (password.length() > 0) ? password.c_str() : nullptr,
                  channel)) {
    String stmp = "AP SSID: '" + SSID + "'";
    stmp += (password.length() > 0) ? "' is started and protected by password"
                                    : " is started not protected by password";
    if (setupMode) {
      stmp += " (setup mode)";
    }

    esp3d_commands.dispatch((uint8_t*)stmp.c_str(), stmp.length(), ESP3DClientType::all_clients,
                            no_id, ESP3DMessageType::unique, ESP3DClientType::system,
                            ESP3DAuthenticationLevel::admin);
    esp3d_log("%s", stmp.c_str());

#if defined(ARDUINO_ARCH_ESP32)
    ESP3DHal::wait(2000);

    esp3d_log("ESP32 AP IP config: IP=%s, GW=%s, MASK=%s",
              ip.toString().c_str(), ip.toString().c_str(), mask.toString().c_str());

    if (!WiFi.softAPConfig(ip, setupMode ? ip : gw, mask)) {
      esp3d_log_e("Set IP to AP failed");
      const char* msg = "Set IP to AP failed";
      esp3d_commands.dispatch((uint8_t*)msg, strlen(msg), ESP3DClientType::all_clients,
                              no_id, ESP3DMessageType::unique, ESP3DClientType::system,
                              ESP3DAuthenticationLevel::admin);
    } else {
      esp3d_commands.dispatch((uint8_t*)ip.toString().c_str(), ip.toString().length(), ESP3DClientType::all_clients,
                              no_id, ESP3DMessageType::unique, ESP3DClientType::system,
                              ESP3DAuthenticationLevel::admin);
    }

    WiFi.setSleep(false);
    WiFi.softAPsetHostname(NetConfig::hostname(true));
    _ap_gateway = setupMode ? ip : gw;
    _ap_subnet = mask;

    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
      if (event == ARDUINO_EVENT_WIFI_AP_STACONNECTED) {
        esp3d_log("ESP32 AP client connected: MAC=%02X:%02X:%02X:%02X:%02X:%02X",
                  info.sta_connected.mac[0], info.sta_connected.mac[1],
                  info.sta_connected.mac[2], info.sta_connected.mac[3],
                  info.sta_connected.mac[4], info.sta_connected.mac[5]);
      } else if (event == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) {
        esp3d_log("ESP32 AP client disconnected: MAC=%02X:%02X:%02X:%02X:%02X:%02X",
                  info.sta_disconnected.mac[0], info.sta_disconnected.mac[1],
                  info.sta_disconnected.mac[2], info.sta_disconnected.mac[3],
                  info.sta_disconnected.mac[4], info.sta_disconnected.mac[5]);
      }
    });
#endif  // ARDUINO_ARCH_ESP32

    return true;
  } else {
    const char* msg = "Starting AP failed";
    esp3d_commands.dispatch((uint8_t*)msg, strlen(msg), ESP3DClientType::all_clients,
                            no_id, ESP3DMessageType::unique, ESP3DClientType::system,
                            ESP3DAuthenticationLevel::admin);
    esp3d_log_e("Starting AP failed");
    return false;
  }
}

bool WiFiConfig::StartAPSTA() {
  esp3d_log("StartAPSTA");

  // Ensure both STA and AP are enabled
  WiFi.enableSTA(true);
  WiFi.enableAP(true);
  WiFi.mode(WIFI_AP_STA);

  // Read AP configuration
  String apSSID = ESP3DSettings::readString(static_cast<int>(ESP3DSettingIndex::esp3d_ap_ssid));
  String apPassword = ESP3DSettings::readString(static_cast<int>(ESP3DSettingIndex::esp3d_ap_password));
  int8_t channel = ESP3DSettings::readByte(static_cast<int>(ESP3DSettingIndex::esp3d_ap_channel));
  IPAddress apIP(ESP3DSettings::read_IP(static_cast<int>(ESP3DSettingIndex::esp3d_ap_ip)));
  IPAddress apGw(0, 0, 0, 0);
  IPAddress apMask(DEFAULT_AP_MASK_VALUE[0], DEFAULT_AP_MASK_VALUE[1], DEFAULT_AP_MASK_VALUE[2], DEFAULT_AP_MASK_VALUE[3]);

  // Read STA configuration
  String staSSID = ESP3DSettings::readString(static_cast<int>(ESP3DSettingIndex::esp3d_sta_ssid));
  String staPassword = ESP3DSettings::readString(static_cast<int>(ESP3DSettingIndex::esp3d_sta_password));

  // Validate SSIDs
  if (apSSID.isEmpty() || staSSID.isEmpty()) {
    esp3d_log_e("AP or STA SSID is empty, cannot start APSTA");
    const char* msg = "AP or STA SSID is empty";
    esp3d_commands.dispatch((uint8_t*)msg, strlen(msg), ESP3DClientType::all_clients,
                            no_id, ESP3DMessageType::unique, ESP3DClientType::system,
                            ESP3DAuthenticationLevel::admin);
    return false;
  }

  // Start AP
  if (!WiFi.softAP(apSSID.c_str(),
                   (apPassword.length() > 0) ? apPassword.c_str() : nullptr,
                   channel)) {
    esp3d_log_e("Starting AP in APSTA failed");
    const char* msg = "Starting AP in APSTA failed";
    esp3d_commands.dispatch((uint8_t*)msg, strlen(msg), ESP3DClientType::all_clients,
                            no_id, ESP3DMessageType::unique, ESP3DClientType::system,
                            ESP3DAuthenticationLevel::admin);
    return false;
  }

#if defined(ARDUINO_ARCH_ESP32)
  ESP3DHal::wait(2000);
  if (!WiFi.softAPConfig(apIP, apGw, apMask)) {
    esp3d_log_e("Set IP to AP in APSTA failed");
    const char* msg = "Set IP to AP in APSTA failed";
    esp3d_commands.dispatch((uint8_t*)msg, strlen(msg), ESP3DClientType::all_clients,
                            no_id, ESP3DMessageType::unique, ESP3DClientType::system,
                            ESP3DAuthenticationLevel::admin);
    return false;
  }
  WiFi.setSleep(false);
  WiFi.softAPsetHostname(NetConfig::hostname(true));
  _ap_gateway = apGw;
  _ap_subnet = apMask;
#endif

#if defined(ARDUINO_ARCH_ESP8266)
  if (!WiFi.softAPConfig(apIP, apGw, apMask)) {
    esp3d_log_e("Set IP to AP in APSTA failed");
    const char* msg = "Set IP to AP in APSTA failed";
    esp3d_commands.dispatch((uint8_t*)msg, strlen(msg), ESP3DClientType::all_clients,
                            no_id, ESP3DMessageType::unique, ESP3DClientType::system,
                            ESP3DAuthenticationLevel::admin);
    return false;
  }
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  _ap_gateway = apGw;
  _ap_subnet = apMask;
#endif

  // Start STA
  if (ESP3DSettings::readByte(static_cast<int>(ESP3DSettingIndex::esp3d_sta_ip_mode)) != DHCP_MODE) {
    IPAddress ip(ESP3DSettings::read_IP(static_cast<int>(ESP3DSettingIndex::esp3d_sta_ip)));
    IPAddress gateway(ESP3DSettings::read_IP(static_cast<int>(ESP3DSettingIndex::esp3d_sta_gateway)));
    IPAddress mask(ESP3DSettings::read_IP(static_cast<int>(ESP3DSettingIndex::esp3d_sta_mask)));
    IPAddress dns(ESP3DSettings::read_IP(static_cast<int>(ESP3DSettingIndex::esp3d_sta_dns)));
    WiFi.config(ip, gateway, mask, dns);
  }

  if (!WiFi.begin(staSSID.c_str(), (staPassword.length() > 0) ? staPassword.c_str() : nullptr)) {
    esp3d_log_e("Starting STA in APSTA failed");
    const char* msg = "Starting STA in APSTA failed";
    esp3d_commands.dispatch((uint8_t*)msg, strlen(msg), ESP3DClientType::all_clients,
                            no_id, ESP3DMessageType::unique, ESP3DClientType::system,
                            ESP3DAuthenticationLevel::admin);
    return false;
  }

  String stmp = "APSTA mode: AP SSID: '" + apSSID + "', STA SSID: '" + staSSID + "'";
  esp3d_commands.dispatch((uint8_t*)stmp.c_str(), stmp.length(), ESP3DClientType::all_clients,
                          no_id, ESP3DMessageType::unique, ESP3DClientType::system,
                          ESP3DAuthenticationLevel::admin);
  esp3d_log("%s", stmp.c_str());
  return ConnectSTA2AP();
}

void WiFiConfig::StopWiFi() {
  end();
}

bool WiFiConfig::started() { 
  return (WiFi.getMode() != WIFI_OFF); 
}

bool WiFiConfig::begin(int8_t& espMode) {
  bool res = false;

  // Clean up any previous WiFi state
  end();

  esp3d_log("Starting WiFi Config");

  if (ESP3DSettings::isVerboseBoot()) {
    const char* msg = "Starting WiFi";
    esp3d_commands.dispatch((uint8_t*)msg, strlen(msg), ESP3DClientType::all_clients,
                            no_id, ESP3DMessageType::unique, ESP3DClientType::system,
                            ESP3DAuthenticationLevel::admin);
  }

  int8_t wifiMode = espMode;

  // Handle AP, Setup, or APSTA mode
  if (wifiMode == wifi_ap) {
    esp3d_log("Requested mode: AP");
    res = StartAP(false);
  } else if (wifiMode == ap_setup) {
    esp3d_log("Requested mode: AP (setup)");
    res = StartAP(true);
  } else if (wifiMode == wifi_apsta) {
    esp3d_log("Requested mode: APSTA");
    res = StartAPSTA();
  } else if (wifiMode == wifi_sta) {
    esp3d_log("Requested mode: STA");
    res = StartSTA();

    // Fallback to AP if STA fails
    if (!res) {
      esp3d_log_e("STA connection failed, attempting fallback");

      if (ESP3DSettings::isVerboseBoot()) {
        const char* msg = "Starting fallback mode";
        esp3d_commands.dispatch((uint8_t*)msg, strlen(msg), ESP3DClientType::all_clients,
                                no_id, ESP3DMessageType::unique, ESP3DClientType::system,
                                ESP3DAuthenticationLevel::admin);
      }

      espMode = ESP3DSettings::readByte(static_cast<int>(ESP3DSettingIndex::esp3d_sta_fallback_mode));
      NetConfig::setMode(espMode);

      if (espMode == ap_setup) {
        esp3d_log("Fallback mode: AP (setup)");
        res = StartAP(true);
      } else if (espMode == wifi_ap) {
        esp3d_log("Fallback mode: AP");
        res = StartAP(false);
      } else if (espMode == wifi_apsta) {
        esp3d_log("Fallback mode: APSTA");
        res = StartAPSTA();
      } else {
        esp3d_log("Fallback mode: %d (handled externally)", espMode);
        res = true;
      }
    }
  } else {
    esp3d_log_e("Unknown WiFi mode: %d", wifiMode);
  }

#if defined(ARDUINO_ARCH_ESP8266)
  esp3d_log("ESP8266 WiFi mode after init: %d", WiFi.getMode());
#endif

#if defined(ARDUINO_ARCH_ESP32)
  esp3d_log("ESP32 WiFi mode after init: %d", WiFi.getMode());
#endif

  return res;
}

void WiFiConfig::end() {
  esp3d_log("Shutting down WiFi");

  if ((WiFi.getMode() == WIFI_STA) || (WiFi.getMode() == WIFI_AP_STA)) {
    if (WiFi.isConnected()) {
      esp3d_log("Disconnecting STA");
      WiFi.disconnect(true);
    }
  }

  if ((WiFi.getMode() == WIFI_AP) || (WiFi.getMode() == WIFI_AP_STA)) {
    esp3d_log("Disconnecting AP");
    WiFi.softAPdisconnect(true);
  }

#if defined(ARDUINO_ARCH_ESP8266)
  WiFi.onStationModeDisconnected(nullptr);
  WiFi.onStationModeConnected(nullptr);
  WiFi.onStationModeGotIP(nullptr);
  WiFi.onSoftAPModeStationConnected(nullptr);
  WiFi.onSoftAPModeStationDisconnected(nullptr);
  esp3d_log("ESP8266 WiFi events cleared");
#endif

#if defined(ARDUINO_ARCH_ESP32)
  WiFi.removeEvent();
  esp3d_log("ESP32 WiFi events cleared");
#endif

  WiFi.mode(WIFI_OFF);
  esp3d_log("WiFi mode set to OFF");
}

void WiFiConfig::handle() {
  if (WiFi.getMode() == WIFI_AP_STA) {
    if (WiFi.scanComplete() != WIFI_SCAN_RUNNING) {
      esp3d_log("Disabling STA to avoid mixed mode conflict");
      WiFi.enableSTA(false);
    }
  }
}

const char* WiFiConfig::getSleepModeString() {
#if defined(ARDUINO_ARCH_ESP32)
  return WiFi.getSleep() ? "modem" : "none";
#elif defined(ARDUINO_ARCH_ESP8266)
  WiFiSleepType_t ps_type = WiFi.getSleepMode();
  switch (ps_type) {
    case WIFI_NONE_SLEEP: return "none";
    case WIFI_LIGHT_SLEEP: return "light";
    case WIFI_MODEM_SLEEP: return "modem";
    default: return "unknown";
  }
#endif
}

const char* WiFiConfig::getPHYModeString(uint8_t wifimode) {
#if defined(ARDUINO_ARCH_ESP32)
  uint8_t PhyMode = 0;
  esp_wifi_get_protocol(
    (wifi_interface_t)((wifimode == WIFI_STA) ? ESP_IF_WIFI_STA : ESP_IF_WIFI_AP),
    &PhyMode);
#elif defined(ARDUINO_ARCH_ESP8266)
  WiFiPhyMode_t PhyMode = WiFi.getPhyMode();
#endif

  switch (PhyMode) {
    case WIFI_PHY_MODE_11B: return "11b";
    case WIFI_PHY_MODE_11G: return "11g";
    case WIFI_PHY_MODE_11N: return "11n";
    default: return "unknown";
  }
}

bool WiFiConfig::is_AP_visible() {
#ifdef ARDUINO_ARCH_ESP32
  wifi_config_t conf;
  esp_wifi_get_config((wifi_interface_t)ESP_IF_WIFI_AP, &conf);
  return (conf.ap.ssid_hidden == 0);
#endif  // ARDUINO_ARCH_ESP32
#ifdef ARDUINO_ARCH_ESP8266
  struct softap_config apconfig;
  wifi_softap_get_config(&apconfig);
  return (apconfig.ssid_hidden == 0);
#endif  // ARDUINO_ARCH_ESP8266
}

const char* WiFiConfig::AP_SSID() {
  static String ssid;
#if defined(ARDUINO_ARCH_ESP32)
  wifi_config_t conf;
  esp_wifi_get_config(ESP_IF_WIFI_AP, &conf);
  ssid = reinterpret_cast<const char*>(conf.ap.ssid);
#elif defined(ARDUINO_ARCH_ESP8266)
  struct softap_config apconfig;
  wifi_softap_get_config(&apconfig);
  ssid = reinterpret_cast<const char*>(apconfig.ssid);
#endif
  return ssid.c_str();
}

const char* WiFiConfig::AP_Auth_String() {
  uint8_t mode = 0;
#if defined(ARDUINO_ARCH_ESP32)
  wifi_config_t conf;
  esp_wifi_get_config(ESP_IF_WIFI_AP, &conf);
  mode = conf.ap.authmode;
#elif defined(ARDUINO_ARCH_ESP8266)
  struct softap_config apconfig;
  wifi_softap_get_config(&apconfig);
  mode = apconfig.authmode;
#endif

  switch (mode) {
    case AUTH_OPEN: return "none";
    case AUTH_WEP: return "WEP";
    case AUTH_WPA_PSK: return "WPA";
    case AUTH_WPA2_PSK: return "WPA2";
    default: return "WPA/WPA2";
  }
}

const char* WiFiConfig::AP_Gateway_String() {
  static String tmp;
#if defined(ARDUINO_ARCH_ESP32)
  esp_netif_ip_info_t ip_AP;
  esp_netif_get_ip_info(get_esp_interface_netif(ESP_IF_WIFI_AP), &ip_AP);
  tmp = IPAddress(ip_AP.gw.addr).toString();
#elif defined(ARDUINO_ARCH_ESP8266)
  struct ip_info ip_AP;
  if (!wifi_get_ip_info(SOFTAP_IF, &ip_AP)) {
    esp3d_log_e("Error getting gateway IP");
  }
  tmp = IPAddress(ip_AP.gw).toString();
#endif
  return tmp.c_str();
}

const char* WiFiConfig::AP_Mask_String() {
  static String tmp;
#if defined(ARDUINO_ARCH_ESP32)
  esp_netif_ip_info_t ip_AP;
  esp_netif_get_ip_info(get_esp_interface_netif(ESP_IF_WIFI_AP), &ip_AP);
  tmp = IPAddress(ip_AP.netmask.addr).toString();
#elif defined(ARDUINO_ARCH_ESP8266)
  struct ip_info ip_AP;
  if (!wifi_get_ip_info(SOFTAP_IF, &ip_AP)) {
    esp3d_log_e("Error getting subnet mask");
  }
  tmp = IPAddress(ip_AP.netmask).toString();
#endif
  return tmp.c_str();
}

const char* WiFiConfig::getConnectedSTA(uint8_t* totalcount, bool reset) {
  static uint8_t count = 0;
  static uint8_t current = 0;
  static String data;
  data = "";

#if defined(ARDUINO_ARCH_ESP32)
  static wifi_sta_list_t sta_list;
#if ESP_ARDUINO_VERSION_MAJOR == 3
  static wifi_sta_mac_ip_list_t tcpip_sta_list;
#elif ESP_ARDUINO_VERSION_MAJOR == 2
  static tcpip_adapter_sta_list_t tcpip_sta_list;
#endif

  if (reset) count = 0;
  if (current > count) current = 0;

  if (count == 0) {
    current = 0;
#if ESP_ARDUINO_VERSION_MAJOR == 3
    if (esp_wifi_ap_get_sta_list(&sta_list) != ESP_OK) return "";
    ESP_ERROR_CHECK(esp_wifi_ap_get_sta_list_with_ip(&sta_list, &tcpip_sta_list));
#elif ESP_ARDUINO_VERSION_MAJOR == 2
    if (tcpip_adapter_get_sta_list(&sta_list, &tcpip_sta_list) != ESP_OK) return "";
#endif
    count = sta_list.num;
  }

  if (count > 0 && current < count) {
    data = IPAddress(tcpip_sta_list.sta[current].ip.addr).toString();
    data += String("(") + NetConfig::mac2str(tcpip_sta_list.sta[current].mac) + String(")");
    current++;
  }

#elif defined(ARDUINO_ARCH_ESP8266)
  static struct station_info* station;
  if (reset || current > count) {
    current = 0;
    count = 0;
    station = wifi_softap_get_station_info();
    struct station_info* tmp = station;
    while (tmp) {
      count++;
      tmp = STAILQ_NEXT(tmp, next);
    }
  }

  if (count > 0 && station) {
    data = IPAddress((const uint8_t*)&station->ip).toString();
    data += String("(") + String(NetConfig::mac2str(station->bssid)) + String(")");
    current++;
    station = STAILQ_NEXT(station, next);
    if (current == count || !station) {
      wifi_softap_free_station_info();
    }
  }
#endif

  if (totalcount) *totalcount = count;
  return data.c_str();
}

#endif  // WIFI_FEATURE