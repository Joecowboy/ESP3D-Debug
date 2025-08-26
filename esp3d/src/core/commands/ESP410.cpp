/*
 ESP410.cpp - ESP3D command class

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
#if defined(WIFI_FEATURE)
#include "../../modules/authentication/authentication_service.h"
#include "../../modules/wifi/wificonfig.h"
#include "../esp3d_commands.h"
#include "../esp3d_settings.h"
#include "../esp3d_string.h"
#include "../../core/esp3d_log.h" // Added for enhanced logging

#define COMMAND_ID 410

void ESP3DCommands::ESP410(int cmd_params_pos, ESP3DMessage* msg) {
  ESP3DClientType target = msg->origin;
  ESP3DRequest requestId = msg->request_id;
  msg->target = target;
  msg->origin = ESP3DClientType::command;

  bool hasError = false;
  String error_msg = "Invalid parameters";
  String ok_msg = "ok";
  bool json = hasTag(msg, cmd_params_pos, "json");
  String tmpstr;

  esp3d_log("Processing [ESP410] command, json=%d", json);

#if defined(AUTHENTICATION_FEATURE)
  if (msg->authentication_level == ESP3DAuthenticationLevel::guest) {
    esp3d_log_e("Authentication error for [ESP410]");
    msg->authentication_level = ESP3DAuthenticationLevel::not_authenticated;
    dispatchAuthenticationError(msg, COMMAND_ID, json);
    return;
  }
#endif

  tmpstr = get_clean_param(msg, cmd_params_pos);
  if (tmpstr.length() != 0) {
    hasError = true;
    error_msg = "This command doesn't take parameters";
    esp3d_log_e("%s", error_msg.c_str());
    if (!dispatchAnswer(msg, COMMAND_ID, json, hasError, error_msg.c_str())) {
      esp3d_log_e("Error sending response to clients");
    }
    return;
  }

  WiFiMode_t currentMode = WiFi.getMode();
  esp3d_log("Current WiFi mode: %d (%s)", currentMode,
            currentMode == WIFI_STA ? "STA" :
            currentMode == WIFI_AP ? "AP" :
            currentMode == WIFI_AP_STA ? "AP_STA" : "OFF");

  if (currentMode == WIFI_STA || currentMode == WIFI_AP || currentMode == WIFI_AP_STA) {
    int n = 0;
    uint8_t total = 0;
    String configuredSSID = ESP3DSettings::readString(static_cast<int>(ESP3DSettingIndex::esp3d_sta_ssid));
    bool ssidFound = false;

    tmpstr = json ? "{\"cmd\":\"410\",\"status\":\"ok\",\"data\":[" : "Start Scan\n";
    msg->type = ESP3DMessageType::head;
    esp3d_log("Sending scan header: %s", tmpstr.c_str());
    if (!dispatch(msg, tmpstr.c_str(), target, requestId, ESP3DMessageType::head, msg->authentication_level)) {
      esp3d_log_e("Error sending response header to clients");
      return;
    }

    // Switch to AP_STA if needed
    bool switchedMode = false;
    if (currentMode == WIFI_AP) {
      esp3d_log("Switching to WIFI_AP_STA for scanning");
      if (!WiFi.mode(WIFI_AP_STA)) {
        hasError = true;
        error_msg = "Failed to switch to AP_STA mode";
        esp3d_log_e("%s", error_msg.c_str());
        if (!dispatchAnswer(msg, COMMAND_ID, json, hasError, error_msg.c_str())) {
          esp3d_log_e("Error sending mode switch error to clients");
        }
        return;
      }
      switchedMode = true;
    }

    esp3d_log("Starting WiFi scan");
    n = WiFi.scanNetworks();
    esp3d_log("Scan completed, found %d networks", n);

    if (n < 0) {
      hasError = true;
      error_msg = "WiFi scan failed";
      esp3d_log_e("%s", error_msg.c_str());
      if (!dispatchAnswer(msg, COMMAND_ID, json, hasError, error_msg.c_str())) {
        esp3d_log_e("Error sending scan failure to clients");
      }
      if (switchedMode) {
        esp3d_log("Restoring WiFi mode to AP");
        WiFi.mode(currentMode);
      }
      return;
    }

    for (int i = 0; i < n; ++i) {
      if (WiFi.RSSI(i) < MIN_RSSI) {
        esp3d_log("Skipping network %s, RSSI %d < MIN_RSSI %d", WiFi.SSID(i).c_str(), WiFi.RSSI(i), MIN_RSSI);
        continue;
      }

      String ssid = WiFi.SSID(i);
      tmpstr = "";
      if (total++ > 0 && json) tmpstr += ",";

      if (json) {
        tmpstr += "{\"SSID\":\"";
        tmpstr += esp3d_string::encodeString(ssid.c_str());
        tmpstr += "\",\"SIGNAL\":\"";
      } else {
        tmpstr += ssid;
        tmpstr += "\t";
      }

      tmpstr += String(WiFiConfig::getSignal(WiFi.RSSI(i)));
      if (!json) tmpstr += "%";

      if (json) {
        tmpstr += "\",\"IS_PROTECTED\":\"";
        tmpstr += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? "0" : "1";
        tmpstr += "\"}";
      } else {
        tmpstr += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? "\tOpen\n" : "\tSecure\n";
      }

      esp3d_log("Network %d: %s, Signal: %s, Protected: %s",
                i, ssid.c_str(), String(WiFiConfig::getSignal(WiFi.RSSI(i))).c_str(),
                WiFi.encryptionType(i) == ENC_TYPE_NONE ? "Open" : "Secure");

      if (!dispatch(msg, tmpstr.c_str(), target, requestId, ESP3DMessageType::core, msg->authentication_level)) {
        esp3d_log_e("Error sending network %d to clients", i);
      }

      if (ssid == configuredSSID) {
        ssidFound = true;
      }
    }

    // Report configured SSID status
    tmpstr = json ? String("{\"ConfiguredSSID\":\"") + String(esp3d_string::encodeString(configuredSSID.c_str())) + "\"," :
                    String("Configured SSID: ") + configuredSSID + "\n";
    tmpstr += json ? "\"Found\":\"" + String(ssidFound ? "yes" : "no") + "\"," :
                    "Found: " + String(ssidFound ? "yes" : "no") + "\n";
    tmpstr += json ? "\"Connected\":\"" + String(WiFi.isConnected() ? "yes" : "no") + "\"}" :
                    "Connected: " + String(WiFi.isConnected() ? "yes" : "no") + "\n";
    esp3d_log("Configured SSID: %s, Found: %s, Connected: %s",
              configuredSSID.c_str(), ssidFound ? "yes" : "no", WiFi.isConnected() ? "yes" : "no");
    if (!dispatch(msg, tmpstr.c_str(), target, requestId, ESP3DMessageType::core, msg->authentication_level)) {
      esp3d_log_e("Error sending configured SSID status to clients");
    }

    // Restore mode if changed
    if (switchedMode) {
      esp3d_log("Restoring WiFi mode to %d", currentMode);
      if (!WiFi.mode(currentMode)) {
        esp3d_log_e("Failed to restore WiFi mode to %d", currentMode);
      }
    }

    WiFi.scanDelete();
    esp3d_log("Scan results deleted");

    tmpstr = json ? "]}" : "End Scan\n";
    esp3d_log("Sending scan footer: %s", tmpstr.c_str());
    if (!dispatch(msg, tmpstr.c_str(), target, requestId, ESP3DMessageType::tail, msg->authentication_level)) {
      esp3d_log_e("Error sending scan footer to clients");
    }
  } else {
    hasError = true;
    error_msg = "WiFi not enabled";
    esp3d_log_e("%s", error_msg.c_str());
    if (!dispatchAnswer(msg, COMMAND_ID, json, hasError, error_msg.c_str())) {
      esp3d_log_e("Error sending WiFi not enabled response to clients");
    }
  }
}

#endif  // WIFI_FEATURE