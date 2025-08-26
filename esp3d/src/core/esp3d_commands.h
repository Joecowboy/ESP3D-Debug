/*
  esp3d_commands.h - ESP3D commands class header file

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

#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdarg.h>
#include <stdio.h>
#include "esp3d_log.h"
#include "esp3d_settings.h"
#include "esp3d_string.h"
#include "../modules/serial/serial_service.h"
#if defined(USB_SERIAL_FEATURE)
#include "../modules/usb-serial/usb_serial_service.h"
#endif  // USB_SERIAL_FEATURE
#if defined(ESP_SERIAL_BRIDGE_OUTPUT)
#include "../modules/serial/serial_bridge_service.h"
#endif  // ESP_SERIAL_BRIDGE_OUTPUT
#if defined(WIFI_FEATURE) || defined(ETH_FEATURE) || defined(BLUETOOTH_FEATURE)
#include "../modules/network/netconfig.h"
#endif  // WIFI_FEATURE || ETH_FEATURE || BLUETOOTH_FEATURE
#ifdef TIMESTAMP_FEATURE
#include "../modules/time/time_service.h"
#endif  // TIMESTAMP_FEATURE
#if defined(HTTP_FEATURE)
#include "../modules/http/http_server.h"
#endif  // HTTP_FEATURE
#if defined(TELNET_FEATURE)
#include "../modules/telnet/telnet_server.h"
#endif  // TELNET_FEATURE
#if defined(WS_DATA_FEATURE)
#include "../modules/websocket/websocket_server.h"
#endif  // WS_DATA_FEATURE
#if defined(WEBDAV_FEATURE)
#include "../modules/webdav/webdav_server.h"
#endif  // WEBDAV_FEATURE
#if defined(GCODE_HOST_FEATURE)
#include "../modules/gcode_host/gcode_host.h"
#endif  // GCODE_HOST_FEATURE
#ifdef ESP_LUA_INTERPRETER_FEATURE
#include "../modules/lua_interpreter/lua_interpreter_service.h"
#endif  // ESP_LUA_INTERPRETER_FEATURE
#if defined(SD_DEVICE)
#include "../modules/filesystem/esp_sd.h"
#endif  // SD_DEVICE
#if defined(DISPLAY_DEVICE)
#include "../modules/display/display.h"
#endif  // DISPLAY_DEVICE
#if defined(CAMERA_DEVICE)
#include "../modules/camera/camera.h"
#endif  // CAMERA_DEVICE
#if defined(BUZZER_DEVICE)
#include "../modules/buzzer/buzzer.h"
#endif  // BUZZER_DEVICE
#ifdef SENSOR_DEVICE
#include "../modules/sensor/sensor.h"
#endif  // SENSOR_DEVICE
#ifdef MDNS_FEATURE
#include "../modules/mDNS/mDNS.h"
#endif  // MDNS_FEATURE
#if defined(FILESYSTEM_FEATURE)
#include "../modules/filesystem/esp_filesystem.h"
#endif  // FILESYSTEM_FEATURE
#if defined(GLOBAL_FILESYSTEM_FEATURE)
#include "../modules/filesystem/esp_globalfs.h"
#endif  // GLOBAL_FILESYSTEM_FEATURE
#if defined(NOTIFICATION_FEATURE)
#include "../modules/notifications/notifications_service.h"
#endif  // NOTIFICATION_FEATURE
#ifdef DIRECT_PIN_FEATURE
#include "../modules/direct_pin/direct_pin.h"
#endif  // DIRECT_PIN_FEATURE

// Forward declaration for FtpServer
class FtpServer;

// Enum for network modes
enum ESP3DNetworkMode : uint8_t {
  no_network = 0,
  bluetooth = 1,
#if defined(WIFI_FEATURE)
  wifi_ap = 2,
  wifi_sta = 3,
  wifi_apsta = 4,
  ap_setup = 5,
#endif
#if defined(ETH_FEATURE)
  eth_sta = 6,
#endif
  ESP_BT = 7,
  ESP_NO_NETWORK = 8
};

class ESP3DCommands {
 public:
  ESP3DCommands();
  ~ESP3DCommands();
  bool begin();
  void handle();
  bool is_esp_command(uint8_t* sbuf, size_t len);
  const char* get_param(ESP3DMessage* msg, uint start, const char* label, bool* found = nullptr);
  const char* get_param(const char* data, uint size, uint start, const char* label, bool* found = nullptr);
  const char* get_clean_param(ESP3DMessage* msg, uint start);
  bool has_param(ESP3DMessage* msg, uint start);
  bool hasTag(ESP3DMessage* msg, uint start, const char* label);
  void process(ESP3DMessage* msg);
  bool dispatch(ESP3DMessage* msg, const char* sbuf);
  bool dispatch(ESP3DMessage* msg, uint8_t* sbuf, size_t len);
  bool dispatch(ESP3DMessage* msg);
  bool dispatch(uint8_t* sbuf, size_t size, ESP3DClientType target, ESP3DRequest requestId, ESP3DMessageType type,
                ESP3DClientType origin, ESP3DAuthenticationLevel authentication_level = ESP3DAuthenticationLevel::guest);
  bool dispatch(ESP3DMessage* msg, const char* sbuf, ESP3DClientType target, ESP3DRequest requestId,
                ESP3DMessageType type, ESP3DAuthenticationLevel authentication_level);
  bool dispatchIdValue(bool json, const char* Id, const char* value, ESP3DClientType target, ESP3DRequest requestId,
                       bool isFirst = false);
  bool dispatchKeyValue(bool json, const char* key, const char* value, ESP3DClientType target, ESP3DRequest requestId,
                        bool nested = false, bool isFirst = false);
  bool dispatchSetting(bool json, const char* filter, ESP3DSettingIndex index, const char* help,
                      const char** optionValues, const char** optionLabels, uint32_t maxsize, uint32_t minsize,
                      uint32_t minsize2, uint8_t precision, const char* unit, bool needRestart,
                      ESP3DClientType target, ESP3DRequest requestId, bool isFirst = false);
  bool dispatchAuthenticationError(ESP3DMessage* msg, uint cmdid, bool json);
  bool dispatchAnswer(ESP3DMessage* msg, uint cmdID, bool json, bool isOk, const char* answer);
  bool formatCommand(char* cmd, size_t len);
  const char* format_response(uint cmdID, bool isjson, bool isok, const char* message);
  ESP3DClientType getOutputClient(bool fromSettings = false);
  void execute_internal_command(int cmd, int cmd_params_pos, ESP3DMessage* msg);

  // Command-specific functions
  void ESP0(int cmd_params_pos, ESP3DMessage* msg);
#if defined(WIFI_FEATURE)
  void ESP100(int cmd_params_pos, ESP3DMessage* msg);
  void ESP101(int cmd_params_pos, ESP3DMessage* msg);
  void ESP102(int cmd_params_pos, ESP3DMessage* msg);
  void ESP103(int cmd_params_pos, ESP3DMessage* msg);
  void ESP104(int cmd_params_pos, ESP3DMessage* msg);
  void ESP105(int cmd_params_pos, ESP3DMessage* msg);
  void ESP106(int cmd_params_pos, ESP3DMessage* msg);
  void ESP107(int cmd_params_pos, ESP3DMessage* msg);
  void ESP108(int cmd_params_pos, ESP3DMessage* msg);
  void ESP410(int cmd_params_pos, ESP3DMessage* msg);
#endif  // WIFI_FEATURE
#if defined(WIFI_FEATURE) || defined(BLUETOOTH_FEATURE) || defined(ETH_FEATURE)
  void ESP110(int cmd_params_pos, ESP3DMessage* msg);
  void ESP112(int cmd_params_pos, ESP3DMessage* msg);
  void ESP114(int cmd_params_pos, ESP3DMessage* msg);
  void ESP115(int cmd_params_pos, ESP3DMessage* msg);
#endif  // WIFI_FEATURE || BLUETOOTH_FEATURE || ETH_FEATURE
#if defined(WIFI_FEATURE) || defined(ETH_FEATURE)
  void ESP111(int cmd_params_pos, ESP3DMessage* msg);
#endif  // WIFI_FEATURE || ETH_FEATURE
#if defined(ETH_FEATURE)
  void ESP116(int cmd_params_pos, ESP3DMessage* msg);
  void ESP117(int cmd_params_pos, ESP3DMessage* msg);
  void ESP118(int cmd_params_pos, ESP3DMessage* msg);
#endif  // ETH_FEATURE
#ifdef HTTP_FEATURE
  void ESP120(int cmd_params_pos, ESP3DMessage* msg);
  void ESP121(int cmd_params_pos, ESP3DMessage* msg);
#endif  // HTTP_FEATURE
#ifdef TELNET_FEATURE
  void ESP130(int cmd_params_pos, ESP3DMessage* msg);
  void ESP131(int cmd_params_pos, ESP3DMessage* msg);
#endif  // TELNET_FEATURE
#ifdef TIMESTAMP_FEATURE
  void ESP140(int cmd_params_pos, ESP3DMessage* msg);
#endif  // TIMESTAMP_FEATURE
  void ESP150(int cmd_params_pos, ESP3DMessage* msg);
#ifdef WS_DATA_FEATURE
  void ESP160(int cmd_params_pos, ESP3DMessage* msg);
  void ESP161(int cmd_params_pos, ESP3DMessage* msg);
#endif  // WS_DATA_FEATURE
#ifdef CAMERA_DEVICE
  void ESP170(int cmd_params_pos, ESP3DMessage* msg);
  void ESP171(int cmd_params_pos, ESP3DMessage* msg);
#endif  // CAMERA_DEVICE
#ifdef FTP_FEATURE
  void ESP180(int cmd_params_pos, ESP3DMessage* msg);
  void ESP181(int cmd_params_pos, ESP3DMessage* msg);
#endif  // FTP_FEATURE
#ifdef WEBDAV_FEATURE
  void ESP190(int cmd_params_pos, ESP3DMessage* msg);
  void ESP191(int cmd_params_pos, ESP3DMessage* msg);
#endif  // WEBDAV_FEATURE
#if defined(SD_DEVICE)
  void ESP200(int cmd_params_pos, ESP3DMessage* msg);
#if SD_DEVICE != ESP_SDIO
  void ESP202(int cmd_params_pos, ESP3DMessage* msg);
#endif  // SD_DEVICE != ESP_SDIO
#ifdef SD_UPDATE_FEATURE
  void ESP402(int cmd_params_pos, ESP3DMessage* msg);
#endif  // SD_UPDATE_FEATURE
#endif  // SD_DEVICE
#ifdef DIRECT_PIN_FEATURE
  void ESP201(int cmd_params_pos, ESP3DMessage* msg);
#endif  // DIRECT_PIN_FEATURE
#ifdef SENSOR_DEVICE
  void ESP210(int cmd_params_pos, ESP3DMessage* msg);
#endif  // SENSOR_DEVICE
#if defined(PRINTER_HAS_DISPLAY)
  void ESP212(int cmd_params_pos, ESP3DMessage* msg);
#endif  // PRINTER_HAS_DISPLAY
#if defined(DISPLAY_DEVICE)
  void ESP214(int cmd_params_pos, ESP3DMessage* msg);
#if defined(DISPLAY_TOUCH_DRIVER)
  void ESP215(int cmd_params_pos, ESP3DMessage* msg);
#endif  // DISPLAY_TOUCH_DRIVER
#endif  // DISPLAY_DEVICE
  void ESP220(int cmd_params_pos, ESP3DMessage* msg);
#ifdef BUZZER_DEVICE
  void ESP250(int cmd_params_pos, ESP3DMessage* msg);
#endif  // BUZZER_DEVICE
  void ESP290(int cmd_params_pos, ESP3DMessage* msg);
#ifdef ESP_LUA_INTERPRETER_FEATURE
  void ESP300(int cmd_params_pos, ESP3DMessage* msg);
  void ESP301(int cmd_params_pos, ESP3DMessage* msg);
#endif  // ESP_LUA_INTERPRETER_FEATURE
  void ESP400(int cmd_params_pos, ESP3DMessage* msg);
  void ESP401(int cmd_params_pos, ESP3DMessage* msg);
#if defined(ESP_SAVE_SETTINGS)
  void ESP420(int cmd_params_pos, ESP3DMessage* msg);
#endif  // ESP_SAVE_SETTINGS
  void ESP444(int cmd_params_pos, ESP3DMessage* msg);
#ifdef MDNS_FEATURE
  void ESP450(int cmd_params_pos, ESP3DMessage* msg);
#endif  // MDNS_FEATURE
#ifdef AUTHENTICATION_FEATURE
  void ESP500(int cmd_params_pos, ESP3DMessage* msg);
  void ESP510(int cmd_params_pos, ESP3DMessage* msg);
  void ESP550(int cmd_params_pos, ESP3DMessage* msg);
  void ESP555(int cmd_params_pos, ESP3DMessage* msg);
#endif  // AUTHENTICATION_FEATURE
#if defined(NOTIFICATION_FEATURE)
  void ESP600(int cmd_params_pos, ESP3DMessage* msg);
  void ESP610(int cmd_params_pos, ESP3DMessage* msg);
  void ESP620(int cmd_params_pos, ESP3DMessage* msg);
#endif  // NOTIFICATION_FEATURE
#if defined(GCODE_HOST_FEATURE)
  void ESP700(int cmd_params_pos, ESP3DMessage* msg);
  void ESP701(int cmd_params_pos, ESP3DMessage* msg);
#endif  // GCODE_HOST_FEATURE
#if defined(FILESYSTEM_FEATURE)
  void ESP710(int cmd_params_pos, ESP3DMessage* msg);
  void ESP720(int cmd_params_pos, ESP3DMessage* msg);
  void ESP730(int cmd_params_pos, ESP3DMessage* msg);
#endif  // FILESYSTEM_FEATURE
#if defined(SD_DEVICE)
  void ESP715(int cmd_params_pos, ESP3DMessage* msg);
  void ESP740(int cmd_params_pos, ESP3DMessage* msg);
  void ESP750(int cmd_params_pos, ESP3DMessage* msg);
#endif  // SD_DEVICE
#if defined(GLOBAL_FILESYSTEM_FEATURE)
  void ESP780(int cmd_params_pos, ESP3DMessage* msg);
  void ESP790(int cmd_params_pos, ESP3DMessage* msg);
#endif  // GLOBAL_FILESYSTEM_FEATURE
  void ESP800(int cmd_params_pos, ESP3DMessage* msg);
#if COMMUNICATION_PROTOCOL != SOCKET_SERIAL
  void ESP900(int cmd_params_pos, ESP3DMessage* msg);
  void ESP901(int cmd_params_pos, ESP3DMessage* msg);
#endif  // COMMUNICATION_PROTOCOL != SOCKET_SERIAL
#if defined(USB_SERIAL_FEATURE)
  void ESP902(int cmd_params_pos, ESP3DMessage* msg);
  void ESP950(int cmd_params_pos, ESP3DMessage* msg);
#endif  // USB_SERIAL_FEATURE
#if defined(ESP_SERIAL_BRIDGE_OUTPUT)
  void ESP930(int cmd_params_pos, ESP3DMessage* msg);
  void ESP931(int cmd_params_pos, ESP3DMessage* msg);
#endif  // ESP_SERIAL_BRIDGE_OUTPUT
#ifdef BUZZER_DEVICE
  void ESP910(int cmd_params_pos, ESP3DMessage* msg);
#endif  // BUZZER_DEVICE
#if defined(ARDUINO_ARCH_ESP32) && \
    (CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32C3)
  void ESP999(int cmd_params_pos, ESP3DMessage* msg);
#endif  // ARDUINO_ARCH_ESP32

 private:
  ESP3DClientType _output_client;
};

extern ESP3DCommands esp3d_commands;

#endif  // COMMANDS_H