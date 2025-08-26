/*
  esp3d_commands.cpp - ESP3D commands class

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

#include "esp3d_commands.h"
#include "../include/esp3d_config.h"
#include "esp3d.h"
#include "esp3d_settings.h"

#if defined(ESP_LOG_FEATURE)
const char *esp3dclientstr[] = {
    "no_client",     "serial",        "usb_serial",      "stream",
    "telnet",        "http",          "webui_websocket", "websocket",
    "rendering",     "bluetooth",     "socket_serial",   "echo_serial",
    "serial_bridge", "remote_screen", "mks_serial",      "command",
    "system",        "all_clients"};
#define GETCLIENTSTR(id)                                         \
  static_cast<uint8_t>(id) >= 0 &&                               \
          static_cast<uint8_t>(id) <=                            \
              static_cast<uint8_t>(ESP3DClientType::all_clients) \
      ? esp3dclientstr[static_cast<uint8_t>(id)]                 \
      : "Out of index"

const char *esp3dmsgstr[] = {"head", "core", "tail", "unique"};
#define GETMSGTYPESTR(id)                                    \
  static_cast<uint8_t>(id) >= 0 &&                           \
          static_cast<uint8_t>(id) <=                        \
              static_cast<uint8_t>(ESP3DMessageType::unique) \
      ? esp3dmsgstr[static_cast<uint8_t>(id)]                \
      : "Out of index"
#endif  // ESP_LOG_FEATURE

#if COMMUNICATION_PROTOCOL == MKS_SERIAL
#include "../modules/mks/mks_service.h"
#endif  // COMMUNICATION_PROTOCOL == MKS_SERIAL

#if defined(TELNET_FEATURE)
#include "../modules/telnet/telnet_server.h"
#endif  // TELNET_FEATURE

#if defined(HTTP_FEATURE) || defined(WS_DATA_FEATURE)
#include "../modules/websocket/websocket_server.h"
#endif  // HTTP_FEATURE || WS_DATA_FEATURE

#if defined(HTTP_FEATURE)
#include "../modules/http/http_server.h"
#endif  // HTTP_FEATURE

#if defined(ESP_LUA_INTERPRETER_FEATURE)
#include "../modules/lua_interpreter/lua_interpreter_service.h"
#endif  // ESP_LUA_INTERPRETER_FEATURE

#ifdef BLUETOOTH_FEATURE
#include "../modules/bluetooth/BT_service.h"
#endif  // BLUETOOTH_FEATURE

#if defined(DISPLAY_DEVICE)
#include "../modules/display/display.h"
#endif  // DISPLAY_DEVICE

#if defined(GCODE_HOST_FEATURE)
#include "../modules/gcode_host/gcode_host.h"
#endif  // GCODE_HOST_FEATURE

#if defined(USB_SERIAL_FEATURE)
#include "../modules/usb-serial/usb_serial_service.h"
#endif  // USB_SERIAL_FEATURE

#ifdef FTP_FEATURE
#include "../modules/ftp/FtpServer.h"
#endif // FTP_FEATURE

ESP3DCommands esp3d_commands;

ESP3DCommands::ESP3DCommands() {
#if COMMUNICATION_PROTOCOL == RAW_SERIAL
  _output_client = ESP3DClientType::serial;
#endif  // COMMUNICATION_PROTOCOL == RAW_SERIAL
#if COMMUNICATION_PROTOCOL == MKS_SERIAL
  _output_client = ESP3DClientType::mks_serial;
#endif  // COMMUNICATION_PROTOCOL == MKS_SERIAL
#if COMMUNICATION_PROTOCOL == SOCKET_SERIAL
  _output_client = ESP3DClientType::socket_serial;
#endif  // COMMUNICATION_PROTOCOL == SOCKET_SERIAL
}

ESP3DCommands::~ESP3DCommands() {}

bool ESP3DCommands::begin() {
  esp3d_log("Starting ESP3D Commands");
#if defined(HTTP_FEATURE)
  if (!HTTP_Server::begin()) {
    esp3d_log_e("Error starting HTTP server");
    return false;
  }
#endif  // HTTP_FEATURE
#if defined(TELNET_FEATURE)
  if (!telnet_server.begin()) {
    esp3d_log_e("Error starting Telnet server");
    return false;
  }
#endif  // TELNET_FEATURE
#if defined(WS_DATA_FEATURE)
  if (!websocket_data_server.begin()) {
    esp3d_log_e("Error starting WebSocket server");
    return false;
  }
#endif  // WS_DATA_FEATURE
#if defined(FTP_FEATURE)
  if (!ftp_server.begin()) {
    esp3d_log_e("Error starting FTP server");
    return false;
  }
#endif  // FTP_FEATURE
#if defined(WEBDAV_FEATURE)
  if (!webdav_server.begin()) {
    esp3d_log_e("Error starting WebDav server");
    return false;
  }
#endif  // WEBDAV_FEATURE
  return true;
}

void ESP3DCommands::handle() {
#if defined(HTTP_FEATURE)
  HTTP_Server::handle();
#endif  // HTTP_FEATURE
#if defined(TELNET_FEATURE)
  telnet_server.handle();
#endif  // TELNET_FEATURE
#if defined(WS_DATA_FEATURE)
  websocket_data_server.handle();
#endif  // WS_DATA_FEATURE
#if defined(FTP_FEATURE)
  ftp_server.handle();
#endif  // FTP_FEATURE
#if defined(WEBDAV_FEATURE)
  webdav_server.handle();
#endif  // WEBDAV_FEATURE
#if defined(AUTHENTICATION_FEATURE)
  AuthenticationService::handle();
#endif  // AUTHENTICATION_FEATURE
}

// Check if current line is an [ESPXXX] command
bool ESP3DCommands::is_esp_command(uint8_t *sbuf, size_t len) {
  esp3d_log("Checking for ESP command, len: %zu", len);
  if (len < 5) {
    return false;
  }
  if ((char(sbuf[0]) == '[') && (char(sbuf[1]) == 'E') &&
      (char(sbuf[2]) == 'S') && (char(sbuf[3]) == 'P') &&
      ((char(sbuf[4]) == ']') || (char(sbuf[5]) == ']') ||
       (char(sbuf[6]) == ']') || (char(sbuf[7]) == ']'))) {
    esp3d_log("Found ESP command");
    return true;
  }
  if (len >= 14) {
    if ((char(sbuf[0]) == 'e') && (char(sbuf[1]) == 'c') &&
        (char(sbuf[2]) == 'h') && (char(sbuf[3]) == 'o') &&
        (char(sbuf[4]) == ':') && (char(sbuf[5]) == ' ') &&
        (char(sbuf[6]) == '[') && (char(sbuf[7]) == 'E') &&
        (char(sbuf[8]) == 'S') && (char(sbuf[9]) == 'P') &&
        ((char(sbuf[10]) == ']') || (char(sbuf[11]) == ']') ||
         (char(sbuf[12]) == ']') || (char(sbuf[13]) == ']'))) {
      esp3d_log("Found ESP command with echo prefix");
      return true;
    }
  }
  return false;
}

const char *ESP3DCommands::format_response(uint cmdID, bool isjson, bool isok,
                                           const char *message) {
  static String res;
  res = "";
  esp3d_log("Formatting response for cmd %d, json: %d, ok: %d, msg: %s",
            cmdID, isjson, isok, message);
  if (!isjson) {
    res += message;
  } else {
    res = "{\"cmd\":\"";
    res += String(cmdID);
    res += "\",\"status\":\"";
    if (isok) {
      res += "ok";
    } else {
      res += "error";
    }
    res += "\",\"data\":";
    if (message[0] != '{') {
      res += "\"";
    }
    res += message;
    if (message[0] != '{') {
      res += "\"";
    }
    res += "}";
  }
  return res.c_str();
}

bool ESP3DCommands::hasTag(ESP3DMessage *msg, uint start, const char *label) {
  if (!msg) {
    esp3d_log_e("No msg for tag %s", label);
    return false;
  }
  String lbl = label;
  esp3d_log("Checking message for tag %s", label);
  uint lenLabel = strlen(label);
  lbl += "=";
  lbl = get_param(msg, start, lbl.c_str());
  if (lbl.length() != 0) {
    esp3d_log("Label is used with parameter %s", lbl.c_str());
    lbl.toUpperCase();
    return (lbl == "YES" || lbl == "1" || lbl == "TRUE");
  }
  bool prevCharIsEscaped = false;
  bool prevCharIsspace = true;
  for (uint i = start; i < msg->size; i++) {
    char c = char(msg->data[i]);
    if (c == label[0] && prevCharIsspace) {
      uint p = 0;
      while (i < msg->size && p < lenLabel && c == label[p]) {
        i++;
        p++;
        if (i < msg->size) {
          c = char(msg->data[i]);
        }
      }
      if (p == lenLabel) {
        if (i == msg->size || std::isspace(c)) {
          esp3d_log("Label %s found", label);
          return true;
        }
      }
      if (std::isspace(c) && !prevCharIsEscaped) {
        prevCharIsspace = true;
      }
      if (c == '\\') {
        prevCharIsEscaped = true;
      } else {
        prevCharIsEscaped = false;
      }
    }
  }
  esp3d_log("Label %s not found", label);
  return false;
}

const char *ESP3DCommands::get_param(ESP3DMessage *msg, uint start,
                                     const char *label, bool *found) {
  if (!msg) {
    esp3d_log_e("No message");
    return "";
  }
  return get_param((const char *)msg->data, msg->size, start, label, found);
}

const char *ESP3DCommands::get_param(const char *data, uint size, uint start,
                                     const char *label, bool *found) {
  esp3d_log("Getting param %s", label);
  int startPos = -1;
  uint lenLabel = strlen(label);
  static String value;
  bool prevCharIsEscaped = false;
  bool prevCharIsspace = true;
  value = "";
  uint startp = start;
  if (found) {
    *found = false;
  }
  if (data[startp] == ' ') {
    while (data[startp] == ' ' && startp < size) {
      startp++;
    }
  }
  for (uint i = startp; i < size; i++) {
    char c = char(data[i]);
    if (c == label[0] && startPos == -1 && prevCharIsspace) {
      uint p = 0;
      while (i < size && p < lenLabel && c == label[p]) {
        i++;
        p++;
        if (i < size) {
          c = char(data[i]);
        }
      }
      if (p == lenLabel) {
        startPos = i;
        if (found) {
          *found = true;
        }
      }
    }
    if (std::isspace(c) && !prevCharIsEscaped) {
      prevCharIsspace = true;
    }
    if (startPos > -1 && i < size) {
      if (c == '\\') {
        prevCharIsEscaped = true;
      }
      if (std::isspace(c) && !prevCharIsEscaped) {
        esp3d_log("Found param %s: %s", label, value.c_str());
        return value.c_str();
      }
      if (c != '\\') {
        value += c;
        prevCharIsEscaped = false;
      }
    }
  }
  esp3d_log("Found param %s: %s", label, value.c_str());
  return value.c_str();
}

const char *ESP3DCommands::get_clean_param(ESP3DMessage *msg, uint start) {
  if (!msg) {
    esp3d_log_e("No message");
    return "";
  }
  static String value;
  bool prevCharIsEscaped = false;
  uint startp = start;
  while (char(msg->data[startp]) == ' ' && startp < msg->size) {
    startp++;
  }
  value = "";
  for (uint i = startp; i < msg->size; i++) {
    char c = char(msg->data[i]);
    if (c == '\\') {
      prevCharIsEscaped = true;
    }
    if (std::isspace(c) && !prevCharIsEscaped) {
      esp3d_log("Testing clean param: %s", value.c_str());
      if (value == "json" || value.startsWith("json=") ||
          value.startsWith("pwd=")) {
        esp3d_log("Clearing param: %s", value.c_str());
        value = "";
      } else {
        esp3d_log("Returning clean param: %s", value.c_str());
        return value.c_str();
      }
    }
    if (c != '\\') {
      if ((std::isspace(c) && prevCharIsEscaped) || !std::isspace(c)) {
        value += c;
      }
      prevCharIsEscaped = false;
    }
  }
  esp3d_log("Testing clean param: %s", value.c_str());
  if (value == "json" || value.startsWith("json=") ||
      value.startsWith("pwd=")) {
    esp3d_log("Clearing param: %s", value.c_str());
    value = "";
  }
  esp3d_log("Returning clean param: %s", value.c_str());
  return value.c_str();
}

bool ESP3DCommands::has_param(ESP3DMessage *msg, uint start) {
  return strlen(get_clean_param(msg, start)) != 0;
}

void ESP3DCommands::process(ESP3DMessage *msg) {
  if (!msg || !msg->data || msg->size == 0) {
    esp3d_log_e("Invalid message for processing");
    return;
  }
  esp3d_log("Processing message from client %s, size: %zu",
            GETCLIENTSTR(msg->origin), msg->size);

  // Check if it's an ESP command
  if (!is_esp_command(msg->data, msg->size)) {
    esp3d_log("Not an ESP command");
    dispatchAnswer(msg, 0, hasTag(msg, 0, "json"), false, "Not an ESP command");
    return;
  }

  // Extract command ID
  String cmdStr = String((const char *)msg->data);
  int startCmd = cmdStr.indexOf("[ESP") + 4;
  int endCmd = cmdStr.indexOf(']', startCmd);
  if (endCmd == -1 || startCmd == -1) {
    esp3d_log_e("Invalid ESP command format");
    dispatchAnswer(msg, 0, hasTag(msg, 0, "json"), false, "Invalid command format");
    return;
  }

  String cmdIdStr = cmdStr.substring(startCmd, endCmd);
  int cmdId = cmdIdStr.toInt();
  int cmd_params_pos = endCmd + 1;

  // Handle echo prefix if present
  if (cmdStr.startsWith("echo: ")) {
    cmd_params_pos += 6; // Skip "echo: "
  }

  // Execute the command
  execute_internal_command(cmdId, cmd_params_pos, msg);
}

void ESP3DCommands::execute_internal_command(int cmd, int cmd_params_pos,
                                             ESP3DMessage *msg) {
  if (!msg) {
    esp3d_log_e("No msg for cmd %d", cmd);
    return;
  }
  esp3d_log("Executing command %d", cmd);
#ifdef AUTHENTICATION_FEATURE
  String pwd = get_param(msg, cmd_params_pos, "pwd=");
  if (pwd.length() != 0 && (msg->origin == ESP3DClientType::serial ||
                            msg->origin == ESP3DClientType::serial_bridge ||
                            msg->origin == ESP3DClientType::telnet ||
                            msg->origin == ESP3DClientType::websocket ||
                            msg->origin == ESP3DClientType::usb_serial ||
                            msg->origin == ESP3DClientType::bluetooth)) {
    msg->authentication_level =
        AuthenticationService::getAuthenticatedLevel(pwd.c_str(), msg);
    esp3d_log("Authentication level set to %d", msg->authentication_level);
    switch (msg->origin) {
      case ESP3DClientType::serial:
        esp3d_serial_service.setAuthentication(msg->authentication_level);
        break;
#if defined(ESP_SERIAL_BRIDGE_OUTPUT)
      case ESP3DClientType::serial_bridge:
        serial_bridge_service.setAuthentication(msg->authentication_level);
        break;
#endif  // ESP_SERIAL_BRIDGE_OUTPUT
#if defined(USB_SERIAL_FEATURE)
      case ESP3DClientType::usb_serial:
        esp3d_usb_serial_service.setAuthentication(msg->authentication_level);
        break;
#endif  // USB_SERIAL_FEATURE
#if defined(TELNET_FEATURE)
      case ESP3DClientType::telnet:
        telnet_server.setAuthentication(msg->authentication_level);
        break;
#endif  // TELNET_FEATURE
#if defined(WS_DATA_FEATURE)
      case ESP3DClientType::websocket:
        websocket_data_server.setAuthentication(msg->authentication_level);
        break;
#endif  // WS_DATA_FEATURE
#ifdef BLUETOOTH_FEATURE
      case ESP3DClientType::bluetooth:
        bt_service.setAuthentication(msg->authentication_level);
        break;
#endif  // BLUETOOTH_FEATURE
      default:
        break;
    }
  }
#endif  // AUTHENTICATION_FEATURE

  bool json = hasTag(msg, cmd_params_pos, "json");
  esp3d_log("JSON output: %d", json);

  switch (cmd) {
    // ESP3D Help
    case 0:
      esp3d_log("Processing [ESP0]");
      ESP0(cmd_params_pos, msg);
      break;

#if defined(WIFI_FEATURE)
    // STA SSID
    case 100:
      esp3d_log("Processing [ESP100]");
      ESP100(cmd_params_pos, msg);
      break;
    // STA Password
    case 101:
      esp3d_log("Processing [ESP101]");
      ESP101(cmd_params_pos, msg);
      break;
    // Change STA IP mode (DHCP/STATIC)
    case 102:
      esp3d_log("Processing [ESP102]");
      ESP102(cmd_params_pos, msg);
      break;
    // Change STA IP/Mask/GW
    case 103:
      esp3d_log("Processing [ESP103]");
      ESP103(cmd_params_pos, msg);
      break;
    // Set fallback mode
    case 104:
      esp3d_log("Processing [ESP104]");
      ESP104(cmd_params_pos, msg);
      break;
    // AP SSID
    case 105:
      esp3d_log("Processing [ESP105]");
      ESP105(cmd_params_pos, msg);
      break;
    // AP Password
    case 106:
      esp3d_log("Processing [ESP106]");
      ESP106(cmd_params_pos, msg);
      break;
    // Change AP IP
    case 107:
      esp3d_log("Processing [ESP107]");
      ESP107(cmd_params_pos, msg);
      break;
    // Change AP channel
    case 108:
      esp3d_log("Processing [ESP108]");
      ESP108(cmd_params_pos, msg);
      break;
#endif  // WIFI_FEATURE

#if defined(WIFI_FEATURE) || defined(BLUETOOTH_FEATURE) || defined(ETH_FEATURE)
    // Set radio state
    case 110:
      esp3d_log("Processing [ESP110]");
      ESP110(cmd_params_pos, msg);
      break;
#endif  // WIFI_FEATURE || BLUETOOTH_FEATURE || ETH_FEATURE

#if defined(WIFI_FEATURE) || defined(ETH_FEATURE)
    // Get current IP
    case 111:
      esp3d_log("Processing [ESP111]");
      ESP111(cmd_params_pos, msg);
      break;
#endif  // WIFI_FEATURE || ETH_FEATURE

#if defined(WIFI_FEATURE) || defined(ETH_FEATURE) || defined(BT_FEATURE)
    // Get/Set hostname
    case 112:
      esp3d_log("Processing [ESP112]");
      ESP112(cmd_params_pos, msg);
      break;
    // Get/Set boot Network state
    case 114:
      esp3d_log("Processing [ESP114]");
      ESP114(cmd_params_pos, msg);
      break;
    // Get/Set immediate Network state
    case 115:
      esp3d_log("Processing [ESP115]");
      ESP115(cmd_params_pos, msg);
      break;
#endif  // WIFI_FEATURE || ETH_FEATURE || BT_FEATURE

#if defined(ETH_FEATURE)
    // Change ETH STA IP mode
    case 116:
      esp3d_log("Processing [ESP116]");
      ESP116(cmd_params_pos, msg); // Fixed: Changed from ESP102 to ESP116
      break;
    // Change ETH STA IP/Mask/GW
    case 117:
      esp3d_log("Processing [ESP117]");
      ESP117(cmd_params_pos, msg);
      break;
    // Set fallback mode
    case 118:
      esp3d_log("Processing [ESP118]");
      ESP118(cmd_params_pos, msg);
      break;
#endif  // ETH_FEATURE

#ifdef HTTP_FEATURE
    // Set HTTP state
    case 120:
      esp3d_log("Processing [ESP120]");
      ESP120(cmd_params_pos, msg);
      break;
    // Set HTTP port
    case 121:
      esp3d_log("Processing [ESP121]");
      ESP121(cmd_params_pos, msg);
      break;
#endif  // HTTP_FEATURE

#ifdef TELNET_FEATURE
    // Set Telnet state
    case 130:
      esp3d_log("Processing [ESP130]");
      ESP130(cmd_params_pos, msg);
      break;
    // Set Telnet port
    case 131:
      esp3d_log("Processing [ESP131]");
      ESP131(cmd_params_pos, msg);
      break;
#endif  // TELNET_FEATURE

#ifdef TIMESTAMP_FEATURE
    // Sync/Set/Get time
    case 140:
      esp3d_log("Processing [ESP140]");
      ESP140(cmd_params_pos, msg);
      break;
#endif  // TIMESTAMP_FEATURE

    // Get/Set boot delay
    case 150:
      esp3d_log("Processing [ESP150]");
      ESP150(cmd_params_pos, msg);
      break;

#ifdef WS_DATA_FEATURE
    // Set WebSocket state
    case 160:
      esp3d_log("Processing [ESP160]");
      ESP160(cmd_params_pos, msg);
      break;
    // Set WebSocket port
    case 161:
      esp3d_log("Processing [ESP161]");
      ESP161(cmd_params_pos, msg);
      break;
#endif  // WS_DATA_FEATURE

#ifdef CAMERA_DEVICE
    // Get/Set Camera settings
    case 170:
      esp3d_log("Processing [ESP170]");
      ESP170(cmd_params_pos, msg);
      break;
    // Save frame
    case 171:
      esp3d_log("Processing [ESP171]");
      ESP171(cmd_params_pos, msg);
      break;
#endif  // CAMERA_DEVICE

#ifdef FTP_FEATURE
    // Set FTP state
    case 180:
      esp3d_log("Processing [ESP180]");
      ESP180(cmd_params_pos, msg);
      break;
    // Set/Get FTP ports
    case 181:
      esp3d_log("Processing [ESP181]");
      ESP181(cmd_params_pos, msg);
      break;
#endif  // FTP_FEATURE

#ifdef WEBDAV_FEATURE
    // Set WebDAV state
    case 190:
      esp3d_log("Processing [ESP190]");
      ESP190(cmd_params_pos, msg);
      break;
    // Set/Get WebDAV port
    case 191:
      esp3d_log("Processing [ESP191]");
      ESP191(cmd_params_pos, msg);
      break;
#endif  // WEBDAV_FEATURE

#if defined(SD_DEVICE)
    // Get/Set SD Card Status
    case 200:
      esp3d_log("Processing [ESP200]");
      ESP200(cmd_params_pos, msg);
      break;
#if SD_DEVICE != ESP_SDIO
    // Get/Set SD card Speed
    case 202:
      esp3d_log("Processing [ESP202]");
      ESP202(cmd_params_pos, msg);
      break;
#endif  // SD_DEVICE != ESP_SDIO
#ifdef SD_UPDATE_FEATURE
    // Get/Set SD Check at boot
    case 402:
      esp3d_log("Processing [ESP402]");
      ESP402(cmd_params_pos, msg);
      break;
#endif  // SD_UPDATE_FEATURE
#endif  // SD_DEVICE

#ifdef DIRECT_PIN_FEATURE
    // Get/Set pin value
    case 201:
      esp3d_log("Processing [ESP201]");
      ESP201(cmd_params_pos, msg);
      break;
#endif  // DIRECT_PIN_FEATURE

#ifdef SENSOR_DEVICE
    // Get/Set SENSOR
    case 210:
      esp3d_log("Processing [ESP210]");
      ESP210(cmd_params_pos, msg);
      break;
#endif  // SENSOR_DEVICE

#if defined(PRINTER_HAS_DISPLAY)
    // Output to printer screen
    case 212:
      esp3d_log("Processing [ESP212]");
      ESP212(cmd_params_pos, msg);
      break;
#endif  // PRINTER_HAS_DISPLAY

#if defined(DISPLAY_DEVICE)
    // Output to ESP screen
    case 214:
      esp3d_log("Processing [ESP214]");
      ESP214(cmd_params_pos, msg);
      break;
#if defined(DISPLAY_TOUCH_DRIVER)
    // Touch Calibration
    case 215:
      esp3d_log("Processing [ESP215]");
      ESP215(cmd_params_pos, msg);
      break;
#endif  // DISPLAY_TOUCH_DRIVER
#endif  // DISPLAY_DEVICE

#ifdef BUZZER_DEVICE
    // Play sound
    case 250:
      esp3d_log("Processing [ESP250]");
      ESP250(cmd_params_pos, msg);
      break;
#endif  // BUZZER_DEVICE

    // Show pins
    case 220:
      esp3d_log("Processing [ESP220]");
      ESP220(cmd_params_pos, msg);
      break;

    // Delay command
    case 290:
      esp3d_log("Processing [ESP290]");
      ESP290(cmd_params_pos, msg);
      break;

#if defined(ESP_LUA_INTERPRETER_FEATURE)
    // Execute Lua script
    case 300:
      esp3d_log("Processing [ESP300]");
      ESP300(cmd_params_pos, msg);
      break;
    // Query and Control ESP300
    case 301:
      esp3d_log("Processing [ESP301]");
      ESP301(cmd_params_pos, msg);
      break;
#endif  // ESP_LUA_INTERPRETER_FEATURE

    // Get full ESP3D settings
    case 400:
      esp3d_log("Processing [ESP400]");
      ESP400(cmd_params_pos, msg);
      break;

    // Set EEPROM setting
    case 401:
      esp3d_log("Processing [ESP401]");
      ESP401(cmd_params_pos, msg);
      break;

#if defined(WIFI_FEATURE)
    // Get available AP list
    case 410:
      esp3d_log("Processing [ESP410]");
      ESP410(cmd_params_pos, msg);
      break;
#endif  // WIFI_FEATURE

    // Get ESP current status
    case 420:
      esp3d_log("Processing [ESP420]");
      ESP420(cmd_params_pos, msg);
      break;

    // Set ESP State
    case 444:
      esp3d_log("Processing [ESP444]");
      ESP444(cmd_params_pos, msg);
      break;

#ifdef MDNS_FEATURE
    // Get ESP3D list
    case 450:
      esp3d_log("Processing [ESP450]");
      ESP450(cmd_params_pos, msg);
      break;
#endif  // MDNS_FEATURE

#ifdef AUTHENTICATION_FEATURE
    // Get current authentication level
    case 500:
      esp3d_log("Processing [ESP500]");
      ESP500(cmd_params_pos, msg);
      break;
    // Set/display session timeout
    case 510:
      esp3d_log("Processing [ESP510]");
      ESP510(cmd_params_pos, msg);
      break;
    // Change admin password
    case 550:
      esp3d_log("Processing [ESP550]");
      ESP550(cmd_params_pos, msg);
      break;
    // Change user password
    case 555:
      esp3d_log("Processing [ESP555]");
      ESP555(cmd_params_pos, msg);
      break;
#endif  // AUTHENTICATION_FEATURE

#if defined(NOTIFICATION_FEATURE)
    // Send Notification
    case 600:
      esp3d_log("Processing [ESP600]");
      ESP600(cmd_params_pos, msg);
      break;
    // Set/Get Notification settings
    case 610:
      esp3d_log("Processing [ESP610]");
      ESP610(cmd_params_pos, msg);
      break;
    // Send Notification using URL
    case 620:
      esp3d_log("Processing [ESP620]");
      ESP620(cmd_params_pos, msg);
      break;
#endif  // NOTIFICATION_FEATURE

#if defined(FILESYSTEM_FEATURE)
    // Format ESP Filesystem
    case 710:
      esp3d_log("Processing [ESP710]");
      ESP710(cmd_params_pos, msg);
      break;
    // List ESP Filesystem
    case 720:
      esp3d_log("Processing [ESP720]");
      ESP720(cmd_params_pos, msg);
      break;
    // Action on ESP Filesystem
    case 730:
      esp3d_log("Processing [ESP730]");
      ESP730(cmd_params_pos, msg);
      break;
#endif  // FILESYSTEM_FEATURE

#if defined(SD_DEVICE)
    // Format SD Filesystem
    case 715:
      esp3d_log("Processing [ESP715]");
      ESP715(cmd_params_pos, msg);
      break;
#endif  // SD_DEVICE

#if defined(GCODE_HOST_FEATURE)
    // Open local file
    case 700:
      esp3d_log("Processing [ESP700]");
      ESP700(cmd_params_pos, msg);
      break;
    // Get Status and Control ESP700
    case 701:
      esp3d_log("Processing [ESP701]");
      ESP701(cmd_params_pos, msg);
      break;
#endif  // GCODE_HOST_FEATURE

#if defined(SD_DEVICE)
    // List SD Filesystem
    case 740:
      esp3d_log("Processing [ESP740]");
      ESP740(cmd_params_pos, msg);
      break;
    // Action on SD Filesystem
    case 750:
      esp3d_log("Processing [ESP750]");
      ESP750(cmd_params_pos, msg);
      break;
#endif  // SD_DEVICE

#if defined(GLOBAL_FILESYSTEM_FEATURE)
    // Global FS Actions
    case 780:
      esp3d_log("Processing [ESP780]");
      ESP780(cmd_params_pos, msg);
      break;
    // Global FS Control
    case 790:
      esp3d_log("Processing [ESP790]");
      ESP790(cmd_params_pos, msg);
      break;
#endif  // GLOBAL_FILESYSTEM_FEATURE

    // System Commands
    case 800:
      esp3d_log("Processing [ESP800]");
      ESP800(cmd_params_pos, msg);
      break;

#if COMMUNICATION_PROTOCOL != SOCKET_SERIAL
    // Serial Commands
    case 900:
      esp3d_log("Processing [ESP900]");
      ESP900(cmd_params_pos, msg);
      break;
    case 901:
      esp3d_log("Processing [ESP901]");
      ESP901(cmd_params_pos, msg);
      break;
#endif  // COMMUNICATION_PROTOCOL != SOCKET_SERIAL

#if defined(USB_SERIAL_FEATURE)
    // USB Serial Commands
    case 902:
      esp3d_log("Processing [ESP902]");
      ESP902(cmd_params_pos, msg);
      break;
    case 950:
      esp3d_log("Processing [ESP950]");
      ESP950(cmd_params_pos, msg);
      break;
#endif  // USB_SERIAL_FEATURE

#if defined(ESP_SERIAL_BRIDGE_OUTPUT)
    // Serial Bridge Commands
    case 930:
      esp3d_log("Processing [ESP930]");
      ESP930(cmd_params_pos, msg);
      break;
    case 931:
      esp3d_log("Processing [ESP931]");
      ESP931(cmd_params_pos, msg);
      break;
#endif  // ESP_SERIAL_BRIDGE_OUTPUT

#ifdef BUZZER_DEVICE
    // Buzzer Commands
    case 910:
      esp3d_log("Processing [ESP910]");
      ESP910(cmd_params_pos, msg);
      break;
#endif  // BUZZER_DEVICE

#if defined(ARDUINO_ARCH_ESP32) &&                             \
    (CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32S2 || \
     CONFIG_IDF_TARGET_ESP32C3)
    // Platform-specific Commands
    case 999:
      esp3d_log("Processing [ESP999]");
      ESP999(cmd_params_pos, msg);
      break;
#endif  // ARDUINO_ARCH_ESP32

    default:
      esp3d_log_e("Unknown command %d", cmd);
      dispatchAnswer(msg, cmd, json, false, "Unknown command");
      break;
  }
}

bool ESP3DCommands::dispatchAnswer(ESP3DMessage *msg, uint cmdID, bool json,
                                   bool isOk, const char *answer) {
  if (!msg) {
    esp3d_log_e("No message to dispatch");
    return false;
  }
  String response = format_response(cmdID, json, isOk, answer);
  ESP3DMessage *newMsg = new ESP3DMessage();
  if (!newMsg) {
    esp3d_log_e("Cannot create response message");
    return false;
  }
  newMsg->data = new uint8_t[response.length() + 1];
  if (!newMsg->data) {
    esp3d_log_e("Cannot allocate response data");
    delete newMsg;
    return false;
  }
  newMsg->size = response.length();
  memcpy(newMsg->data, response.c_str(), newMsg->size);
  newMsg->data[newMsg->size] = '\0';
  newMsg->type = ESP3DMessageType::tail;
  newMsg->origin = msg->origin;
  newMsg->authentication_level = msg->authentication_level;

  bool success = false;
  switch (msg->origin) {
    case ESP3DClientType::serial:
      success = esp3d_serial_service.dispatch(newMsg);
      break;
#if defined(ESP_SERIAL_BRIDGE_OUTPUT)
    case ESP3DClientType::serial_bridge:
      success = serial_bridge_service.dispatch(newMsg);
      break;
#endif  // ESP_SERIAL_BRIDGE_OUTPUT
#if defined(USB_SERIAL_FEATURE)
    case ESP3DClientType::usb_serial:
      success = esp3d_usb_serial_service.dispatch(newMsg);
      break;
#endif  // USB_SERIAL_FEATURE
#if defined(TELNET_FEATURE)
    case ESP3DClientType::telnet:
      success = telnet_server.dispatch(newMsg);
      break;
#endif  // TELNET_FEATURE
#if defined(WS_DATA_FEATURE)
    case ESP3DClientType::websocket:
      success = websocket_data_server.dispatch(newMsg);
      break;
#endif  // WS_DATA_FEATURE
#ifdef BLUETOOTH_FEATURE
    case ESP3DClientType::bluetooth:
      success = bt_service.dispatch(newMsg);
      break;
#endif  // BLUETOOTH_FEATURE
    default:
      esp3d_log_e("Unsupported client type for response: %d", msg->origin);
      delete[] newMsg->data;
      delete newMsg;
      return false;
  }
  if (!success) {
    esp3d_log_e("Failed to send message to client %d", msg->origin);
    delete[] newMsg->data;
    delete newMsg;
    return false;
  }
  return true;
}

ESP3DClientType ESP3DCommands::getOutputClient(bool force_update) {
  static uint32_t last_check = 0;
  if (force_update || (millis() - last_check > 1000)) {
    _output_client = ESP3DClientType::no_client;
#if COMMUNICATION_PROTOCOL == RAW_SERIAL
    if (esp3d_serial_service.started()) {
      _output_client = ESP3DClientType::serial;
    }
#endif  // COMMUNICATION_PROTOCOL == RAW_SERIAL
#if COMMUNICATION_PROTOCOL == MKS_SERIAL
    if (esp3d_serial_service.started()) {
      _output_client = ESP3DClientType::mks_serial;
    }
#endif  // COMMUNICATION_PROTOCOL == MKS_SERIAL
#if COMMUNICATION_PROTOCOL == SOCKET_SERIAL
    if (Serial2Socket.isConnected()) {
      _output_client = ESP3DClientType::socket_serial;
    }
#endif  // COMMUNICATION_PROTOCOL == SOCKET_SERIAL
#if defined(USB_SERIAL_FEATURE)
    if (esp3d_usb_serial_service.started()) {
      _output_client = ESP3DClientType::usb_serial;
    }
#endif  // USB_SERIAL_FEATURE
    last_check = millis();
  }
  return _output_client;
}

bool ESP3DCommands::dispatch(ESP3DMessage *msg, const char *sbuf) {
  if (!msg || !sbuf) {
    esp3d_log_e("Invalid message or buffer");
    return false;
  }
  ESP3DMessage *newMsg = new ESP3DMessage();
  if (!newMsg) {
    esp3d_log_e("Cannot create dispatch message");
    return false;
  }
  newMsg->data = new uint8_t[strlen(sbuf) + 1];
  if (!newMsg->data) {
    esp3d_log_e("Cannot allocate dispatch data");
    delete newMsg;
    return false;
  }
  newMsg->size = strlen(sbuf);
  memcpy(newMsg->data, sbuf, newMsg->size);
  newMsg->data[newMsg->size] = '\0';
  newMsg->type = ESP3DMessageType::unique;
  newMsg->origin = msg->origin;
  newMsg->target = msg->target;
  newMsg->authentication_level = msg->authentication_level;

  bool success = false;
  switch (msg->target) {
    case ESP3DClientType::serial:
      success = esp3d_serial_service.dispatch(newMsg);
      break;
#if defined(ESP_SERIAL_BRIDGE_OUTPUT)
    case ESP3DClientType::serial_bridge:
      success = serial_bridge_service.dispatch(newMsg);
      break;
#endif  // ESP_SERIAL_BRIDGE_OUTPUT
#if defined(USB_SERIAL_FEATURE)
    case ESP3DClientType::usb_serial:
      success = esp3d_usb_serial_service.dispatch(newMsg);
      break;
#endif  // USB_SERIAL_FEATURE
#if defined(TELNET_FEATURE)
    case ESP3DClientType::telnet:
      success = telnet_server.dispatch(newMsg);
      break;
#endif  // TELNET_FEATURE
#if defined(WS_DATA_FEATURE)
    case ESP3DClientType::websocket:
      success = websocket_data_server.dispatch(newMsg);
      break;
#endif  // WS_DATA_FEATURE
#ifdef BLUETOOTH_FEATURE
    case ESP3DClientType::bluetooth:
      success = bt_service.dispatch(newMsg);
      break;
#endif  // BLUETOOTH_FEATURE
    default:
      esp3d_log_e("Unsupported client type for dispatch: %d", msg->target);
      delete[] newMsg->data;
      delete newMsg;
      return false;
  }
  if (!success) {
    esp3d_log_e("Failed to dispatch message to client %d", msg->target);
    delete[] newMsg->data;
    delete newMsg;
    return false;
  }
  return true;
}

bool ESP3DCommands::dispatch(uint8_t *sbuf, size_t size, ESP3DClientType target,
                             ESP3DRequest requestId, ESP3DMessageType type,
                             ESP3DClientType origin,
                             ESP3DAuthenticationLevel authentication_level) {
  if (!sbuf || size == 0) {
    esp3d_log_e("Invalid buffer");
    return false;
  }
  ESP3DMessage *newMsg = new ESP3DMessage();
  if (!newMsg) {
    esp3d_log_e("Cannot create dispatch message");
    return false;
  }
  newMsg->data = new uint8_t[size + 1];
  if (!newMsg->data) {
    esp3d_log_e("Cannot allocate dispatch data");
    delete newMsg;
    return false;
  }
  newMsg->size = size;
  memcpy(newMsg->data, sbuf, size);
  newMsg->data[size] = '\0';
  newMsg->type = type;
  newMsg->origin = origin;
  newMsg->target = target;
  newMsg->request_id = requestId;
  newMsg->authentication_level = authentication_level;

  bool success = false;
  switch (target) {
    case ESP3DClientType::serial:
      success = esp3d_serial_service.dispatch(newMsg);
      break;
#if defined(ESP_SERIAL_BRIDGE_OUTPUT)
    case ESP3DClientType::serial_bridge:
      success = serial_bridge_service.dispatch(newMsg);
      break;
#endif  // ESP_SERIAL_BRIDGE_OUTPUT
#if defined(USB_SERIAL_FEATURE)
    case ESP3DClientType::usb_serial:
      success = esp3d_usb_serial_service.dispatch(newMsg);
      break;
#endif  // USB_SERIAL_FEATURE
#if defined(TELNET_FEATURE)
    case ESP3DClientType::telnet:
      success = telnet_server.dispatch(newMsg);
      break;
#endif  // TELNET_FEATURE
#if defined(WS_DATA_FEATURE)
    case ESP3DClientType::websocket:
      success = websocket_data_server.dispatch(newMsg);
      break;
#endif  // WS_DATA_FEATURE
#ifdef BLUETOOTH_FEATURE
    case ESP3DClientType::bluetooth:
      success = bt_service.dispatch(newMsg);
      break;
#endif  // BLUETOOTH_FEATURE
    default:
      esp3d_log_e("Unsupported client type for dispatch: %d", target);
      delete[] newMsg->data;
      delete newMsg;
      return false;
  }
  if (!success) {
    esp3d_log_e("Failed to dispatch message to client %d", target);
    delete[] newMsg->data;
    delete newMsg;
    return false;
  }
  return true;
}

bool ESP3DCommands::dispatch(ESP3DMessage *msg, uint8_t *sbuf, size_t len) {
  if (!msg || !sbuf || len == 0) {
    esp3d_log_e("Invalid message or buffer");
    return false;
  }
  ESP3DMessage *newMsg = new ESP3DMessage();
  if (!newMsg) {
    esp3d_log_e("Cannot create dispatch message");
    return false;
  }
  newMsg->data = new uint8_t[len + 1];
  if (!newMsg->data) {
    esp3d_log_e("Cannot allocate dispatch data");
    delete newMsg;
    return false;
  }
  newMsg->size = len;
  memcpy(newMsg->data, sbuf, len);
  newMsg->data[len] = '\0';
  newMsg->type = ESP3DMessageType::unique;
  newMsg->origin = msg->origin;
  newMsg->target = msg->target;
  newMsg->authentication_level = msg->authentication_level;

  bool success = false;
  switch (msg->target) {
    case ESP3DClientType::serial:
      success = esp3d_serial_service.dispatch(newMsg);
      break;
#if defined(ESP_SERIAL_BRIDGE_OUTPUT)
    case ESP3DClientType::serial_bridge:
      success = serial_bridge_service.dispatch(newMsg);
      break;
#endif  // ESP_SERIAL_BRIDGE_OUTPUT
#if defined(USB_SERIAL_FEATURE)
    case ESP3DClientType::usb_serial:
      success = esp3d_usb_serial_service.dispatch(newMsg);
      break;
#endif  // USB_SERIAL_FEATURE
#if defined(TELNET_FEATURE)
    case ESP3DClientType::telnet:
      success = telnet_server.dispatch(newMsg);
      break;
#endif  // TELNET_FEATURE
#if defined(WS_DATA_FEATURE)
    case ESP3DClientType::websocket:
      success = websocket_data_server.dispatch(newMsg);
      break;
#endif  // WS_DATA_FEATURE
#ifdef BLUETOOTH_FEATURE
    case ESP3DClientType::bluetooth:
      success = bt_service.dispatch(newMsg);
      break;
#endif  // BLUETOOTH_FEATURE
    default:
      esp3d_log_e("Unsupported client type for dispatch: %d", msg->target);
      delete[] newMsg->data;
      delete newMsg;
      return false;
  }
  if (!success) {
    esp3d_log_e("Failed to dispatch message to client %d", msg->target);
    delete[] newMsg->data;
    delete newMsg;
    return false;
  }
  return true;
}

bool ESP3DCommands::dispatch(ESP3DMessage *msg) {
  if (!msg || !msg->data) {
    esp3d_log_e("Invalid message");
    return false;
  }
  bool success = false;
  switch (msg->target) {
    case ESP3DClientType::serial:
      success = esp3d_serial_service.dispatch(msg);
      break;
#if defined(ESP_SERIAL_BRIDGE_OUTPUT)
    case ESP3DClientType::serial_bridge:
      success = serial_bridge_service.dispatch(msg);
      break;
#endif  // ESP_SERIAL_BRIDGE_OUTPUT
#if defined(USB_SERIAL_FEATURE)
    case ESP3DClientType::usb_serial:
      success = esp3d_usb_serial_service.dispatch(msg);
      break;
#endif  // USB_SERIAL_FEATURE
#if defined(TELNET_FEATURE)
    case ESP3DClientType::telnet:
      success = telnet_server.dispatch(msg);
      break;
#endif  // TELNET_FEATURE
#if defined(WS_DATA_FEATURE)
    case ESP3DClientType::websocket:
      success = websocket_data_server.dispatch(msg);
      break;
#endif  // WS_DATA_FEATURE
#ifdef BLUETOOTH_FEATURE
    case ESP3DClientType::bluetooth:
      success = bt_service.dispatch(msg);
      break;
#endif  // BLUETOOTH_FEATURE
    default:
      esp3d_log_e("Unsupported client type for dispatch: %d", msg->target);
      return false;
  }
  if (!success) {
    esp3d_log_e("Failed to dispatch message to client %d", msg->target);
    return false;
  }
  return true;
}

bool ESP3DCommands::dispatchSetting(bool json, const char *filter,
                                    ESP3DSettingIndex index, const char *help,
                                    const char **optionValues,
                                    const char **optionLabels,
                                    uint32_t maxsize, uint32_t minsize,
                                    uint32_t minsize2, uint8_t precision,
                                    const char *unit, bool needRestart,
                                    ESP3DClientType target,
                                    ESP3DRequest requestId, bool isFirst) {
  String response;
  if (json) {
    response = isFirst ? "" : ",";
    response += "{\"filter\":\"";
    response += filter;
    response += "\",\"id\":\"";
    response += String(static_cast<uint32_t>(index));
    response += "\",\"help\":\"";
    response += help;
    response += "\",\"value\":\"";
    String value = ESP3DSettings::readString(static_cast<int>(index));
    response += value;
    response += "\",\"options\":";
    if (optionValues && optionLabels) {
      response += "[";
      for (uint i = 0; optionValues[i] != nullptr; i++) {
        if (i > 0) response += ",";
        response += "{\"label\":\"";
        response += optionLabels[i];
        response += "\",\"value\":\"";
        response += optionValues[i];
        response += "\"}";
      }
      response += "]";
    } else {
      response += "[]";
    }
    response += ",\"maxsize\":";
    response += String(maxsize);
    response += ",\"minsize\":";
    response += String(minsize);
    response += ",\"minsize2\":";
    response += String(minsize2);
    response += ",\"precision\":";
    response += String(precision);
    response += ",\"unit\":\"";
    if (unit) response += unit;
    response += "\",\"restart\":";
    response += needRestart ? "true" : "false";
    response += "}";
  } else {
    response = help;
    response += ": ";
    response += ESP3DSettings::readString(static_cast<int>(index));
    if (optionValues && optionLabels) {
      response += " [";
      for (uint i = 0; optionValues[i] != nullptr; i++) {
        if (i > 0) response += ", ";
        response += optionLabels[i];
        response += "=";
        response += optionValues[i];
      }
      response += "]";
    }
    response += "\n";
  }
  return dispatch(nullptr, response.c_str(), target, requestId,
                  isFirst ? ESP3DMessageType::head : ESP3DMessageType::core, ESP3DAuthenticationLevel::guest);
}

bool ESP3DCommands::dispatchAuthenticationError(ESP3DMessage *msg, uint cmdid,
                                               bool json) {
  if (!msg) {
    esp3d_log_e("No message for authentication error");
    return false;
  }
  String response = format_response(cmdid, json, false, "Authentication error");
  ESP3DMessage *newMsg = new ESP3DMessage();
  if (!newMsg) {
    esp3d_log_e("Cannot create authentication error message");
    return false;
  }
  newMsg->data = new uint8_t[response.length() + 1];
  if (!newMsg->data) {
    esp3d_log_e("Cannot allocate authentication error data");
    delete newMsg;
    return false;
  }
  newMsg->size = response.length();
  memcpy(newMsg->data, response.c_str(), newMsg->size);
  newMsg->data[newMsg->size] = '\0';
  newMsg->type = ESP3DMessageType::unique;
  newMsg->origin = msg->origin;
  newMsg->target = msg->target;
  newMsg->authentication_level = msg->authentication_level;

  bool success = false;
  switch (msg->target) {
    case ESP3DClientType::serial:
      success = esp3d_serial_service.dispatch(newMsg);
      break;
#if defined(ESP_SERIAL_BRIDGE_OUTPUT)
    case ESP3DClientType::serial_bridge:
      success = serial_bridge_service.dispatch(newMsg);
      break;
#endif  // ESP_SERIAL_BRIDGE_OUTPUT
#if defined(USB_SERIAL_FEATURE)
    case ESP3DClientType::usb_serial:
      success = esp3d_usb_serial_service.dispatch(newMsg);
      break;
#endif  // USB_SERIAL_FEATURE
#if defined(TELNET_FEATURE)
    case ESP3DClientType::telnet:
      success = telnet_server.dispatch(newMsg);
      break;
#endif  // TELNET_FEATURE
#if defined(WS_DATA_FEATURE)
    case ESP3DClientType::websocket:
      success = websocket_data_server.dispatch(newMsg);
      break;
#endif  // WS_DATA_FEATURE
#ifdef BLUETOOTH_FEATURE
    case ESP3DClientType::bluetooth:
      success = bt_service.dispatch(newMsg);
      break;
#endif  // BLUETOOTH_FEATURE
    default:
      esp3d_log_e("Unsupported client type for authentication error: %d", msg->target);
      delete[] newMsg->data;
      delete newMsg;
      return false;
  }
  if (!success) {
    esp3d_log_e("Failed to dispatch authentication error to client %d", msg->target);
    delete[] newMsg->data;
    delete newMsg;
    return false;
  }
  return true;
}

bool ESP3DCommands::dispatchIdValue(bool json, const char *Id, const char *value,
                                    ESP3DClientType target, ESP3DRequest requestId,
                                    bool isFirst) {
  if (!Id || !value) {
    esp3d_log_e("Invalid Id or value");
    return false;
  }
  String response;
  if (json) {
    response = isFirst ? "" : ",";
    response += "{\"id\":\"";
    response += Id;
    response += "\",\"value\":\"";
    response += value;
    response += "\"}";
  } else {
    response = Id;
    response += "=";
    response += value;
    response += "\n";
  }
  return dispatch(nullptr, response.c_str(), target, requestId,
                  isFirst ? ESP3DMessageType::head : ESP3DMessageType::core, ESP3DAuthenticationLevel::guest);
}

bool ESP3DCommands::dispatchKeyValue(bool json, const char *key, const char *value,
                                     ESP3DClientType target, ESP3DRequest requestId,
                                     bool nested, bool isFirst) {
  if (!key || !value) {
    esp3d_log_e("Invalid key or value");
    return false;
  }
  String response;
  if (json) {
    response = isFirst ? "" : ",";
    if (nested) response += "{";
    response += "\"";
    response += key;
    response += "\":\"";
    response += value;
    response += "\"";
    if (nested) response += "}";
  } else {
    response = key;
    response += "=";
    response += value;
    response += "\n";
  }
  return dispatch(nullptr, response.c_str(), target, requestId,
                  isFirst ? ESP3DMessageType::head : ESP3DMessageType::core, ESP3DAuthenticationLevel::guest);
}

bool ESP3DCommands::formatCommand(char *cmd, size_t len) {
  if (!cmd || len == 0) {
    esp3d_log_e("Invalid command or length");
    return false;
  }
  // Basic command formatting, assuming it should be an ESP command
  String command = cmd;
  command.trim();
  if (!command.startsWith("[ESP")) {
    esp3d_log_e("Command does not start with [ESP");
    return false;
  }
  // Ensure command ends properly
  if (!command.endsWith("]")) {
    if (command.length() + 1 < len) {
      command += "]";
      strncpy(cmd, command.c_str(), len);
      cmd[len - 1] = '\0';
    } else {
      esp3d_log_e("Command buffer too small");
      return false;
    }
  }
  return true;
}