/*
 ESP110.cpp - ESP3D command class

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
#if defined(WIFI_FEATURE) || defined(BLUETOOTH_FEATURE) || defined(ETH_FEATURE)
#include "../../modules/authentication/authentication_service.h"
#include "../../modules/network/netconfig.h"
#include "../esp3d_commands.h"
#include "../esp3d_settings.h"

#define COMMAND_ID 110
// Set radio state at boot: BT, WIFI-STA, WIFI-AP, WIFI-AP+STA, ETH-STA, OFF
// [ESP110]<state> json=<no> pwd=<admin password>

void ESP3DCommands::ESP110(int cmd_params_pos, ESP3DMessage* msg) {
  ESP3DClientType target = msg->origin;
  ESP3DRequest requestId = msg->request_id;
  msg->target = target;
  msg->origin = ESP3DClientType::command;

  bool hasError = false;
  String error_msg = "Invalid parameters";
  String ok_msg = "ok";
  bool json = hasTag(msg, cmd_params_pos, "json");
  String tmpstr;
  uint8_t byteValue = (uint8_t)-1;

#if defined(AUTHENTICATION_FEATURE)
  if (msg->authentication_level == ESP3DAuthenticationLevel::guest) {
    msg->authentication_level = ESP3DAuthenticationLevel::not_authenticated;
    dispatchAuthenticationError(msg, COMMAND_ID, json);
    return;
  }
#endif

  tmpstr = get_clean_param(msg, cmd_params_pos);

  // Query current mode
  if (tmpstr.length() == 0) {
    byteValue = ESP3DSettings::readByte(static_cast<int>(ESP3DSettingIndex::esp3d_radio_mode));
    switch (byteValue) {
      case bluetooth:         ok_msg = "BT"; break;
#if defined(WIFI_FEATURE)
      case wifi_ap:    ok_msg = "WIFI-AP"; break;
      case wifi_sta:   ok_msg = "WIFI-STA"; break;
      case wifi_apsta: ok_msg = "WIFI-AP+STA"; break;
      case ap_setup:   ok_msg = "WIFI-SETUP"; break;
#endif
#if defined(ETH_FEATURE)
      case eth_sta:    ok_msg = "ETH-STA"; break;
#endif
      case no_network: ok_msg = "OFF"; break;
      default:         ok_msg = "Unknown"; break;
    }
  } else {
    // Set new mode
#if defined(AUTHENTICATION_FEATURE)
    if (msg->authentication_level != ESP3DAuthenticationLevel::admin) {
      dispatchAuthenticationError(msg, COMMAND_ID, json);
      return;
    }
#endif

    tmpstr.toUpperCase();
    if      (tmpstr == "BT")           byteValue = bluetooth;
#if defined(WIFI_FEATURE)
    else if (tmpstr == "WIFI-AP")      byteValue = wifi_ap;
    else if (tmpstr == "WIFI-STA")     byteValue = wifi_sta;
    else if (tmpstr == "WIFI-AP+STA")  byteValue = wifi_apsta;
    else if (tmpstr == "WIFI-SETUP")   byteValue = ap_setup;
#endif
#if defined(ETH_FEATURE)
    else if (tmpstr == "ETH-STA")      byteValue = eth_sta;
#endif
    else if (tmpstr == "OFF")          byteValue = no_network;
    else                               byteValue = (uint8_t)-1;

    esp3d_log("Parsed mode: %s → %d (valid: %d)", tmpstr.c_str(), byteValue,
              ESP3DSettings::isValidByteSetting(byteValue, ESP3DSettingIndex::esp3d_radio_mode));

    if (ESP3DSettings::isValidByteSetting(byteValue, ESP3DSettingIndex::esp3d_radio_mode)) {
      if (!ESP3DSettings::writeByte(static_cast<int>(ESP3DSettingIndex::esp3d_radio_mode), byteValue)) {
        hasError = true;
        error_msg = "Set value failed";
      } else {
        if (!NetConfig::begin()) {
          hasError = true;
          error_msg = "Cannot setup network";
        } else {
          ok_msg = tmpstr;
        }
      }
    } else {
      hasError = true;
      error_msg = "Invalid parameter";
    }
  }

  if (!dispatch(msg, format_response(COMMAND_ID, json, !hasError, hasError ? error_msg.c_str() : ok_msg.c_str()),
                target, requestId, ESP3DMessageType::unique, msg->authentication_level)) {
    esp3d_log_e("Error sending response to clients");
  }
}

#endif  // WIFI_FEATURE || BLUETOOTH_FEATURE || ETH_FEATURE