/*
  esp3d_settings.cpp - settings esp3d functions class

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

#include "esp3d_settings.h"

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
#ifdef ETH_FEATURE
#define DEFAULT_ETH_STA_FALLBACK_MODE STRING(ESP_NO_NETWORK)
#else
#define DEFAULT_ETH_STA_FALLBACK_MODE STRING(ESP_NO_NETWORK)
#endif  // ETH_FEATURE
#ifdef WIFI_FEATURE
#define DEFAULT_STA_FALLBACK_MODE STRING(ESP_AP_SETUP)
#else
#define DEFAULT_STA_FALLBACK_MODE STRING(ESP_NO_NETWORK)
#endif  // WIFI_FEATURE
#ifdef BLUETOOTH_FEATURE
#define DEFAULT_ESP_RADIO_MODE STRING(ESP_BT)
#else
#ifdef WIFI_FEATURE
#define DEFAULT_ESP_RADIO_MODE STRING(ESP_WIFI_STA)
#else
#ifdef ETH_FEATURE
#define DEFAULT_ESP_RADIO_MODE STRING(ESP_ETH_STA)
#else
#define DEFAULT_ESP_RADIO_MODE STRING(ESP_NO_NETWORK)
#endif  // ETH_FEATURE
#endif  // WIFI_FEATURE
#endif  // BLUETOOTH_FEATURE
#if COMMUNICATION_PROTOCOL == RAW_SERIAL
#define DEFAULT_OUTPUT_CLIENT STRING(ESP3DClientType::serial)
#endif  // COMMUNICATION_PROTOCOL == RAW_SERIAL
#if COMMUNICATION_PROTOCOL == MKS_SERIAL
#define DEFAULT_OUTPUT_CLIENT STRING(ESP3DClientType::mks_serial)
#endif  // COMMUNICATION_PROTOCOL == MKS_SERIAL
#if COMMUNICATION_PROTOCOL == SOCKET_SERIAL
#define DEFAULT_OUTPUT_CLIENT STRING(ESP3DClientType::socket_serial)
#endif  // COMMUNICATION_PROTOCOL == SOCKET_SERIAL
#define DEFAULT_BUZZER_STATE "0"
#define DEFAULT_INTERNET_TIME "0"
#define DEFAULT_SETUP "0"
#define DEFAULT_VERBOSE_BOOT "0"
#define DEFAULT_STA_IP_MODE "1"
#define DEFAULT_AP_CHANNEL "11"
#define DEFAULT_OUTPUT_FLAG "255"
#define DEFAULT_SD_SPI_DIV "4"
#ifndef DEFAULT_FW
#define DEFAULT_FW "0"
#endif  // DEFAULT_FW
#define DEFAULT_TIME_ZONE "+00:00"
#define DEFAULT_TIME_DST "0"
#define DEFAULT_SD_MOUNT "0"
#define DEFAULT_SD_CHECK_UPDATE_AT_BOOT "0"
#define DEFAULT_SENSOR_TYPE "0"
#define DEFAULT_HTTP_ON "1"
#define DEFAULT_FTP_ON "0"
#define DEFAULT_SERIAL_BRIDGE_ON "0"
#define DEFAULT_WEBDAV_ON "0"
#define DEFAULT_TELNET_ON "0"
#define DEFAULT_WEBSOCKET_ON "0"
#define DEFAULT_NOTIFICATION_TYPE "0"
#define DEFAULT_NOTIFICATION_TOKEN1 ""
#define DEFAULT_NOTIFICATION_TOKEN2 ""
#define DEFAULT_NOTIFICATION_SETTINGS ""
#define DEFAULT_AUTO_NOTIFICATION_STATE "0"
#define DEFAULT_SECURE_SERIAL "0"
#define DEFAULT_BOOT_RADIO_STATE "1"
#define DEFAULT_PLUGIN_DIRECT_PIN_ENABLED "1"

// default int values
#define DEFAULT_ESP_INT "0"
#define DEFAULT_BAUD_RATE "115200"
#define DEFAULT_SERIAL_BRIDGE_BAUD_RATE "115200"
#define DEFAULT_HTTP_PORT "80"
#define DEFAULT_FTP_CTRL_PORT "21"
#define DEFAULT_FTP_ACTIVE_PORT "20"
#define DEFAULT_FTP_PASSIVE_PORT "55600"
#define DEFAULT_WEBSOCKET_PORT "8282"
#define DEFAULT_WEBDAV_PORT "8181"
#define DEFAULT_TELNET_PORT "23"
#define DEFAULT_SENSOR_INTERVAL "30000"
#define DEFAULT_BOOT_DELAY "5000"
#define DEFAULT_CALIBRATION_VALUE "0"
#define DEFAULT_CALIBRATION_DONE "0"
#define DEFAULT_SESSION_TIMEOUT "3"

// default string values
#define DEFAULT_AP_SSID "ESP3D"
#define DEFAULT_AP_PASSWORD "12345678"
#define DEFAULT_STA_SSID "NETWORK_SSID"
#define DEFAULT_STA_PASSWORD "12345678"
#define DEFAULT_HOSTNAME "esp3d"
#define DEFAULT_ADMIN_PWD "admin"
#define DEFAULT_USER_PWD "user"
#define DEFAULT_TIME_SERVER1 "time.windows.com"
#define DEFAULT_TIME_SERVER2 "time.google.com"
#define DEFAULT_TIME_SERVER3 "0.pool.ntp.org"
#define DEFAULT_SETTINGS_VERSION "ESP3D31"

// default IP values
#define DEFAULT_STA_IP_VALUE "192.168.0.254"
#define DEFAULT_STA_GATEWAY_VALUE DEFAULT_STA_IP_VALUE
#define DEFAULT_STA_MASK_VALUE "255.255.255.0"
#define DEFAULT_STA_DNS_VALUE DEFAULT_STA_IP_VALUE
#define DEFAULT_AP_IP_VALUE "192.168.0.1"
#define DEFAULT_AP_GATEWAY_VALUE DEFAULT_AP_IP_VALUE
#define DEFAULT_AP_MASK_VALUE "255.255.255.0"
#define DEFAULT_AP_DNS_VALUE DEFAULT_AP_IP_VALUE

uint16_t ESP3DSettingsData[] = {
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_plugin_direct_pin_enabled),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_radio_mode),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_sta_ssid),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_sta_password),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_boot_radio_state),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_sta_fallback_mode),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_ap_ssid),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_ap_password),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_sta_ip_value),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_sta_gateway_value),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_sta_mask_value),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_sta_dns_value),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_ap_ip_value),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_sta_ip_mode),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_settings_version),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_ap_channel),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_http_on),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_ftp_on),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_telnet_port),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_websocket_on),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_verbose_boot),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_hostname),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_baud_rate),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_http_port),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_ftp_ctrl_port),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_websocket_port),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_webdav_port),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_boot_delay),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_notification_type),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_notification_token1),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_notification_token2),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_notification_settings),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_auto_notification),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_time_server1),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_time_server2),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_time_server3),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_time_zone),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_sensor_type),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_sensor_interval),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_output_client),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_admin_pwd),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_user_pwd),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_session_timeout),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_secure_serial),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_sd_mount),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_sd_check_update_at_boot),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_sd_speed_div),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_serial_bridge_on),
    static_cast<uint16_t>(ESP3DSettingIndex::esp3d_serial_bridge_baud)
};

#if defined(WIFI_FEATURE)
const uint8_t SupportedApChannels[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
const uint8_t SupportedApChannelsSize = sizeof(SupportedApChannels) / sizeof(uint8_t);
#endif  // WIFI_FEATURE
#if defined(SD_DEVICE)
const uint8_t SupportedSPIDivider[] = {2, 4, 6, 8, 10};
const uint8_t SupportedSPIDividerSize = sizeof(SupportedSPIDivider) / sizeof(uint8_t);
#endif  // SD_DEVICE
const uint32_t SupportedBaudList[] = {9600, 19200, 38400, 57600, 74880, 115200, 230400, 250000, 500000, 921600};
const uint8_t SupportedBaudListSize = sizeof(SupportedBaudList) / sizeof(uint32_t);

uint8_t ESP3DSettings::_FirmwareTarget = 0;
bool ESP3DSettings::_isverboseboot = false;

bool ICACHE_FLASH_ATTR ESP3DSettings::begin() {
  esp3d_log("Initializing ESP3D settings");
  esp3d_log("Free heap before settings init: %u", ESP.getFreeHeap());
  if (GetSettingsVersion() == -1) {
    esp3d_log_e("Failed to get settings version");
    return false;
  }
  ESP3DSettings::GetFirmwareTarget(true);
  ESP3DSettings::isVerboseBoot(true);
  esp3d_log("Settings initialized, firmware target: %d, verbose boot: %d, free heap: %u",
            _FirmwareTarget, _isverboseboot, ESP.getFreeHeap());
  return true;
}

bool ICACHE_FLASH_ATTR ESP3DSettings::isVerboseBoot(bool fromsettings) {
#if COMMUNICATION_PROTOCOL != MKS_SERIAL
  if (fromsettings) {
    _isverboseboot = readByte(static_cast<int>(ESP3DSettingIndex::esp3d_verbose_boot));
    esp3d_log("Verbose boot set to %d", _isverboseboot);
  }
#else
  _isverboseboot = false;
  esp3d_log("Verbose boot disabled for MKS_SERIAL");
#endif
  return _isverboseboot;
}

uint8_t ICACHE_FLASH_ATTR ESP3DSettings::GetFirmwareTarget(bool fromsettings) {
#if defined(FIXED_FW_TARGET)
  (void)fromsettings;
  _FirmwareTarget = FIXED_FW_TARGET;
#else
  if (fromsettings) {
    _FirmwareTarget = readByte(static_cast<int>(ESP3DSettingIndex::esp3d_target_fw));
    esp3d_log("Firmware target read: %d", _FirmwareTarget);
  }
#endif
  return _FirmwareTarget;
}

uint8_t ICACHE_FLASH_ATTR ESP3DSettings::GetSDDevice() {
  esp3d_log("No SD device");
  return ESP_NO_SD;
}

String ICACHE_FLASH_ATTR ESP3DSettings::GetFirmwareTargetShortName() {
  String response;
  esp3d_log("Getting firmware target name: %d", _FirmwareTarget);
  switch (_FirmwareTarget) {
    case REPETIER: response = "repetier"; break;
    case MARLIN: case MARLIN_EMBEDDED: response = "marlin"; break;
    case KLIPPER: response = "klipper"; break;
    case SMOOTHIEWARE: response = "smoothieware"; break;
    case GRBL: response = "grbl"; break;
    default: response = "unknown"; break;
  }
  esp3d_log("Firmware target name: %s", response.c_str());
  return response;
}

String ICACHE_FLASH_ATTR ESP3DSettings::TargetBoard() {
#if defined(ARDUINO_ARCH_ESP32)
  return "esp32";
#elif defined(ARDUINO_ARCH_ESP8266)
  return "esp8266";
#else
  return "unknown";
#endif
}

int8_t ICACHE_FLASH_ATTR ESP3DSettings::GetSettingsVersion() {
  bool haserror = true;
  String version = readString(static_cast<int>(ESP3DSettingIndex::esp3d_settings_version), &haserror);
  if (haserror || version != CURRENT_SETTINGS_VERSION) {
    esp3d_log_e("Invalid settings version: %s, expected: %s", version.c_str(), CURRENT_SETTINGS_VERSION);
    return -1;
  }
  esp3d_log("Settings version: %s", version.c_str());
  return 0;
}

bool ICACHE_FLASH_ATTR ESP3DSettings::reset(bool networkonly) {
  esp3d_log("Resetting settings, networkonly: %d", networkonly);
#if ESP_SAVE_SETTINGS == SETTINGS_IN_EEPROM
  EEPROM.begin(EEPROM_SIZE);
  for (uint i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0xFF);
  }
  if (!EEPROM.commit()) {
    esp3d_log_e("Failed to reset EEPROM");
    EEPROM.end();
    return false;
  }
  EEPROM.end();
#endif
#if ESP_SAVE_SETTINGS == SETTINGS_IN_PREFERENCES
  Preferences prefs;
  if (!prefs.begin(NAMESPACE, false)) {
    esp3d_log_e("Error opening preferences namespace %s", NAMESPACE);
    return false;
  }
  if (!prefs.clear()) {
    esp3d_log_e("Failed to clear preferences");
    prefs.end();
    return false;
  }
  prefs.end();
#endif
  esp3d_log("Settings reset complete");
  return true;
}

uint8_t ICACHE_FLASH_ATTR ESP3DSettings::readByte(int pos, bool *haserror) {
  if (haserror) *haserror = true;
  esp3d_log("Reading byte at position %d", pos);
  uint8_t value = getDefaultByteSetting(static_cast<ESP3DSettingIndex>(pos));
#if ESP_SAVE_SETTINGS == SETTINGS_IN_EEPROM
  if (pos + 1 > EEPROM_SIZE) {
    esp3d_log_e("Error: position %d exceeds EEPROM size %d", pos, EEPROM_SIZE);
    return value;
  }
  EEPROM.begin(EEPROM_SIZE);
  value = EEPROM.read(pos);
  EEPROM.end();
  esp3d_log("Read byte value: %d", value);
#endif
#if ESP_SAVE_SETTINGS == SETTINGS_IN_PREFERENCES
  Preferences prefs;
  if (!prefs.begin(NAMESPACE, true)) {
    esp3d_log_e("Error opening preferences namespace %s", NAMESPACE);
    return value;
  }
  char p[16];
  snprintf(p, sizeof(p), "P_%d", pos);
  if (prefs.isKey(p)) {
    value = prefs.getChar(p, getDefaultByteSetting(static_cast<ESP3DSettingIndex>(pos)));
    esp3d_log("Read byte from preferences %s: %d", p, value);
  }
  prefs.end();
#endif
  if (haserror) *haserror = false;
  return value;
}

bool ICACHE_FLASH_ATTR ESP3DSettings::writeByte(int pos, const uint8_t value) {
  esp3d_log("Writing byte %d to position %d", value, pos);
#if ESP_SAVE_SETTINGS == SETTINGS_IN_EEPROM
  if (pos + 1 > EEPROM_SIZE) {
    esp3d_log_e("Error: position %d exceeds EEPROM size %d", pos, EEPROM_SIZE);
    return false;
  }
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(pos, value);
  if (!EEPROM.commit()) {
    esp3d_log_e("Error committing byte to EEPROM at position %d", pos);
    return false;
  }
  EEPROM.end();
#endif
#if ESP_SAVE_SETTINGS == SETTINGS_IN_PREFERENCES
  Preferences prefs;
  if (!prefs.begin(NAMESPACE, false)) {
    esp3d_log_e("Error opening preferences namespace %s", NAMESPACE);
    return false;
  }
  char p[16];
  snprintf(p, sizeof(p), "P_%d", pos);
  uint8_t r = prefs.putChar(p, value);
  prefs.end();
  if (r == 0) {
    esp3d_log_e("Error committing byte to preferences %s", p);
    return false;
  }
#endif
  return true;
}

uint32_t ICACHE_FLASH_ATTR ESP3DSettings::readUint32(int pos, bool *haserror) {
  if (haserror) *haserror = true;
  esp3d_log("Reading uint32 at position %d", pos);
  uint32_t res = getDefaultIntegerSetting(static_cast<ESP3DSettingIndex>(pos));
#if ESP_SAVE_SETTINGS == SETTINGS_IN_EEPROM
  uint8_t size_buffer = sizeof(uint32_t);
  if (pos + size_buffer > EEPROM_SIZE) {
    esp3d_log_e("Error: position %d exceeds EEPROM size %d", pos, EEPROM_SIZE);
    return res;
  }
  EEPROM.begin(EEPROM_SIZE);
  for (uint8_t i = 0; i < size_buffer; i++) {
    ((uint8_t *)(&res))[i] = EEPROM.read(pos + i);
  }
  EEPROM.end();
#endif
#if ESP_SAVE_SETTINGS == SETTINGS_IN_PREFERENCES
  Preferences prefs;
  if (!prefs.begin(NAMESPACE, true)) {
    esp3d_log_e("Error opening preferences namespace %s", NAMESPACE);
    return res;
  }
  char p[16];
  snprintf(p, sizeof(p), "P_%d", pos);
  if (prefs.isKey(p)) {
    res = prefs.getUInt(p, res);
  }
  prefs.end();
#endif
  if (haserror) *haserror = false;
  return res;
}

bool ICACHE_FLASH_ATTR ESP3DSettings::writeUint32(int pos, const uint32_t value) {
  esp3d_log("Writing uint32 %u to position %d", value, pos);
#if ESP_SAVE_SETTINGS == SETTINGS_IN_EEPROM
  uint8_t size_buffer = sizeof(uint32_t);
  if (pos + size_buffer > EEPROM_SIZE) {
    esp3d_log_e("Error: position %d exceeds EEPROM size %d", pos, EEPROM_SIZE);
    return false;
  }
  EEPROM.begin(EEPROM_SIZE);
  for (uint8_t i = 0; i < size_buffer; i++) {
    EEPROM.write(pos + i, ((uint8_t *)(&value))[i]);
  }
  if (!EEPROM.commit()) {
    esp3d_log_e("Error committing uint32 to EEPROM at position %d", pos);
    return false;
  }
  EEPROM.end();
#endif
#if ESP_SAVE_SETTINGS == SETTINGS_IN_PREFERENCES
  Preferences prefs;
  if (!prefs.begin(NAMESPACE, false)) {
    esp3d_log_e("Error opening preferences namespace %s", NAMESPACE);
    return false;
  }
  char p[16];
  snprintf(p, sizeof(p), "P_%d", pos);
  uint8_t r = prefs.putUInt(p, value);
  prefs.end();
  if (r == 0) {
    esp3d_log_e("Error committing uint32 to preferences %s", p);
    return false;
  }
#endif
  return true;
}

String ICACHE_FLASH_ATTR ESP3DSettings::readIPString(int pos, bool *haserror) {
  uint32_t ip = readUint32(pos, haserror);
  return _IpToString(ip);
}

bool ICACHE_FLASH_ATTR ESP3DSettings::writeIPString(int pos, const char *value) {
  esp3d_log("Writing IP string '%s' to position %d", value, pos);
  return writeUint32(pos, _stringToIP(value));
}

String ICACHE_FLASH_ATTR ESP3DSettings::readString(int pos, bool *haserror) {
  if (haserror) *haserror = true;
  const ESP3DSettingDescription *query = getSettingPtr(static_cast<ESP3DSettingIndex>(pos));
  esp3d_log("Reading string at position %d", pos);
  if (!query) {
    esp3d_log_e("Unknown setting entry %d", pos);
    return "";
  }
  size_t size_max = query->size;
#if ESP_SAVE_SETTINGS == SETTINGS_IN_EEPROM
  String res;
  size_max = min(size_max + 1, size_t(128));
  if (pos + size_max > EEPROM_SIZE) {
    esp3d_log_e("Error: position %d exceeds EEPROM size %d", pos, EEPROM_SIZE);
    return "";
  }
  EEPROM.begin(EEPROM_SIZE);
  size_t i = 0;
  uint8_t b = 1;
  while (i < size_max - 1 && b != 0) {
    b = EEPROM.read(pos + i);
    if (esp3d_string::isPrintableChar(b)) res += (char)b;
    i++;
  }
  EEPROM.end();
  esp3d_log("Read string: %s", res.c_str());
#endif
#if ESP_SAVE_SETTINGS == SETTINGS_IN_PREFERENCES
  String res;
  Preferences prefs;
  if (!prefs.begin(NAMESPACE, true)) {
    esp3d_log_e("Error opening preferences namespace %s", NAMESPACE);
    return "";
  }
  char p[16];
  snprintf(p, sizeof(p), "P_%d", pos);
  if (prefs.isKey(p)) {
    char res_buf[128];
    size_t len = prefs.getString(p, res_buf, sizeof(res_buf));
    res_buf[len] = 0;
    res = res_buf;
    esp3d_log("Read string from preferences %s: %s", p, res.c_str());
  } else {
    res = getDefaultStringSetting(static_cast<ESP3DSettingIndex>(pos));
  }
  prefs.end();
  if (res.length() > size_max) {
    res = res.substring(0, size_max);
  }
#endif
  if (haserror) *haserror = false;
  return res;
}

bool ICACHE_FLASH_ATTR ESP3DSettings::writeString(int pos, const char *byte_buffer) {
  size_t size_buffer = strlen(byte_buffer);
  const ESP3DSettingDescription *query = getSettingPtr(static_cast<ESP3DSettingIndex>(pos));
  esp3d_log("Writing string '%s' to position %d", byte_buffer, pos);
  if (!query || size_buffer > query->size) {
    esp3d_log_e("Invalid setting or string too long for pos %d: %zu vs max %zu", pos, size_buffer, query ? query->size : 0);
    return false;
  }
#if ESP_SAVE_SETTINGS == SETTINGS_IN_EEPROM
  if (pos + size_buffer + 1 > EEPROM_SIZE) {
    esp3d_log_e("Invalid parameters for string write at pos %d", pos);
    return false;
  }
  EEPROM.begin(EEPROM_SIZE);
  for (size_t i = 0; i < size_buffer; i++) {
    EEPROM.write(pos + i, byte_buffer[i]);
  }
  EEPROM.write(pos + size_buffer, 0);
  if (!EEPROM.commit()) {
    esp3d_log_e("Error committing string to EEPROM at position %d", pos);
    return false;
  }
  EEPROM.end();
#endif
#if ESP_SAVE_SETTINGS == SETTINGS_IN_PREFERENCES
  Preferences prefs;
  if (!prefs.begin(NAMESPACE, false)) {
    esp3d_log_e("Error opening preferences namespace %s", NAMESPACE);
    return false;
  }
  char p[16];
  snprintf(p, sizeof(p), "P_%d", pos);
  uint8_t r = prefs.putString(p, byte_buffer);
  prefs.end();
  if (r != size_buffer) {
    esp3d_log_e("Error committing string to preferences %s", p);
    return false;
  }
#endif
  return true;
}

uint32_t ICACHE_FLASH_ATTR ESP3DSettings::read_IP(int pos, bool *haserror) {
  esp3d_log("Reading IP at position %d", pos);
  return readUint32(pos, haserror);
}

bool ICACHE_FLASH_ATTR ESP3DSettings::is_string(const char *s, uint len) {
  for (uint p = 0; p < len; p++) {
    if (!esp3d_string::isPrintableChar(s[p])) {
      esp3d_log_e("Non-printable character at position %u: %c", p, s[p]);
      return false;
    }
  }
  return true;
}

String ICACHE_FLASH_ATTR ESP3DSettings::_IpToString(uint32_t ip_int) {
  char ip_str[16];
  snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d",
           (ip_int >> 24) & 0xFF, (ip_int >> 16) & 0xFF,
           (ip_int >> 8) & 0xFF, ip_int & 0xFF);
  return String(ip_str);
}

uint32_t ICACHE_FLASH_ATTR ESP3DSettings::_stringToIP(const char *s) {
  esp3d_log("Converting string to IP: %s", s);
  uint32_t ip_int = 0;
  uint8_t parts[4] = {0};
  uint8_t part = 0;
  char buf[32];
  strncpy(buf, s, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = 0;
  char *token = strtok(buf, ".");
  while (token && part < 4) {
    parts[part++] = atoi(token);
    token = strtok(NULL, ".");
  }
  if (part == 4) {
    ip_int = (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3];
  }
  return ip_int;
}

bool ICACHE_FLASH_ATTR ESP3DSettings::isValidIPStringSetting(const char *value, ESP3DSettingIndex settingElement) {
  esp3d_log("Validating IP string '%s' for setting %d", value, settingElement);
  const ESP3DSettingDescription *settingPtr = getSettingPtr(settingElement);
  if (!settingPtr || settingPtr->type != ESP3DSettingType::ip_t) {
    esp3d_log_e("Invalid setting or type for %d", settingElement);
    return false;
  }
  char buf[32];
  strncpy(buf, value, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = 0;
  uint8_t parts[4] = {0};
  uint8_t part = 0;
  char *token = strtok(buf, ".");
  while (token && part < 4) {
    int v = atoi(token);
    if (v < 0 || v > 255 || strlen(token) > 3) {
      esp3d_log_e("Invalid IP part %d: %s", part, token);
      return false;
    }
    parts[part++] = v;
    token = strtok(NULL, ".");
  }
  if (part != 4) {
    esp3d_log_e("Invalid number of IP parts: %d", part);
    return false;
  }
  return true;
}

bool ICACHE_FLASH_ATTR ESP3DSettings::isValidStringSetting(const char *value, ESP3DSettingIndex settingElement) {
  esp3d_log("Validating string '%s' for setting %d", value, settingElement);
  const ESP3DSettingDescription *settingPtr = getSettingPtr(settingElement);
  if (!settingPtr || settingPtr->type != ESP3DSettingType::string_t) {
    esp3d_log_e("Invalid setting or type for %d", settingElement);
    return false;
  }
  size_t len = strlen(value);
  if (len > settingPtr->size) {
    esp3d_log_e("String length %zu exceeds max %zu for setting %d", len, settingPtr->size, settingElement);
    return false;
  }
  for (size_t i = 0; i < len; i++) {
    if (!esp3d_string::isPrintableChar(value[i])) {
      esp3d_log_e("Non-printable character at position %zu: %c", i, value[i]);
      return false;
    }
  }
  switch (settingElement) {
    case ESP3DSettingIndex::esp3d_settings_version:
      if (strcmp(value, DEFAULT_SETTINGS_VERSION) != 0) {
        esp3d_log_e("Invalid settings version %s, expected %s", value, DEFAULT_SETTINGS_VERSION);
        return false;
      }
      break;
    case ESP3DSettingIndex::esp3d_hostname:
      for (size_t i = 0; i < len; i++) {
        char c = value[i];
        if (!(isdigit(c) || isalpha(c) || c == '-')) {
          esp3d_log_e("Invalid character in hostname: %c", c);
          return false;
        }
      }
      if (len < 1) {
        esp3d_log_e("Hostname too short: %zu", len);
        return false;
      }
      break;
    case ESP3DSettingIndex::esp3d_sta_ssid:
    case ESP3DSettingIndex::esp3d_ap_ssid:
      if (len < MIN_SSID_LENGTH) {
        esp3d_log_e("SSID too short: %zu, min %d", len, MIN_SSID_LENGTH);
        return false;
      }
      break;
    case ESP3DSettingIndex::esp3d_sta_password:
    case ESP3DSettingIndex::esp3d_ap_password:
    case ESP3DSettingIndex::esp3d_admin_pwd:
    case ESP3DSettingIndex::esp3d_user_pwd:
      if (len > 0 && len < MIN_PASSWORD_LENGTH) {
        esp3d_log_e("Password too short: %zu, min %d", len, MIN_PASSWORD_LENGTH);
        return false;
      }
      break;
    case ESP3DSettingIndex::esp3d_notification_token1:
    case ESP3DSettingIndex::esp3d_notification_token2:
    case ESP3DSettingIndex::esp3d_notification_settings:
      if (len > MAX_NOTIFICATION_TOKEN_LENGTH) {
        esp3d_log_e("Notification string too long: %zu, max %d", len, MAX_NOTIFICATION_TOKEN_LENGTH);
        return false;
      }
      break;
    case ESP3DSettingIndex::esp3d_time_server1:
    case ESP3DSettingIndex::esp3d_time_server2:
    case ESP3DSettingIndex::esp3d_time_server3:
      if (len > MAX_SERVER_ADDRESS_LENGTH || (len > 0 && len < MIN_SERVER_ADDRESS_LENGTH)) {
        esp3d_log_e("Server address length invalid: %zu, max %d, min %d", len, MAX_SERVER_ADDRESS_LENGTH, MIN_SERVER_ADDRESS_LENGTH);
        return false;
      }
      break;
    case ESP3DSettingIndex::esp3d_time_zone:
      if (len > MAX_TIME_ZONE_LENGTH) {
        esp3d_log_e("Time zone string too long: %zu, max %d", len, MAX_TIME_ZONE_LENGTH);
        return false;
      }
      break;
  }
  return true;
}

bool ICACHE_FLASH_ATTR ESP3DSettings::isValidIntegerSetting(uint32_t value, ESP3DSettingIndex settingElement) {
  esp3d_log("Validating integer %u for setting %d", value, settingElement);
  const ESP3DSettingDescription *settingPtr = getSettingPtr(settingElement);
  if (!settingPtr || settingPtr->type != ESP3DSettingType::integer_t) {
    esp3d_log_e("Invalid setting or type for %d", settingElement);
    return false;
  }
  switch (settingElement) {
    case ESP3DSettingIndex::esp3d_baud_rate:
    case ESP3DSettingIndex::esp3d_serial_bridge_baud:
      for (uint8_t i = 0; i < SupportedBaudListSize; i++) {
        if (value == SupportedBaudList[i]) {
          esp3d_log("Valid baud rate: %u", value);
          return true;
        }
      }
      esp3d_log_e("Invalid baud rate: %u", value);
      break;
    case ESP3DSettingIndex::esp3d_http_port:
    case ESP3DSettingIndex::esp3d_ftp_ctrl_port:
    case ESP3DSettingIndex::esp3d_websocket_port:
    case ESP3DSettingIndex::esp3d_webdav_port:
    case ESP3DSettingIndex::esp3d_telnet_port:
      if (value >= 1 && value <= 65535) {
        esp3d_log("Valid port: %u", value);
        return true;
      }
      esp3d_log_e("Invalid port: %u", value);
      break;
    case ESP3DSettingIndex::esp3d_boot_delay:
      if (value >= MIN_BOOT_DELAY && value <= MAX_BOOT_DELAY) {
        esp3d_log("Valid boot delay: %u", value);
        return true;
      }
      esp3d_log_e("Invalid boot delay: %u", value);
      break;
    case ESP3DSettingIndex::esp3d_sensor_interval:
      if (value >= MIN_SENSOR_INTERVAL && value <= MAX_SENSOR_INTERVAL) {
        esp3d_log("Valid sensor interval: %u", value);
        return true;
      }
      esp3d_log_e("Invalid sensor interval: %u", value);
      break;
    case ESP3DSettingIndex::esp3d_session_timeout:
      if (value >= 1 && value <= 255) {
        esp3d_log("Valid session timeout: %u", value);
        return true;
      }
      esp3d_log_e("Invalid session timeout: %u", value);
      break;
  }
  return false;
}

bool ICACHE_FLASH_ATTR ESP3DSettings::isValidByteSetting(uint8_t value, ESP3DSettingIndex settingElement) {
  esp3d_log("Validating byte %d for setting %d", value, settingElement);
  const ESP3DSettingDescription *settingPtr = getSettingPtr(settingElement);
  if (!settingPtr || settingPtr->type != ESP3DSettingType::byte_t) {
    esp3d_log_e("Invalid setting or type for %d", settingElement);
    return false;
  }
  switch (settingElement) {
    case ESP3DSettingIndex::esp3d_plugin_direct_pin_enabled:
    case ESP3DSettingIndex::esp3d_http_on:
    case ESP3DSettingIndex::esp3d_ftp_on:
    case ESP3DSettingIndex::esp3d_websocket_on:
    case ESP3DSettingIndex::esp3d_verbose_boot:
    case ESP3DSettingIndex::esp3d_boot_radio_state:
    case ESP3DSettingIndex::esp3d_sd_mount:
    case ESP3DSettingIndex::esp3d_sd_check_update_at_boot:
    case ESP3DSettingIndex::esp3d_auto_notification:
    case ESP3DSettingIndex::esp3d_secure_serial:
    case ESP3DSettingIndex::esp3d_serial_bridge_on:
      if (value == (uint8_t)ESP3DState::off || value == (uint8_t)ESP3DState::on) {
        esp3d_log("Valid state: %d", value);
        return true;
      }
      esp3d_log_e("Invalid state: %d", value);
      break;
    case ESP3DSettingIndex::esp3d_radio_mode:
      if (value == ESP_NO_NETWORK || value == ESP_WIFI_STA || value == ESP_WIFI_AP || value == ESP_AP_SETUP || value == ESP_BT) {
        esp3d_log("Valid radio mode: %d", value);
        return true;
      }
      esp3d_log_e("Invalid radio mode: %d", value);
      break;
    case ESP3DSettingIndex::esp3d_ap_channel:
      for (uint8_t i = 0; i < SupportedApChannelsSize; i++) {
        if (value == SupportedApChannels[i]) {
          esp3d_log("Valid AP channel: %d", value);
          return true;
        }
      }
      esp3d_log_e("Invalid AP channel: %d", value);
      break;
    case ESP3DSettingIndex::esp3d_notification_type:
      if (value <= (uint8_t)ESP3DNotificationsType::max) {
        esp3d_log("Valid notification type: %d", value);
        return true;
      }
      esp3d_log_e("Invalid notification type: %d", value);
      break;
    case ESP3DSettingIndex::esp3d_sensor_type:
      if (value <= (uint8_t)ESP3DSensorType::max) {
        esp3d_log("Valid sensor type: %d", value);
        return true;
      }
      esp3d_log_e("Invalid sensor type: %d", value);
      break;
    case ESP3DSettingIndex::esp3d_sd_speed_div:
      for (uint8_t i = 0; i < SupportedSPIDividerSize; i++) {
        if (value == SupportedSPIDivider[i]) {
          esp3d_log("Valid SD SPI divider: %d", value);
          return true;
        }
      }
      esp3d_log_e("Invalid SD SPI divider: %d", value);
      break;
    case ESP3DSettingIndex::esp3d_output_client:
      if (value == (uint8_t)ESP3DClientType::serial || value == (uint8_t)ESP3DClientType::mks_serial || value == (uint8_t)ESP3DClientType::socket_serial) {
        esp3d_log("Valid output client: %d", value);
        return true;
      }
      esp3d_log_e("Invalid output client: %d", value);
      break;
  }
  return false;
}

uint32_t ICACHE_FLASH_ATTR ESP3DSettings::getDefaultIntegerSetting(ESP3DSettingIndex settingElement) {
  const ESP3DSettingDescription *query = getSettingPtr(settingElement);
  if (query && query->type == ESP3DSettingType::integer_t) {
    return (uint32_t)strtoul(query->default_val, NULL, 0);
  }
  if (query && query->type == ESP3DSettingType::ip_t) {
    return _stringToIP(query->default_val);
  }
  return 0;
}

String ICACHE_FLASH_ATTR ESP3DSettings::getDefaultStringSetting(ESP3DSettingIndex settingElement) {
  const ESP3DSettingDescription *query = getSettingPtr(settingElement);
  if (query && (query->type == ESP3DSettingType::string_t || query->type == ESP3DSettingType::ip_t)) {
    return String(query->default_val);
  }
  return "";
}

uint8_t ICACHE_FLASH_ATTR ESP3DSettings::getDefaultByteSetting(ESP3DSettingIndex settingElement) {
  const ESP3DSettingDescription *query = getSettingPtr(settingElement);
  if (query && query->type == ESP3DSettingType::byte_t) {
    return (uint8_t)strtoul(query->default_val, NULL, 0);
  }
  return 0;
}

const ESP3DSettingDescription * ICACHE_FLASH_ATTR ESP3DSettings::getSettingPtr(const ESP3DSettingIndex index) {
  static ESP3DSettingDescription setting;
  esp3d_log("Getting setting pointer for index %d", index);
  memset(&setting, 0, sizeof(setting));
  setting.index = index;
  switch (index) {
    case ESP3DSettingIndex::esp3d_plugin_direct_pin_enabled:
      setting.type = ESP3DSettingType::byte_t;
      setting.size = 1;
      setting.default_val = DEFAULT_PLUGIN_DIRECT_PIN_ENABLED;
      break;
    case ESP3DSettingIndex::esp3d_radio_mode:
      setting.type = ESP3DSettingType::byte_t;
      setting.size = 1;
      setting.default_val = DEFAULT_ESP_RADIO_MODE;
      break;
    case ESP3DSettingIndex::esp3d_sta_ssid:
      setting.type = ESP3DSettingType::string_t;
      setting.size = 32;
      setting.default_val = DEFAULT_STA_SSID;
      break;
    case ESP3DSettingIndex::esp3d_sta_password:
      setting.type = ESP3DSettingType::string_t;
      setting.size = 64;
      setting.default_val = DEFAULT_STA_PASSWORD;
      break;
    case ESP3DSettingIndex::esp3d_boot_radio_state:
      setting.type = ESP3DSettingType::byte_t;
      setting.size = 1;
      setting.default_val = DEFAULT_BOOT_RADIO_STATE;
      break;
    case ESP3DSettingIndex::esp3d_sta_fallback_mode:
      setting.type = ESP3DSettingType::byte_t;
      setting.size = 1;
      setting.default_val = DEFAULT_STA_FALLBACK_MODE;
      break;
    case ESP3DSettingIndex::esp3d_ap_ssid:
      setting.type = ESP3DSettingType::string_t;
      setting.size = 32;
      setting.default_val = DEFAULT_AP_SSID;
      break;
    case ESP3DSettingIndex::esp3d_ap_password:
      setting.type = ESP3DSettingType::string_t;
      setting.size = 64;
      setting.default_val = DEFAULT_AP_PASSWORD;
      break;
    case ESP3DSettingIndex::esp3d_sta_ip_value:
      setting.type = ESP3DSettingType::ip_t;
      setting.size = 4;
      setting.default_val = DEFAULT_STA_IP_VALUE;
      break;
    case ESP3DSettingIndex::esp3d_sta_gateway_value:
      setting.type = ESP3DSettingType::ip_t;
      setting.size = 4;
      setting.default_val = DEFAULT_STA_GATEWAY_VALUE;
      break;
    case ESP3DSettingIndex::esp3d_sta_mask_value:
      setting.type = ESP3DSettingType::ip_t;
      setting.size = 4;
      setting.default_val = DEFAULT_STA_MASK_VALUE;
      break;
    case ESP3DSettingIndex::esp3d_sta_dns_value:
      setting.type = ESP3DSettingType::ip_t;
      setting.size = 4;
      setting.default_val = DEFAULT_STA_DNS_VALUE;
      break;
    case ESP3DSettingIndex::esp3d_ap_ip_value:
      setting.type = ESP3DSettingType::ip_t;
      setting.size = 4;
      setting.default_val = DEFAULT_AP_IP_VALUE;
      break;
    case ESP3DSettingIndex::esp3d_sta_ip_mode:
      setting.type = ESP3DSettingType::byte_t;
      setting.size = 1;
      setting.default_val = DEFAULT_STA_IP_MODE;
      break;
    case ESP3DSettingIndex::esp3d_settings_version:
      setting.type = ESP3DSettingType::string_t;
      setting.size = MAX_VERSION_LENGTH;
      setting.default_val = DEFAULT_SETTINGS_VERSION;
      break;
    case ESP3DSettingIndex::esp3d_ap_channel:
      setting.type = ESP3DSettingType::byte_t;
      setting.size = 1;
      setting.default_val = DEFAULT_AP_CHANNEL;
      break;
    case ESP3DSettingIndex::esp3d_http_on:
      setting.type = ESP3DSettingType::byte_t;
      setting.size = 1;
      setting.default_val = DEFAULT_HTTP_ON;
      break;
    case ESP3DSettingIndex::esp3d_ftp_on:
      setting.type = ESP3DSettingType::byte_t;
      setting.size = 1;
      setting.default_val = DEFAULT_FTP_ON;
      break;
    case ESP3DSettingIndex::esp3d_telnet_port:
      setting.type = ESP3DSettingType::integer_t;
      setting.size = 4;
      setting.default_val = DEFAULT_TELNET_PORT;
      break;
    case ESP3DSettingIndex::esp3d_websocket_on:
      setting.type = ESP3DSettingType::byte_t;
      setting.size = 1;
      setting.default_val = DEFAULT_WEBSOCKET_ON;
      break;
    case ESP3DSettingIndex::esp3d_verbose_boot:
      setting.type = ESP3DSettingType::byte_t;
      setting.size = 1;
      setting.default_val = DEFAULT_VERBOSE_BOOT;
      break;
    case ESP3DSettingIndex::esp3d_hostname:
      setting.type = ESP3DSettingType::string_t;
      setting.size = 32;
      setting.default_val = DEFAULT_HOSTNAME;
      break;
    case ESP3DSettingIndex::esp3d_baud_rate:
      setting.type = ESP3DSettingType::integer_t;
      setting.size = 4;
      setting.default_val = DEFAULT_BAUD_RATE;
      break;
    case ESP3DSettingIndex::esp3d_http_port:
      setting.type = ESP3DSettingType::integer_t;
      setting.size = 4;
      setting.default_val = DEFAULT_HTTP_PORT;
      break;
    case ESP3DSettingIndex::esp3d_ftp_ctrl_port:
      setting.type = ESP3DSettingType::integer_t;
      setting.size = 4;
      setting.default_val = DEFAULT_FTP_CTRL_PORT;
      break;
    case ESP3DSettingIndex::esp3d_websocket_port:
      setting.type = ESP3DSettingType::integer_t;
      setting.size = 4;
      setting.default_val = DEFAULT_WEBSOCKET_PORT;
      break;
    case ESP3DSettingIndex::esp3d_webdav_port:
      setting.type = ESP3DSettingType::integer_t;
      setting.size = 4;
      setting.default_val = DEFAULT_WEBDAV_PORT;
      break;
    case ESP3DSettingIndex::esp3d_boot_delay:
      setting.type = ESP3DSettingType::integer_t;
      setting.size = 4;
      setting.default_val = DEFAULT_BOOT_DELAY;
      break;
    case ESP3DSettingIndex::esp3d_notification_type:
      setting.type = ESP3DSettingType::byte_t;
      setting.size = 1;
      setting.default_val = DEFAULT_NOTIFICATION_TYPE;
      break;
    case ESP3DSettingIndex::esp3d_notification_token1:
      setting.type = ESP3DSettingType::string_t;
      setting.size = MAX_NOTIFICATION_TOKEN_LENGTH;
      setting.default_val = DEFAULT_NOTIFICATION_TOKEN1;
      break;
    case ESP3DSettingIndex::esp3d_notification_token2:
      setting.type = ESP3DSettingType::string_t;
      setting.size = MAX_NOTIFICATION_TOKEN_LENGTH;
      setting.default_val = DEFAULT_NOTIFICATION_TOKEN2;
      break;
    case ESP3DSettingIndex::esp3d_notification_settings:
      setting.type = ESP3DSettingType::string_t;
      setting.size = MAX_NOTIFICATION_SETTINGS_LENGTH;
      setting.default_val = DEFAULT_NOTIFICATION_SETTINGS;
      break;
    case ESP3DSettingIndex::esp3d_auto_notification:
      setting.type = ESP3DSettingType::byte_t;
      setting.size = 1;
      setting.default_val = DEFAULT_AUTO_NOTIFICATION_STATE;
      break;
    case ESP3DSettingIndex::esp3d_time_server1:
      setting.type = ESP3DSettingType::string_t;
      setting.size = MAX_SERVER_ADDRESS_LENGTH;
      setting.default_val = DEFAULT_TIME_SERVER1;
      break;
    case ESP3DSettingIndex::esp3d_time_server2:
      setting.type = ESP3DSettingType::string_t;
      setting.size = MAX_SERVER_ADDRESS_LENGTH;
      setting.default_val = DEFAULT_TIME_SERVER2;
      break;
    case ESP3DSettingIndex::esp3d_time_server3:
      setting.type = ESP3DSettingType::string_t;
      setting.size = MAX_SERVER_ADDRESS_LENGTH;
      setting.default_val = DEFAULT_TIME_SERVER3;
      break;
    case ESP3DSettingIndex::esp3d_time_zone:
      setting.type = ESP3DSettingType::string_t;
      setting.size = MAX_TIME_ZONE_LENGTH;
      setting.default_val = DEFAULT_TIME_ZONE;
      break;
    case ESP3DSettingIndex::esp3d_sensor_type:
      setting.type = ESP3DSettingType::byte_t;
      setting.size = 1;
      setting.default_val = DEFAULT_SENSOR_TYPE;
      break;
    case ESP3DSettingIndex::esp3d_sensor_interval:
      setting.type = ESP3DSettingType::integer_t;
      setting.size = 4;
      setting.default_val = DEFAULT_SENSOR_INTERVAL;
      break;
    case ESP3DSettingIndex::esp3d_output_client:
      setting.type = ESP3DSettingType::byte_t;
      setting.size = 1;
      setting.default_val = DEFAULT_OUTPUT_CLIENT;
      break;
    case ESP3DSettingIndex::esp3d_admin_pwd:
      setting.type = ESP3DSettingType::string_t;
      setting.size = MAX_LOCAL_PASSWORD_LENGTH;
      setting.default_val = DEFAULT_ADMIN_PWD;
      break;
    case ESP3DSettingIndex::esp3d_user_pwd:
      setting.type = ESP3DSettingType::string_t;
      setting.size = MAX_LOCAL_PASSWORD_LENGTH;
      setting.default_val = DEFAULT_USER_PWD;
      break;
    case ESP3DSettingIndex::esp3d_session_timeout:
      setting.type = ESP3DSettingType::integer_t;
      setting.size = 4;
      setting.default_val = DEFAULT_SESSION_TIMEOUT;
      break;
    case ESP3DSettingIndex::esp3d_secure_serial:
      setting.type = ESP3DSettingType::byte_t;
      setting.size = 1;
      setting.default_val = DEFAULT_SECURE_SERIAL;
      break;
    case ESP3DSettingIndex::esp3d_sd_mount:
      setting.type = ESP3DSettingType::byte_t;
      setting.size = 1;
      setting.default_val = DEFAULT_SD_MOUNT;
      break;
    case ESP3DSettingIndex::esp3d_sd_check_update_at_boot:
      setting.type = ESP3DSettingType::byte_t;
      setting.size = 1;
      setting.default_val = DEFAULT_SD_CHECK_UPDATE_AT_BOOT;
      break;
    case ESP3DSettingIndex::esp3d_sd_speed_div:
      setting.type = ESP3DSettingType::byte_t;
      setting.size = 1;
      setting.default_val = DEFAULT_SD_SPI_DIV;
      break;
    case ESP3DSettingIndex::esp3d_serial_bridge_on:
      setting.type = ESP3DSettingType::byte_t;
      setting.size = 1;
      setting.default_val = DEFAULT_SERIAL_BRIDGE_ON;
      break;
    case ESP3DSettingIndex::esp3d_serial_bridge_baud:
      setting.type = ESP3DSettingType::integer_t;
      setting.size = 4;
      setting.default_val = DEFAULT_SERIAL_BRIDGE_BAUD_RATE;
      break;
    default:
      esp3d_log_e("Unknown setting index %d", index);
      return NULL;
  }
  return &setting;
}
#endif  // ESP_SAVE_SETTINGS