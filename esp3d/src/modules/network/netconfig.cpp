/*
  netconfig.cpp - network functions class

  Copyright (c) 2018 Luc Lebosse. All rights reserved.

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
#if defined(WIFI_FEATURE) || defined(ETH_FEATURE) || defined(BLUETOOTH_FEATURE)
#ifdef ARDUINO_ARCH_ESP32
#define WIFI_EVENT_STAMODE_CONNECTED ARDUINO_EVENT_WIFI_STA_CONNECTED
#define WIFI_EVENT_STAMODE_DISCONNECTED ARDUINO_EVENT_WIFI_STA_DISCONNECTED
#define WIFI_EVENT_STAMODE_GOT_IP ARDUINO_EVENT_WIFI_STA_GOT_IP
#define WIFI_EVENT_SOFTAPMODE_STACONNECTED ARDUINO_EVENT_WIFI_AP_STACONNECTED
#define RADIO_OFF_MSG "Radio Off"
#endif  // ARDUINO_ARCH_ESP32
#ifdef ARDUINO_ARCH_ESP8266
#define RADIO_OFF_MSG "WiFi Off"
#endif  // ARDUINO_ARCH_ESP8266
#include "netconfig.h"
#if defined(WIFI_FEATURE)
#include "../wifi/wificonfig.h"
#endif  // WIFI_FEATURE
#if defined(ETH_FEATURE)
#include "../ethernet/ethconfig.h"
#endif  // ETH_FEATURE
#if defined(BLUETOOTH_FEATURE)
#include "../bluetooth/BT_service.h"
#endif  // BLUETOOTH_FEATURE
#include "../../core/esp3d_commands.h"
#include "../../core/esp3d_settings.h"
#include "../../core/esp3d_string.h"
#include "netservices.h"
#if defined(GCODE_HOST_FEATURE)
#include "../gcode_host/gcode_host.h"
#endif  // GCODE_HOST_FEATURE

#if defined(ARDUINO_ARCH_ESP32)
esp_netif_t *get_esp_interface_netif(esp_interface_t interface);
#endif  // ARDUINO_ARCH_ESP32

String NetConfig::_hostname = "";
bool NetConfig::_needReconnect2AP = false;
bool NetConfig::_events_registered = false;
bool NetConfig::_started = false;
uint8_t NetConfig::_mode = ESP_NO_NETWORK;

/**
 * Convert MAC address to string
 */
char* NetConfig::mac2str(uint8_t mac[8]) {
  static char macstr[18];
  if (sprintf(macstr, "%02X:%02X:%02X:%02X:%02X:%02X",
              mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]) < 0) {
    strcpy(macstr, "00:00:00:00:00:00");
  }
  return macstr;
}

/**
 * Convert IP string to uint32_t
 */
uint32_t NetConfig::IP_int_from_string(const char* s) {
  IPAddress ipaddr;
  return ipaddr.fromString(s) ? (uint32_t)ipaddr : 0;
}

/**
 * Convert uint32_t to IP string
 */
String NetConfig::IP_string_from_int(uint32_t ip_int) {
  return IPAddress(ip_int).toString();
}

/**
 * Get current IP address (STA, AP, or ETH)
 */
IPAddress NetConfig::localIPAddress() {
  IPAddress current_ip(0, 0, 0, 0);

#if defined(WIFI_FEATURE)
  if (WiFi.getMode() == WIFI_STA || WiFi.getMode() == WIFI_AP_STA) {
    current_ip = WiFi.localIP();
  } else if (WiFi.getMode() == WIFI_AP) {
    current_ip = WiFi.softAPIP();
  }
#endif

#if defined(ETH_FEATURE)
  if (EthConfig::started()) {
    current_ip = ETH.localIP();
  }
#endif

  return current_ip;
}

/**
 * Get AP IP address
 */
IPAddress NetConfig::softAPIPAddress() {
#if defined(WIFI_FEATURE)
  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
    return WiFi.softAPIP();
  }
#endif
  return IPAddress(0, 0, 0, 0);
}

/**
 * Get current IP as string
 */
String NetConfig::localIP() {
  IPAddress ip = localIPAddress();
  return ip.toString();
}

/**
 * Get AP IP as string
 */
String NetConfig::softAPIP() {
#if defined(WIFI_FEATURE)
  return WiFi.softAPIP().toString();
#else
  return "0.0.0.0";
#endif
}

/**
 * Get current Gateway IP as string
 */
String NetConfig::localGW() {
#if defined(WIFI_FEATURE)
  if (WiFi.getMode() == WIFI_STA || WiFi.getMode() == WIFI_AP_STA) {
    return WiFi.gatewayIP().toString();
  } else if (WiFi.getMode() == WIFI_AP) {
    return WiFiConfig::getAPGateway().toString();
  }
#endif

#if defined(ETH_FEATURE)
  if (EthConfig::started()) {
    return ETH.gatewayIP().toString();
  }
#endif

  return "0.0.0.0";
}

/**
 * Get current subnet mask as string
 */
String NetConfig::localMSK() {
#if defined(WIFI_FEATURE)
  if (WiFi.getMode() == WIFI_STA || WiFi.getMode() == WIFI_AP_STA) {
    return WiFi.subnetMask().toString();
  } else if (WiFi.getMode() == WIFI_AP) {
    return WiFiConfig::getAPSubnet().toString();
  }
#endif

#if defined(ETH_FEATURE)
  if (EthConfig::started()) {
    return ETH.subnetMask().toString();
  }
#endif

  return "0.0.0.0";
}

/**
 * Get current DNS IP as string
 */
String NetConfig::localDNS() {
#if defined(WIFI_FEATURE)
  if (WiFi.getMode() == WIFI_STA || WiFi.getMode() == WIFI_AP_STA) {
    return WiFi.dnsIP().toString();
  } else if (WiFi.getMode() == WIFI_AP) {
    return WiFi.softAPIP().toString();  // fallback
  }
#endif

#if defined(ETH_FEATURE)
  if (EthConfig::started()) {
    return ETH.dnsIP().toString();
  }
#endif

  return "0.0.0.0";
}

// WiFi event handler
void NetConfig::onWiFiEvent(WiFiEvent_t event) {
  // Filter out noisy events before system is marked as started
  if (!_started && event != WIFI_EVENT_STAMODE_CONNECTED && event != WIFI_EVENT_SOFTAPMODE_STACONNECTED) {
    return;
  }

  switch (event) {
    case WIFI_EVENT_STAMODE_CONNECTED:
      _needReconnect2AP = false;
      esp3d_log("WiFi STA connected to SSID");
      break;

    case WIFI_EVENT_STAMODE_DISCONNECTED:
      if (_started) {
        esp3d_log("WiFi STA disconnected");
        {
          const char* msg = "STA disconnected";
          esp3d_commands.dispatch((uint8_t*)msg, strlen(msg), ESP3DClientType::all_clients,
                                  no_id, ESP3DMessageType::unique, ESP3DClientType::system,
                                  ESP3DAuthenticationLevel::admin);
        }
        _needReconnect2AP = true;
      }
      break;

    case WIFI_EVENT_STAMODE_GOT_IP:
      esp3d_log("WiFi STA got IP: %s", WiFi.localIP().toString().c_str());
#if COMMUNICATION_PROTOCOL != MKS_SERIAL
#if defined(ESP_GOT_IP_HOOK) && defined(GCODE_HOST_FEATURE)
      {
        String ipMsg = esp3d_string::expandString(ESP_GOT_IP_HOOK);
        esp3d_log("Got IP, sending hook: %s", ipMsg.c_str());
        esp3d_gcode_host.processScript(ipMsg.c_str(),
                                       ESP3DAuthenticationLevel::admin);
      }
#endif
#endif
      {
        String ipMsg = String("STA got IP: ") + WiFi.localIP().toString();
        esp3d_commands.dispatch((uint8_t*)ipMsg.c_str(), ipMsg.length(), ESP3DClientType::all_clients,
                                no_id, ESP3DMessageType::unique, ESP3DClientType::system,
                                ESP3DAuthenticationLevel::admin);
      }
      break;

    case WIFI_EVENT_SOFTAPMODE_STACONNECTED:
      esp3d_log("Client connected to SoftAP");
      {
        const char* msg = "New AP client";
        esp3d_commands.dispatch((uint8_t*)msg, strlen(msg), ESP3DClientType::all_clients,
                                no_id, ESP3DMessageType::unique, ESP3DClientType::system,
                                ESP3DAuthenticationLevel::admin);
      }
      break;

    case WIFI_EVENT_SOFTAPMODE_STADISCONNECTED:
      esp3d_log("Client disconnected from SoftAP");
      {
        const char* msg = "AP client disconnected";
        esp3d_commands.dispatch((uint8_t*)msg, strlen(msg), ESP3DClientType::all_clients,
                                no_id, ESP3DMessageType::unique, ESP3DClientType::system,
                                ESP3DAuthenticationLevel::admin);
      }
      break;

#ifdef ARDUINO_ARCH_ESP32
    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
      if (_started) {
        esp3d_log("WiFi STA lost IP");
        _needReconnect2AP = true;
        {
          const char* msg = "STA lost IP";
          esp3d_commands.dispatch((uint8_t*)msg, strlen(msg), ESP3DClientType::all_clients,
                                  no_id, ESP3DMessageType::unique, ESP3DClientType::system,
                                  ESP3DAuthenticationLevel::admin);
        }
      }
      break;

#ifdef ETH_FEATURE
    case ARDUINO_EVENT_ETH_START:
      esp3d_log("Ethernet started");
      if (ESP3DSettings::isVerboseBoot()) {
        const char* msg = "Checking connection";
        esp3d_commands.dispatch((uint8_t*)msg, strlen(msg), ESP3DClientType::all_clients,
                                no_id, ESP3DMessageType::unique, ESP3DClientType::system,
                                ESP3DAuthenticationLevel::admin);
      }
      break;

    case ARDUINO_EVENT_ETH_CONNECTED:
      esp3d_log("Ethernet connected");
      {
        const char* msg = "Cable connected";
        esp3d_commands.dispatch((uint8_t*)msg, strlen(msg), ESP3DClientType::all_clients,
                                no_id, ESP3DMessageType::unique, ESP3DClientType::system,
                                ESP3DAuthenticationLevel::admin);
      }
      EthConfig::setConnected(true);
      break;

    case ARDUINO_EVENT_ETH_DISCONNECTED:
      esp3d_log("Ethernet disconnected");
      {
        const char* msg = "Cable disconnected";
        esp3d_commands.dispatch((uint8_t*)msg, strlen(msg), ESP3DClientType::all_clients,
                                no_id, ESP3DMessageType::unique, ESP3DClientType::system,
                                ESP3DAuthenticationLevel::admin);
      }
      EthConfig::setConnected(false);
      break;

    case ARDUINO_EVENT_ETH_LOST_IP:
      esp3d_log("Ethernet lost IP");
      break;

    case ARDUINO_EVENT_ETH_GOT_IP:
#if COMMUNICATION_PROTOCOL != MKS_SERIAL
#if defined(ESP_GOT_IP_HOOK) && defined(GCODE_HOST_FEATURE)
      {
        ESP3DHal::wait(500);
        String ipMsg = esp3d_string::expandString(ESP_GOT_IP_HOOK);
        esp3d_log("Got IP, sending hook: %s", ipMsg.c_str());
        esp3d_gcode_host.processScript(ipMsg.c_str(),
                                       ESP3DAuthenticationLevel::admin);
      }
#endif
#endif
      esp3d_log("Ethernet got IP: %s", ETH.localIP().toString().c_str());
      EthConfig::setConnected(true);
      break;

    case ARDUINO_EVENT_ETH_STOP:
      esp3d_log("Ethernet stopped");
      EthConfig::setConnected(false);
      break;
#endif  // ETH_FEATURE
#endif  // ARDUINO_ARCH_ESP32

    default:
      esp3d_log("Unhandled WiFi event: %d", event);
      break;
  }
}

void NetConfig::setMode(uint8_t mode) { _mode = mode; }

uint8_t NetConfig::getMode() { return _mode; }

/**
 * Begin WiFi setup
 */
bool NetConfig::begin() {
  bool res = false;
  // Clear everything
  end();
  int8_t espMode = ESP3DSettings::readByte(ESP_RADIO_MODE);
  esp3d_log("Starting Network in mode %d", espMode);
  if (espMode != ESP_NO_NETWORK) {
    if (ESP3DSettings::isVerboseBoot()) {
      const char* msg = "Starting Network";
      esp3d_commands.dispatch((uint8_t*)msg, strlen(msg), ESP3DClientType::all_clients,
                              no_id, ESP3DMessageType::unique, ESP3DClientType::system,
                              ESP3DAuthenticationLevel::admin);
    }
  }
  // Setup events
  if (!_events_registered) {
#ifdef ARDUINO_ARCH_ESP8266
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    WiFi.onEvent(NetConfig::onWiFiEvent, WIFI_EVENT_ANY);
#pragma GCC diagnostic pop
#endif
#ifdef ARDUINO_ARCH_ESP32
    WiFi.onEvent(NetConfig::onWiFiEvent);
#endif
    _events_registered = true;
    esp3d_log("WiFi events registered");
  }
  // Get hostname
  _hostname = ESP3DSettings::readString(ESP_MODEL_NAME);
  esp3d_log("Hostname: %s", _hostname.c_str());
  _mode = espMode;
  if (espMode == ESP_NO_NETWORK) {
    esp3d_log("Disabling network");
    {
      const char* msg = "Disable Network";
      esp3d_commands.dispatch((uint8_t*)msg, strlen(msg), ESP3DClientType::all_clients,
                              no_id, ESP3DMessageType::unique, ESP3DClientType::system,
                              ESP3DAuthenticationLevel::admin);
    }
    WiFi.mode(WIFI_OFF);
#if defined(DISPLAY_DEVICE)
    ESP3DRequest reqId = {
        .id = ESP_OUTPUT_IP_ADDRESS,
    };
    esp3d_commands.dispatch((uint8_t*)" ", 1, ESP3DClientType::rendering, reqId,
                            ESP3DMessageType::unique, ESP3DClientType::system,
                            ESP3DAuthenticationLevel::admin);
#endif  // DISPLAY_DEVICE
    if (ESP3DSettings::isVerboseBoot()) {
      esp3d_commands.dispatch((uint8_t*)RADIO_OFF_MSG, strlen(RADIO_OFF_MSG), ESP3DClientType::all_clients,
                              no_id, ESP3DMessageType::unique, ESP3DClientType::system,
                              ESP3DAuthenticationLevel::admin);
    }
    return true;
  }
#if defined(WIFI_FEATURE)
  if ((espMode == ap_setup) || (espMode == wifi_ap) ||
      (espMode == wifi_sta) || (espMode == wifi_apsta)) {
    esp3d_log("Setting up WiFi in mode %d", espMode);
    {
      const char* msg = "Setup WiFi";
      esp3d_commands.dispatch((uint8_t*)msg, strlen(msg), ESP3DClientType::all_clients,
                              no_id, ESP3DMessageType::unique, ESP3DClientType::system,
                              ESP3DAuthenticationLevel::admin);
    }
    res = WiFiConfig::begin(espMode);
    if (res && espMode == wifi_apsta) {
      // Verify both AP and STA are active
      if (WiFi.getMode() == WIFI_AP_STA) {
        esp3d_log("APSTA mode active, AP IP: %s, STA IP: %s",
                  WiFi.softAPIP().toString().c_str(),
                  WiFi.localIP().toString().c_str());
        if (WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
          esp3d_log_e("STA IP not assigned, retrying");
          _needReconnect2AP = true;
          res = false;
        }
      } else {
        esp3d_log_e("Failed to set APSTA mode, current mode: %d", WiFi.getMode());
        res = false;
      }
    }
  }
#endif  // WIFI_FEATURE
#if defined(ETH_FEATURE)
  if (espMode == eth_sta) {
    WiFi.mode(WIFI_OFF);
    esp3d_log("Setting up Ethernet");
    res = EthConfig::begin(espMode);
  }
#else
  if (espMode == eth_sta) {
    espMode = ESP_NO_NETWORK;
    esp3d_log_e("Ethernet mode requested but ETH_FEATURE not enabled");
  }
#endif  // ETH_FEATURE

#if defined(BLUETOOTH_FEATURE)
  if (espMode == ESP_BT) {
    WiFi.mode(WIFI_OFF);
    String msg = "BT On";
    esp3d_log("Starting Bluetooth");
#if defined(DISPLAY_DEVICE)
    ESP3DRequest reqId = {
        .id = ESP_OUTPUT_STATUS,
    };
    esp3d_commands.dispatch((uint8_t*)msg.c_str(), msg.length(), ESP3DClientType::rendering, reqId,
                            ESP3DMessageType::unique, ESP3DClientType::system,
                            ESP3DAuthenticationLevel::admin);
#endif  // DISPLAY_DEVICE
    res = bt_service.begin();
  }
#else
  if (espMode == ESP_BT) {
    espMode = ESP_NO_NETWORK;
    esp3d_log_e("Bluetooth mode requested but BLUETOOTH_FEATURE not enabled");
  }
#endif  // BLUETOOTH_FEATURE

  if (espMode == ESP_NO_NETWORK) {
    esp3d_log("No network mode, disabling network");
    {
      const char* msg = "Disable Network";
      esp3d_commands.dispatch((uint8_t*)msg, strlen(msg), ESP3DClientType::all_clients,
                              no_id, ESP3DMessageType::unique, ESP3DClientType::system,
                              ESP3DAuthenticationLevel::admin);
    }
    WiFi.mode(WIFI_OFF);
#if defined(DISPLAY_DEVICE)
    ESP3DRequest reqId = {
        .id = ESP_OUTPUT_IP_ADDRESS,
    };
    esp3d_commands.dispatch((uint8_t*)" ", 1, ESP3DClientType::rendering, reqId,
                            ESP3DMessageType::unique, ESP3DClientType::system,
                            ESP3DAuthenticationLevel::admin);
#endif  // DISPLAY_DEVICE
    if (ESP3DSettings::isVerboseBoot()) {
      esp3d_commands.dispatch((uint8_t*)RADIO_OFF_MSG, strlen(RADIO_OFF_MSG), ESP3DClientType::all_clients,
                              no_id, ESP3DMessageType::unique, ESP3DClientType::system,
                              ESP3DAuthenticationLevel::admin);
    }
    return true;
  }
  // Start network services if initialization succeeded
  if (res) {
    _started = true;
    bool start_services = false;
#if defined(ETH_FEATURE)
    if (EthConfig::started()) {
      start_services = true;
      esp3d_log("Ethernet services ready");
    }
#endif  // ETH_FEATURE
#if defined(WIFI_FEATURE)
    if (WiFiConfig::started()) {
      start_services = true;
      esp3d_log("WiFi services ready");
    }
#endif  // WIFI_FEATURE
    if (start_services) {
      esp3d_log("Starting network services");
      res = NetServices::begin();
    }
  }
  // Workaround for AP hostname reset
#ifdef ARDUINO_ARCH_ESP32
#if defined(WIFI_FEATURE)
  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
    WiFi.softAPsetHostname(_hostname.c_str());
    esp3d_log("Set AP hostname: %s", _hostname.c_str());
  }
#endif  // WIFI_FEATURE
#endif  // ARDUINO_ARCH_ESP32
  if (res) {
    esp3d_log("Network config started successfully");
  } else {
    esp3d_log_e("Network config failed");
    end();
  }
#if defined(DISPLAY_DEVICE)
  ESP3DRequest reqId = {
      .id = ESP_OUTPUT_IP_ADDRESS,
  };
  esp3d_commands.dispatch((uint8_t*)localIP().c_str(), localIP().length(), ESP3DClientType::rendering, reqId,
                          ESP3DMessageType::unique, ESP3DClientType::system,
                          ESP3DAuthenticationLevel::admin);
#endif  // DISPLAY_DEVICE
  return res;
}

/**
 * End WiFi
 */
void NetConfig::end() {
  esp3d_log("Ending network");
  NetServices::end();
  _mode = ESP_NO_NETWORK;
#if defined(WIFI_FEATURE)
  WiFiConfig::end();
  _needReconnect2AP = false;
#else
  WiFi.mode(WIFI_OFF);
#endif  // WIFI_FEATURE

#if defined(ETH_FEATURE)
  EthConfig::end();
#endif  // ETH_FEATURE
#if defined(BLUETOOTH_FEATURE)
  bt_service.end();
#endif  // BLUETOOTH_FEATURE
  _started = false;
  esp3d_log("Network ended");
}

/**
 * Get hostname
 */
const char* NetConfig::hostname(bool fromsettings) {
  if (fromsettings) {
    _hostname = ESP3DSettings::readString(ESP_MODEL_NAME);
    esp3d_log("Read hostname from settings: %s", _hostname.c_str());
    return _hostname.c_str();
  }
#if defined(WIFI_FEATURE)
  if (WiFi.getMode() != WIFI_OFF) {
    _hostname = WiFiConfig::hostname();
    esp3d_log("WiFi hostname: %s", _hostname.c_str());
    return _hostname.c_str();
  }
#endif  // WIFI_FEATURE
#if defined(ETH_FEATURE)
  if (EthConfig::started()) {
    _hostname = ETH.getHostname();
    esp3d_log("Ethernet hostname: %s", _hostname.c_str());
    return _hostname.c_str();
  }
#endif  // ETH_FEATURE
#if defined(BLUETOOTH_FEATURE)
  if (bt_service.started()) {
    _hostname = bt_service.hostname();
    esp3d_log("Bluetooth hostname: %s", _hostname.c_str());
    return _hostname.c_str();
  }
#endif  // BLUETOOTH_FEATURE
  esp3d_log("Default hostname: %s", _hostname.c_str());
  return _hostname.c_str();
}

/**
 * Handle non-critical actions in sync environment
 */
void NetConfig::handle() {
  if (_started) {
#if defined(WIFI_FEATURE)
    if (_needReconnect2AP) {
      if (WiFi.getMode() != WIFI_OFF) {
        esp3d_log("Reconnecting to STA");
        int8_t tempMode = _mode; // Create a local int8_t variable
        WiFiConfig::begin(tempMode); // Retry STA connection
        _mode = tempMode; // Update _mode in case WiFiConfig::begin modifies it
      }
    }
    WiFiConfig::handle();
#endif  // WIFI_FEATURE
#if defined(ETH_FEATURE)
    EthConfig::handle();
#endif  // ETH_FEATURE
#if defined(BLUETOOTH_FEATURE)
    bt_service.handle();
#endif  // BLUETOOTH_FEATURE
    NetServices::handle();
    esp3d_log("Network handle executed");
  }
}

/**
 * Check if IP mode is DHCP
 */
bool NetConfig::isIPModeDHCP(uint8_t mode) {
  bool started = false;
#ifdef ARDUINO_ARCH_ESP32
  if (mode == wifi_ap || mode == wifi_sta || mode == wifi_apsta) {
    started = WiFiConfig::isDHCP();
    esp3d_log("WiFi DHCP mode: %d", started);
  }
#if defined(ETH_FEATURE)
  if (mode == eth_sta) {
    started = (EthConfig::ipMode() == DHCP_MODE);
    esp3d_log("Ethernet DHCP mode: %d", started);
  }
#endif  // ETH_FEATURE
#endif  // ARDUINO_ARCH_ESP32
#ifdef ARDUINO_ARCH_ESP8266
  (void)mode;
  started = (wifi_station_dhcpc_status() == DHCP_STARTED);
  esp3d_log("ESP8266 DHCP status: %d", started);
#endif  // ARDUINO_ARCH_ESP8266
  return started;
}

/**
 * Check if DHCP server is enabled
 */
bool NetConfig::isDHCPServer(uint8_t mode) {
  bool itis = false;
#ifdef ARDUINO_ARCH_ESP32
  if (mode == wifi_ap || mode == wifi_apsta) {
    esp_netif_dhcp_status_t dhcp_status;
    esp_netif_dhcps_get_status(get_esp_interface_netif(ESP_IF_WIFI_AP),
                               &dhcp_status);
    itis = (dhcp_status == ESP_NETIF_DHCP_STARTED);
    esp3d_log("DHCP server status: %d", itis);
  }
#endif  // ARDUINO_ARCH_ESP32
#ifdef ARDUINO_ARCH_ESP8266
  (void)mode;
  itis = (wifi_softap_dhcps_status() == DHCP_STARTED);
  esp3d_log("ESP8266 DHCP server status: %d", itis);
#endif  // ARDUINO_ARCH_ESP8266
  return itis;
}

#endif  // WIFI_FEATURE || ETH_FEATURE