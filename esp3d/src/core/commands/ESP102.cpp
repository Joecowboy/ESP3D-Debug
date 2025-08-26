/*
 ESP102.cpp - ESP3D command class

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
#include "../../modules/network/netconfig.h"
#include "../esp3d_commands.h"
#include "../esp3d_settings.h"
#include "../../modules/authentication/authentication_service.h"

#if defined(WIFI_FEATURE)
#include "../../modules/wifi/wificonfig.h"
#endif  // WIFI_FEATURE

#define COMMAND_ID 102
// Define IP modes if not defined in esp3d_config.h
#ifndef DHCP_MODE
enum ESP3DIPMode : uint8_t {
  DHCP_MODE = 0,
  STATIC_IP_MODE = 1
};
#endif

// Change STA IP mode (DHCP/STATIC)
//[ESP102]<mode>[json=no] [pwd=<admin password>]
void ESP3DCommands::ESP102(int cmd_params_pos, ESP3DMessage* msg) {
  ESP3DClientType target = msg->origin;
  ESP3DRequest requestId = msg->request_id;
  (void)requestId;
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
#endif  // AUTHENTICATION_FEATURE
  tmpstr = get_clean_param(msg, cmd_params_pos);
  if (tmpstr.length() == 0) {
    byteValue = ESP3DSettings::readByte(static_cast<int>(ESP3DSettingIndex::esp3d_sta_ip_mode));
    if (byteValue == static_cast<uint8_t>(DHCP_MODE)) {
      ok_msg = "DHCP";
    } else if (byteValue == static_cast<uint8_t>(STATIC_IP_MODE)) {
      ok_msg = "STATIC";
    } else {
      ok_msg = "Unknown:" + String(byteValue);
    }
  } else {
#if defined(AUTHENTICATION_FEATURE)
    if (msg->authentication_level != ESP3DAuthenticationLevel::admin) {
      dispatchAuthenticationError(msg, COMMAND_ID, json);
      return;
    }
#endif  // AUTHENTICATION_FEATURE
    tmpstr.toUpperCase();
    if (tmpstr == "DHCP") {
      byteValue = static_cast<uint8_t>(DHCP_MODE);
    } else if (tmpstr == "STATIC") {
      byteValue = static_cast<uint8_t>(STATIC_IP_MODE);
    } else {
      byteValue = (uint8_t)-1;  // unknown flag so put out of range value
    }
    esp3d_log("got %s param for a value of %d, is valid %d", tmpstr.c_str(),
              byteValue,
              ESP3DSettings::isValidByteSetting(byteValue, ESP3DSettingIndex::esp3d_sta_ip_mode));
    if (ESP3DSettings::isValidByteSetting(byteValue, ESP3DSettingIndex::esp3d_sta_ip_mode)) {
      esp3d_log("Value %d is valid", byteValue);
      if (!ESP3DSettings::writeByte(static_cast<int>(ESP3DSettingIndex::esp3d_sta_ip_mode), byteValue)) {
        hasError = true;
        error_msg = "Set value failed";
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

#endif  // WIFI_FEATURE