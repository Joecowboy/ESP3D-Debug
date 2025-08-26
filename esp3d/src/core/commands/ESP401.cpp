/*
 ESP401.cpp - ESP3D command class

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
#include "../../modules/authentication/authentication_service.h"
#include "../esp3d_commands.h"
#include "../esp3d_settings.h"
#include "../../core/esp3d_log.h"
#if defined(WS_DATA_FEATURE)
#include "../../modules/websocket/websocket_server.h"
#endif  // WS_DATA_FEATURE
#if defined(WIFI_FEATURE)
#include "../../modules/wifi/wificonfig.h"
#endif  // WIFI_FEATURE
#if defined(WIFI_FEATURE) || defined(ETH_FEATURE)
#include "../../modules/network/netconfig.h"
#endif  // WIFI_FEATURE || ETH_FEATURE
#if defined(ESP_SERIAL_BRIDGE_OUTPUT)
#include "../../modules/serial/serial_service.h"
#endif  // ESP_SERIAL_BRIDGE_OUTPUT
#ifdef SENSOR_DEVICE
#include "../../modules/sensor/sensor.h"
#endif  // SENSOR_DEVICE
#ifdef BUZZER_DEVICE
#include "../../modules/buzzer/buzzer.h"
#endif  // BUZZER_DEVICE
#ifdef TIMESTAMP_FEATURE
#include "../../modules/time/time_service.h"
#endif  // TIMESTAMP_FEATURE
#ifdef NOTIFICATION_FEATURE
#include "../../modules/notifications/notifications_service.h"
#endif  // NOTIFICATION_FEATURE
#ifdef SD_DEVICE
#include "../../modules/filesystem/esp_sd.h"
#endif  // SD_DEVICE

#define COMMAND_ID 401

// Set EEPROM setting
// [ESP401]P=<position> T=<type> V=<value> json=<no> pwd=<user/admin password>
void ESP3DCommands::ESP401(int cmd_params_pos, ESP3DMessage* msg) {
  ESP3DClientType target = msg->origin;
  ESP3DRequest requestId = msg->request_id;
  msg->target = target;
  msg->origin = ESP3DClientType::command;
  bool hasError = false;
  String error_msg = "Invalid parameters";
  String ok_msg = "ok";
  String spos = get_param(msg, cmd_params_pos, "P=");
  bool foundV = false;
  String sval = get_param(msg, cmd_params_pos, "V=", &foundV);
  String styp = get_param(msg, cmd_params_pos, "T=");
  bool json = hasTag(msg, cmd_params_pos, "json");

  esp3d_log("Processing [ESP401] P=%s T=%s V=%s json=%d", 
            spos.c_str(), styp.c_str(), sval.c_str(), json);

#if defined(AUTHENTICATION_FEATURE)
  if (msg->authentication_level != ESP3DAuthenticationLevel::admin) {
    esp3d_log_e("Authentication error: admin access required");
    msg->authentication_level = ESP3DAuthenticationLevel::not_authenticated;
    dispatchAuthenticationError(msg, COMMAND_ID, json);
    return;
  }
#endif  // AUTHENTICATION_FEATURE

  if (spos.length() == 0) {
    error_msg = "Invalid parameter P";
    hasError = true;
    esp3d_log_e("%s", error_msg.c_str());
  } else if (styp.length() == 0) {
    error_msg = "Invalid parameter T";
    hasError = true;
    esp3d_log_e("%s", error_msg.c_str());
  } else if (sval.length() == 0 && !foundV) {
    error_msg = "Invalid parameter V";
    hasError = true;
    esp3d_log_e("%s", error_msg.c_str());
  } else if (!(styp == "B" || styp == "S" || styp == "A" || styp == "I")) {
    error_msg = "Invalid value for T";
    hasError = true;
    esp3d_log_e("%s", error_msg.c_str());
  }

  if (!hasError) {
    ESP3DSettingIndex position = static_cast<ESP3DSettingIndex>(spos.toInt());
    switch (styp[0]) {
      case 'B':  // Byte value
        if (position == ESP3DSettingIndex::esp3d_radio_mode) {
          uint8_t val = (uint8_t)sval.toInt();
          if (val <= 6) {  // Supports ESP_NO_NETWORK(0), ESP_BT(1), ESP_WIFI_STA(2), ESP_WIFI_AP(3), ESP_AP_SETUP(4), ESP_ETH_STA(5), ESP_WIFI_APSTA(6)
            if (!ESP3DSettings::writeByte(static_cast<int>(position), val)) {
              error_msg = "Set failed for esp3d_radio_mode";
              hasError = true;
              esp3d_log_e("Set failed for esp3d_radio_mode=%d", val);
            } else {
              esp3d_log("Set esp3d_radio_mode to %d", val);
#if defined(WIFI_FEATURE) || defined(ETH_FEATURE)
              if (!NetConfig::begin()) {
                error_msg = "Cannot setup network for mode " + String(val);
                hasError = true;
                esp3d_log_e("Network setup failed for mode %d", val);
              } else {
                esp3d_log("Network setup successful for mode %d, STA Connected: %s", 
                          val, WiFi.isConnected() ? "yes" : "no");
              }
#endif  // WIFI_FEATURE || ETH_FEATURE
            }
          } else {
            error_msg = "Invalid value for esp3d_radio_mode: " + String(val);
            hasError = true;
            esp3d_log_e("Invalid value %d for esp3d_radio_mode", val);
          }
        } else if (ESP3DSettings::isValidByteSetting((uint8_t)sval.toInt(), position)) {
          if (!ESP3DSettings::writeByte(static_cast<int>(position), (uint8_t)sval.toInt())) {
            error_msg = "Set failed for byte setting at position " + spos;
            hasError = true;
            esp3d_log_e("Set failed for byte setting at position %d", static_cast<int>(position));
          } else {
            esp3d_log("Set byte setting at position %d to %d", static_cast<int>(position), sval.toInt());
          }
        } else {
          error_msg = "Invalid byte value for position " + spos;
          hasError = true;
          esp3d_log_e("Invalid byte value %d for position %d", sval.toInt(), static_cast<int>(position));
        }
        break;
      case 'I':  // Integer value
        if (position == ESP3DSettingIndex::esp3d_websocket_port) {
          uint32_t port = sval.toInt();
          if (port >= 1 && port <= 65535) {
            if (!ESP3DSettings::writeUint32(static_cast<int>(position), port)) {
              error_msg = "Set failed for WebSocket port";
              hasError = true;
              esp3d_log_e("Failed to set WebSocket port %d", port);
            } else {
              esp3d_log("Set WebSocket port to %d", port);
#if defined(WS_DATA_FEATURE)
              if (!websocket_terminal_server.begin()) {
                error_msg = "Cannot restart WebSocket server on port " + String(port);
                hasError = true;
                esp3d_log_e("Failed to restart WebSocket server on port %d", port);
              } else {
                esp3d_log("WebSocket server restarted on port %d", port);
              }
#endif  // WS_DATA_FEATURE
            }
          } else {
            error_msg = "Invalid WebSocket port value: " + String(port);
            hasError = true;
            esp3d_log_e("Invalid WebSocket port %d", port);
          }
        } else if (ESP3DSettings::isValidIntegerSetting(sval.toInt(), position)) {
          if (!ESP3DSettings::writeUint32(static_cast<int>(position), sval.toInt())) {
            error_msg = "Set failed for integer setting at position " + spos;
            hasError = true;
            esp3d_log_e("Set failed for integer setting at position %d", static_cast<int>(position));
          } else {
            esp3d_log("Set integer setting at position %d to %d", static_cast<int>(position), sval.toInt());
          }
        } else {
          error_msg = "Invalid integer value for position " + spos;
          hasError = true;
          esp3d_log_e("Invalid integer value %d for position %d", sval.toInt(), static_cast<int>(position));
        }
        break;
      case 'S':  // String value
        if (position == ESP3DSettingIndex::esp3d_sta_ssid) {
#if defined(WIFI_FEATURE)
          if (WiFi.getMode() != WIFI_OFF) {
            int n = WiFi.scanNetworks();
            bool ssidFound = false;
            for (int i = 0; i < n; ++i) {
              if (WiFi.SSID(i) == sval && WiFi.RSSI(i) >= MIN_RSSI) {
                ssidFound = true;
                break;
              }
            }
            WiFi.scanDelete();
            if (!ssidFound) {
              error_msg = "SSID " + sval + " not found or signal too weak";
              hasError = true;
              esp3d_log_e("SSID %s not found or signal < MIN_RSSI %d", sval.c_str(), MIN_RSSI);
            }
          }
#endif  // WIFI_FEATURE
          if (!hasError && ESP3DSettings::isValidStringSetting(sval.c_str(), position)) {
            if (!ESP3DSettings::writeString(static_cast<int>(position), sval.c_str())) {
              error_msg = "Set failed for STA SSID";
              hasError = true;
              esp3d_log_e("Set failed for STA SSID at position %d", static_cast<int>(position));
            } else {
              esp3d_log("Set STA SSID to %s", sval.c_str());
#if defined(WIFI_FEATURE)
              if (WiFi.getMode() == WIFI_STA || WiFi.getMode() == WIFI_AP_STA) {
                if (!NetConfig::begin()) {
                  error_msg = "Cannot setup network after setting STA SSID";
                  hasError = true;
                  esp3d_log_e("Network setup failed after setting STA SSID");
                } else {
                  esp3d_log("Network setup successful after setting STA SSID, Connected: %s", 
                            WiFi.isConnected() ? "yes" : "no");
                }
              }
#endif  // WIFI_FEATURE
            }
          } else if (!hasError) {
            error_msg = "Invalid STA SSID";
            hasError = true;
            esp3d_log_e("Invalid STA SSID %s for position %d", sval.c_str(), static_cast<int>(position));
          }
        } else if (position == ESP3DSettingIndex::esp3d_sta_password) {
          if (ESP3DSettings::isValidStringSetting(sval.c_str(), position)) {
            if (!ESP3DSettings::writeString(static_cast<int>(position), sval.c_str())) {
              error_msg = "Set failed for STA Password";
              hasError = true;
              esp3d_log_e("Set failed for STA Password at position %d", static_cast<int>(position));
            } else {
              esp3d_log("Set STA Password successfully");
#if defined(WIFI_FEATURE)
              if (WiFi.getMode() == WIFI_STA || WiFi.getMode() == WIFI_AP_STA) {
                if (!NetConfig::begin()) {
                  error_msg = "Cannot setup network after setting STA Password";
                  hasError = true;
                  esp3d_log_e("Network setup failed after setting STA Password");
                } else {
                  esp3d_log("Network setup successful after setting STA Password, Connected: %s", 
                            WiFi.isConnected() ? "yes" : "no");
                }
              }
#endif  // WIFI_FEATURE
            }
          } else {
            error_msg = "Invalid STA Password";
            hasError = true;
            esp3d_log_e("Invalid STA Password for position %d", static_cast<int>(position));
          }
        } else if (ESP3DSettings::isValidStringSetting(sval.c_str(), position)) {
          if (!ESP3DSettings::writeString(static_cast<int>(position), sval.c_str())) {
            error_msg = "Set failed for string setting at position " + spos;
            hasError = true;
            esp3d_log_e("Set failed for string setting at position %d", static_cast<int>(position));
          } else {
            esp3d_log("Set string setting at position %d to %s", static_cast<int>(position), sval.c_str());
          }
        } else {
          error_msg = "Invalid string value for position " + spos;
          hasError = true;
          esp3d_log_e("Invalid string value %s for position %d", sval.c_str(), static_cast<int>(position));
        }
        break;
      case 'A':  // IP address
        if (ESP3DSettings::isValidIPStringSetting(sval.c_str(), position)) {
          if (!ESP3DSettings::writeIPString(static_cast<int>(position), sval.c_str())) {
            error_msg = "Set failed for IP setting at position " + spos;
            hasError = true;
            esp3d_log_e("Set failed for IP setting at position %d", static_cast<int>(position));
          } else {
            esp3d_log("Set IP setting at position %d to %s", static_cast<int>(position), sval.c_str());
#if defined(WIFI_FEATURE)
            if (position == ESP3DSettingIndex::esp3d_sta_ip_value || 
                position == ESP3DSettingIndex::esp3d_sta_gateway_value || 
                position == ESP3DSettingIndex::esp3d_sta_mask_value || 
                position == ESP3DSettingIndex::esp3d_sta_dns_value) {
              if (WiFi.getMode() == WIFI_STA || WiFi.getMode() == WIFI_AP_STA) {
                if (!NetConfig::begin()) {
                  error_msg = "Cannot setup network after setting IP";
                  hasError = true;
                  esp3d_log_e("Network setup failed after setting IP at position %d", static_cast<int>(position));
                } else {
                  esp3d_log("Network setup successful after setting IP, Connected: %s", 
                            WiFi.isConnected() ? "yes" : "no");
                }
              }
            }
#endif  // WIFI_FEATURE
          }
        } else {
          error_msg = "Invalid IP value for position " + spos;
          hasError = true;
          esp3d_log_e("Invalid IP value %s for position %d", sval.c_str(), static_cast<int>(position));
        }
        break;
      default:
        error_msg = "Invalid value for T: " + styp;
        hasError = true;
        esp3d_log_e("Invalid type %s", styp.c_str());
        break;
    }

    if (!hasError) {
      switch (position) {
#if defined(ESP_SERIAL_BRIDGE_OUTPUT)
        case ESP3DSettingIndex::esp3d_serial_bridge_on:
          if (!serial_bridge_service.started()) {
            esp3d_log("Starting serial bridge");
            if (!serial_bridge_service.begin()) {
              error_msg = "Failed to start serial bridge";
              hasError = true;
              esp3d_log_e("Failed to start serial bridge");
            } else {
              esp3d_log("Serial bridge started");
            }
          }
          break;
#endif  // ESP_SERIAL_BRIDGE_OUTPUT
        case ESP3DSettingIndex::esp3d_verbose_boot:
          esp3d_log("Setting verbose boot to %d", sval.toInt());
          // ESP3DSettings::isVerboseBoot(true); // Removed: no such method
          break;
        case ESP3DSettingIndex::esp3d_session_timeout:
          esp3d_log("Setting session timeout to %d minutes", sval.toInt());
          AuthenticationService::setSessionTimeout(1000 * 60 * sval.toInt());
          break;
#ifdef SD_DEVICE
        case ESP3DSettingIndex::esp3d_sd_speed_div:
          esp3d_log("Setting SD SPI speed divider to %d", sval.toInt());
          ESP_SD::setSPISpeedDivider(sval.toInt());
          break;
#endif  // SD_DEVICE
#ifdef TIMESTAMP_FEATURE
        case ESP3DSettingIndex::esp3d_time_server1:
        case ESP3DSettingIndex::esp3d_time_server2:
        case ESP3DSettingIndex::esp3d_time_server3:
          esp3d_log("Starting time service");
          timeService.begin();
          break;
#endif  // TIMESTAMP_FEATURE
#ifdef NOTIFICATION_FEATURE
        case ESP3DSettingIndex::esp3d_auto_notification:
          esp3d_log("Setting auto notification to %s", sval.toInt() == 0 ? "false" : "true");
          notificationsservice.setAutonotification(sval.toInt() != 0);
          break;
#endif  // NOTIFICATION_FEATURE
#ifdef SENSOR_DEVICE
        case ESP3DSettingIndex::esp3d_sensor_type:
          esp3d_log("Starting sensor");
          esp3d_sensor.begin();
          break;
        case ESP3DSettingIndex::esp3d_sensor_interval:
          esp3d_log("Setting sensor interval to %d", sval.toInt());
          esp3d_sensor.setInterval(sval.toInt());
          break;
#endif  // SENSOR_DEVICE
#ifdef BUZZER_DEVICE
        case ESP3DSettingIndex::esp3d_buzzer:
          if (sval.toInt() == 1) {
            esp3d_log("Starting buzzer");
            esp3d_buzzer.begin();
          } else if (sval.toInt() == 0) {
            esp3d_log("Stopping buzzer");
            esp3d_buzzer.end();
          }
          break;
#endif  // BUZZER_DEVICE
#if defined(ESP_SERIAL_BRIDGE_OUTPUT)
        case ESP3DSettingIndex::esp3d_serial_bridge_baud:
          esp3d_log("Updating serial bridge baud rate to %d", sval.toInt());
          serial_bridge_service.updateBaudRate(sval.toInt());
          break;
#endif  // ESP_SERIAL_BRIDGE_OUTPUT
#ifdef AUTHENTICATION_FEATURE
        case ESP3DSettingIndex::esp3d_admin_pwd:
        case ESP3DSettingIndex::esp3d_user_pwd:
          esp3d_log("Updating authentication password for position %d", static_cast<int>(position));
          AuthenticationService::update();
          break;
#endif  // AUTHENTICATION_FEATURE
        default:
          esp3d_log("No specific action for position %d", static_cast<int>(position));
          break;
      }
    }
  }

  if (!hasError) {
    if (json) {
      ok_msg = "{\"position\":\"" + spos + "\",\"value\":\"" + sval + "\",\"type\":\"" + styp + "\"}";
    } else {
      ok_msg = "ok";
    }
    esp3d_log("Setting applied successfully: P=%s, T=%s, V=%s", spos.c_str(), styp.c_str(), sval.c_str());
  } else {
    if (json) {
      error_msg = "{\"error\":\"" + error_msg + "\",\"position\":\"" + spos + "\"}";
    } else {
      error_msg += spos.length() > 0 ? " for P=" + spos : "";
    }
    esp3d_log_e("Setting failed: %s", error_msg.c_str());
  }

  if (!dispatch(msg, format_response(COMMAND_ID, json, !hasError, hasError ? error_msg.c_str() : ok_msg.c_str()),
                target, requestId, ESP3DMessageType::unique, msg->authentication_level)) {
    esp3d_log_e("Error sending response to clients");
  }
}