/*
  settings_esp3d.h - settings esp3d functions class

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

#ifndef _ESP3D_SETTINGS_H
#define _ESP3D_SETTINGS_H

#include "../include/esp3d_defines.h"
#if defined(ESP_LOG_FEATURE)
#include "../core/esp3d_log.h"
#endif  // ESP_LOG_FEATURE
#if defined(ESP_SAVE_SETTINGS)
#include "esp3d_message.h"
#include "esp3d_string.h"

#if ESP_SAVE_SETTINGS == SETTINGS_IN_EEPROM
#include <EEPROM.h>
// EEPROM SIZE (Up to 4096)
#define EEPROM_SIZE 1024
#endif  // SETTINGS_IN_EEPROM

#if defined(WIFI_FEATURE) || defined(ETH_FEATURE)
#include "../modules/network/netconfig.h"
#if defined(WIFI_FEATURE)
#include "../modules/wifi/wificonfig.h"
#endif  // WIFI_FEATURE
#endif  // WIFI_FEATURE || ETH_FEATURE
#ifdef SENSOR_DEVICE
#include "../modules/sensor/sensor.h"
#endif  // SENSOR_DEVICE
#ifdef BUZZER_DEVICE
#include "../modules/buzzer/buzzer.h"
#endif  // BUZZER_DEVICE
#if defined(OTA_FEATURE) || defined(WEB_UPDATE_FEATURE)
#include "../modules/update/update_service.h"
#endif  // OTA_FEATURE || WEB_UPDATE_FEATURE
#if defined(DISPLAY_DEVICE)
#include "../modules/display/display.h"
#endif  // DISPLAY_DEVICE
#if defined(AUTHENTICATION_FEATURE)
#include "../modules/authentication/authentication_service.h"
#endif  // AUTHENTICATION_FEATURE
#if defined(SD_DEVICE)
#include "../modules/filesystem/esp_sd.h"
#endif  // SD_DEVICE
#if defined(GCODE_HOST_FEATURE)
#include "../modules/gcode_host/gcode_host.h"
#endif  // GCODE_HOST_FEATURE
#if defined(TIMESTAMP_FEATURE)
#include "../modules/time/time_service.h"
#endif  // TIMESTAMP_FEATURE
#if defined(NOTIFICATION_FEATURE)
#include "../modules/notifications/notifications_service.h"
#endif  // NOTIFICATION_FEATURE
#include "../modules/serial/serial_service.h"
#if defined(USB_SERIAL_FEATURE)
#include "../modules/usb-serial/usb_serial_service.h"
#endif  // USB_SERIAL_FEATURE
#if defined(DIRECT_PIN_FEATURE)
#include "../modules/direct_pin/direct_pin.h"
#endif  // DIRECT_PIN_FEATURE
#if defined(ESP_LUA_INTERPRETER_FEATURE)
#include "../modules/lua_interpreter/lua_interpreter_service.h"
#endif  // ESP_LUA_INTERPRETER_FEATURE

// Include esp3d_commands.h for ESP3DNetworkMode
#include "esp3d_commands.h"

// Current Settings Version
#define CURRENT_SETTINGS_VERSION "ESP3D05"

// boundaries
#define MAX_SENSOR_INTERVAL 60000
#define MIN_SENSOR_INTERVAL 0
#define MAX_LOCAL_PASSWORD_LENGTH 20
#define MIN_LOCAL_PASSWORD_LENGTH 1
#define MAX_VERSION_LENGTH 7
#define MAX_BOOT_DELAY 40000
#define MIN_BOOT_DELAY 0
#define MAX_NOTIFICATION_TOKEN_LENGTH 250
#define MAX_NOTIFICATION_SETTINGS_LENGTH 128
#define MAX_SERVER_ADDRESS_LENGTH 128
#define MAX_TIME_ZONE_LENGTH 6
#define MIN_SERVER_ADDRESS_LENGTH 0
#define MIN_SSID_LENGTH 1
#define MIN_PASSWORD_LENGTH 8

// default byte values
#define DEFAULT_ETH_STA_FALLBACK_MODE 0
#define DEFAULT_STA_FALLBACK_MODE 0
#define DEFAULT_ESP_RADIO_MODE 0
#define DEFAULT_OUTPUT_CLIENT 0
#define DEFAULT_BUZZER_STATE 1
#define DEFAULT_INTERNET_TIME 1
#define DEFAULT_SETUP 0
#define DEFAULT_VERBOSE_BOOT 0
#define DEFAULT_STA_IP_MODE 1  // DHCP
#define DEFAULT_AP_CHANNEL 1
#define DEFAULT_OUTPUT_FLAG 0
#define DEFAULT_SD_SPI_DIV 2
#define DEFAULT_TIME_ZONE 0
#define DEFAULT_TIME_DST 0
#define DEFAULT_SD_MOUNT 0
#define DEFAULT_SD_CHECK_UPDATE_AT_BOOT 0
#define DEFAULT_SENSOR_TYPE 0
#define DEFAULT_HTTP_ON 1
#define DEFAULT_FTP_ON 0
#define DEFAULT_SERIAL_BRIDGE_ON 0
#define DEFAULT_WEBDAV_ON 0
#define DEFAULT_TELNET_ON 0
#define DEFAULT_WEBSOCKET_ON 0
#define DEFAULT_NOTIFICATION_TYPE 0
#define DEFAULT_AUTO_NOTIFICATION_STATE 0
#define DEFAULT_SECURE_SERIAL 0
#define DEFAULT_BOOT_RADIO_STATE 0
#define DEFAULT_PLUGIN_DIRECT_PIN_ENABLED 0

// default int values
#define DEFAULT_ESP_INT 0
#define DEFAULT_BAUD_RATE 115200
#define DEFAULT_SERIAL_BRIDGE_BAUD_RATE 115200
#define DEFAULT_HTTP_PORT 80
#define DEFAULT_FTP_CTRL_PORT 21
#define DEFAULT_FTP_ACTIVE_PORT 20
#define DEFAULT_FTP_PASSIVE_PORT 55600
#define DEFAULT_WEBSOCKET_PORT 81
#define DEFAULT_WEBDAV_PORT 80
#define DEFAULT_TELNET_PORT 23
#define DEFAULT_SENSOR_INTERVAL 1000
#define DEFAULT_BOOT_DELAY 1000
#define DEFAULT_CALIBRATION_VALUE 0
#define DEFAULT_CALIBRATION_DONE 0
#define DEFAULT_SESSION_TIMEOUT 60

// default string values
#define DEFAULT_AP_SSID_VALUE "ESP3D"
#define DEFAULT_AP_PASSWORD_VALUE "12345678"
#define DEFAULT_STA_SSID_VALUE "NETWORK_SSID"
#define DEFAULT_STA_PASSWORD_VALUE "12345678"
#define DEFAULT_HOSTNAME_VALUE "esp3d"
#define DEFAULT_ADMIN_PWD_VALUE "admin"
#define DEFAULT_USER_PWD_VALUE "user"
#define DEFAULT_TIME_SERVER1_VALUE "time.windows.com"
#define DEFAULT_TIME_SERVER2_VALUE "time.google.com"
#define DEFAULT_TIME_SERVER3_VALUE "0.pool.ntp.org"
#define DEFAULT_SETTINGS_VERSION_VALUE "ESP3D31"
#define DEFAULT_STA_IP_VALUE "192.168.0.254"
#define DEFAULT_STA_GATEWAY_VALUE "192.168.0.254"
#define DEFAULT_STA_MASK_VALUE "255.255.255.0"
#define DEFAULT_STA_DNS_VALUE "192.168.0.254"
#define DEFAULT_AP_IP_VALUE "192.168.0.1"
#define DEFAULT_AP_GATEWAY_VALUE "192.168.0.1"
#define DEFAULT_AP_MASK_VALUE "255.255.255.0"
#define DEFAULT_AP_DNS_VALUE "192.168.0.1"
#define DEFAULT_NOTIFICATION_TOKEN1_VALUE ""
#define DEFAULT_NOTIFICATION_TOKEN2_VALUE ""
#define DEFAULT_NOTIFICATION_SETTINGS_VALUE ""

enum class ESP3DSettingType : uint8_t {
  byte_t = 0,
  integer_t = 1,
  string_t = 2,
  ip_t = 3
};

struct ESP3DSettingDescription {
  ESP3DSettingIndex index;
  ESP3DSettingType type;
  size_t size;
  const char *default_val;
};

enum ESP3DState : uint8_t {
  off = 0,
  on = 1
};

enum ESP3DNotificationsType : uint8_t {
  none = 0,
  pushover = 1,
  email = 2,
  line = 3,
  telegram = 4,
  max_notifications = 4
};

enum ESP3DSensorType : uint8_t {
  none_sensor = 0,
  dht11 = 1,
  dht22 = 2,
  bmp280 = 3,
  bme280 = 4,
  max_sensor = 4
};

class ESP3DSettings {
public:
  static bool begin();
  static bool isVerboseBoot(bool fromsettings = false);
  static uint8_t GetFirmwareTarget(bool fromsettings = false);
  static uint8_t GetSDDevice();
  static String GetFirmwareTargetShortName();
  static String TargetBoard();
  static int8_t GetSettingsVersion();
  static bool reset(bool networkonly = false);
  static uint8_t readByte(int pos, bool *haserror = NULL);
  static bool writeByte(int pos, const uint8_t value);
  static uint32_t readUint32(int pos, bool *haserror = NULL);
  static bool writeUint32(int pos, const uint32_t value);
  static String readIPString(int pos, bool *haserror = NULL);
  static bool writeIPString(int pos, const char *value);
  static String readString(int pos, bool *haserror = NULL);
  static bool writeString(int pos, const char *byte_buffer);
  static uint32_t read_IP(int pos, bool *haserror = NULL);
  static bool is_string(const char *s, uint len);
  static bool isValidIPStringSetting(const char *value, ESP3DSettingIndex settingElement);
  static bool isValidStringSetting(const char *value, ESP3DSettingIndex settingElement);
  static bool isValidIntegerSetting(uint32_t value, ESP3DSettingIndex settingElement);
  static bool isValidByteSetting(uint8_t value, ESP3DSettingIndex settingElement);
  static uint32_t getDefaultIntegerSetting(ESP3DSettingIndex settingElement);
  static String getDefaultStringSetting(ESP3DSettingIndex settingElement);
  static uint8_t getDefaultByteSetting(ESP3DSettingIndex settingElement);
  static const ESP3DSettingDescription *getSettingPtr(const ESP3DSettingIndex index);

private:
  static String _IpToString(uint32_t ip_int);
  static uint32_t _stringToIP(const char *s);
  static uint8_t _FirmwareTarget;
  static bool _isverboseboot;
};

extern uint16_t ESP3DSettingsData[];

#if defined(WIFI_FEATURE)
extern const uint8_t SupportedApChannels[];
extern const uint8_t SupportedApChannelsSize;
#endif  // WIFI_FEATURE
#if defined(SD_DEVICE)
extern const uint8_t SupportedSPIDivider[];
extern const uint8_t SupportedSPIDividerSize;
#endif  // SD_DEVICE
extern const uint32_t SupportedBaudList[];
extern const uint8_t SupportedBaudListSize;

#endif  // ESP_SAVE_SETTINGS
#endif  // _ESP3D_SETTINGS_H