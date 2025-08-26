/*
 ESP400.cpp - ESP3D command class

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
#include "../esp3d_commands.h"
#include "../esp3d_settings.h"
#if COMMUNICATION_PROTOCOL != SOCKET_SERIAL
#include "../../modules/serial/serial_service.h"
#endif  // COMMUNICATION_PROTOCOL != SOCKET_SERIAL
#include "../../modules/authentication/authentication_service.h"
#if defined(SENSOR_DEVICE)
#include "../../modules/sensor/sensor.h"
#endif  // SENSOR_DEVICE
#ifdef TIMESTAMP_FEATURE
#include "../../modules/time/time_service.h"
#endif  // TIMESTAMP_FEATURE
#define COMMAND_ID 400

#ifndef STRINGIFY
#define STRINGIFY(x) #x
#endif  // STRINGIFY
#define STRING(x) STRINGIFY(x)

#if defined(USB_SERIAL_FEATURE)
const char* OutputClientsLabels[] = {
    "serial port",
    "usb port",
};
const char* OutputClientsValues[] = {"1",
                                     "2"};
#endif  // USB_SERIAL_FEATURE

const char* YesNoLabels[] = {"no", "yes"};
const char* YesNoValues[] = {"0", "1"};
const char* RadioModeLabels[] = {"none"
#ifdef WIFI_FEATURE
                                 ,
                                 "sta",
                                 "ap",
                                 "setup"
#endif  // WIFI_FEATURE
#ifdef BLUETOOTH_FEATURE
                                 ,
                                 "bt"
#endif  // BLUETOOTH_FEATURE
#ifdef ETH_FEATURE
                                 ,
                                 "eth-sta"
#endif  // ETH_FEATURE
};
const char* RadioModeValues[] = {"0"
#ifdef WIFI_FEATURE
                                 ,
                                 "1",
                                 "2",
                                 "5"
#endif  // WIFI_FEATURE
#ifdef BLUETOOTH_FEATURE
                                 ,
                                 "3"
#endif  // BLUETOOTH_FEATURE
#ifdef ETH_FEATURE
                                 ,
                                 "4"
#endif  // ETH_FEATURE
};

const char* FallbackLabels[] = {"none"
#ifdef WIFI_FEATURE
                                ,
                                "setup"
#endif  // WIFI_FEATURE
#ifdef BLUETOOTH_FEATURE
                                ,
                                "bt"
#endif  // BLUETOOTH_FEATURE
};
const char* FallbackValues[] = {"0"
#ifdef WIFI_FEATURE
                                ,
                                "5"
#endif  // WIFI_FEATURE
#ifdef BLUETOOTH_FEATURE
                                ,
                                "3"
#endif  // BLUETOOTH_FEATURE
};

const char* EthFallbackValues[] = {"0"
#ifdef BLUETOOTH_FEATURE
                                   ,
                                   "3"
#endif  // BLUETOOTH_FEATURE
};
const char* EthFallbackLabels[] = {"none"
#ifdef BLUETOOTH_FEATURE
                                   ,
                                   "bt"
#endif  // BLUETOOTH_FEATURE
};

const char* FirmwareLabels[] = {"Unknown", "Grbl", "Marlin", "Klipper", "Smoothieware",
                                "Repetier"};

const char* FirmwareValues[] = {"0", "10", "20", "40", "50"};
#ifdef NOTIFICATION_FEATURE
const char* NotificationsLabels[] = {
    "none", "pushover", "email", "line", "telegram", "ifttt", "home-assistant","WhatsApp"};

const char* NotificationsValues[] = {"0", "1", "2", "3", "4", "5", "6", "7"};
#endif  // NOTIFICATION_FEATURE

const char* IpModeLabels[] = {"static", "dhcp"};
const char* IpModeValues[] = {"0", "1"};

const char* SupportedApChannelsStr[] = {"1", "2", "3",  "4",  "5",  "6",  "7",
                                        "8", "9", "10", "11", "12", "13", "14"};

const char* SupportedBaudListSizeStr[] = {
    "9600",   "19200",  "38400",  "57600",   "74880",   "115200", "230400",
    "250000", "500000", "921600", "1000000", "1958400", "2000000"};

#ifdef SENSOR_DEVICE
const char* SensorLabels[] = {"none"
#if SENSOR_DEVICE == DHT11_DEVICE || SENSOR_DEVICE == DHT22_DEVICE
                              ,
                              "DHT11",
                              "DHT22"
#endif
#if SENSOR_DEVICE == ANALOG_DEVICE
                              ,
                              "Analog"
#endif
#if SENSOR_DEVICE == BMP280_DEVICE || SENSOR_DEVICE == BME280_DEVICE
                              ,
                              "BMP280",
                              "BME280"
#endif
};
const char* SensorValues[] = {"0"
#if SENSOR_DEVICE == DHT11_DEVICE || SENSOR_DEVICE == DHT22_DEVICE
                              ,
                              "1",
                              "2"
#endif
#if SENSOR_DEVICE == ANALOG_DEVICE
                              ,
                              "3"
#endif
#if SENSOR_DEVICE == BMP280_DEVICE || SENSOR_DEVICE == BME280_DEVICE
                              ,
                              "4",
                              "5"
#endif
};
#endif  // SENSOR_DEVICE

#ifdef SD_DEVICE
#if SD_DEVICE != ESP_SDIO
const char* SupportedSPIDividerStr[] = {"1", "2", "4", "6", "8", "16", "32"};
const uint8_t SupportedSPIDividerStrSize =
    sizeof(SupportedSPIDividerStr) / sizeof(char*);
#endif  // SD_DEVICE != ESP_SDIO
#endif  // SD_DEVICE

// Get full ESP3D settings
//[ESP400]<pwd=admin>
void ESP3DCommands::ESP400(int cmd_params_pos, ESP3DMessage* msg) {
  ESP3DClientType target = msg->origin;
  ESP3DRequest requestId = msg->request_id;
  msg->target = target;
  msg->origin = ESP3DClientType::command;
  bool json = hasTag(msg, cmd_params_pos, "json");
  String tmpstr;
#if defined(AUTHENTICATION_FEATURE)
  if (msg->authentication_level == ESP3DAuthenticationLevel::guest) {
    msg->authentication_level = ESP3DAuthenticationLevel::not_authenticated;
    dispatchAuthenticationError(msg, COMMAND_ID, json);
    return;
  }
#endif  // AUTHENTICATION_FEATURE
  if (json) {
    tmpstr = "{\"cmd\":\"400\",\"status\":\"ok\",\"data\":[";

  } else {
    tmpstr = "Settings:\n";
  }
  msg->type = ESP3DMessageType::head;
  if (!dispatch(msg, tmpstr.c_str())) {
    esp3d_log_e("Error sending response to clients");
    return;
  }

#if defined(WIFI_FEATURE) || defined(ETH_FEATURE) || defined(BT_FEATURE)
  // Hostname network/network
  dispatchSetting(json, "network/network", ESP3DSettingIndex::esp3d_hostname, "hostname", NULL, NULL,
                  32, 1, 1, -1, NULL, true, target, requestId, true);
#endif  // WIFI_FEATURE || ETH_FEATURE || BT_FEATURE

  // radio mode network/network
  dispatchSetting(json, "network/network", ESP3DSettingIndex::esp3d_radio_mode, "radio mode",
                  RadioModeValues, RadioModeLabels,
                  sizeof(RadioModeValues) / sizeof(char*), -1, -1, -1, NULL,
                  true, target, requestId);

  // Radio State at Boot
  dispatchSetting(json, "network/network", ESP3DSettingIndex::esp3d_boot_radio_state, "radio_boot",
                  YesNoValues, YesNoLabels, sizeof(YesNoValues) / sizeof(char*),
                  -1, -1, -1, NULL, true, target, requestId);
#if defined(ETH_FEATURE)
  // Ethernet STA IP mode
  dispatchSetting(json, "network/eth-sta", ESP3DSettingIndex::esp3d_eth_sta_ip_mode, "ip mode",
                  IpModeValues, IpModeLabels,
                  sizeof(IpModeLabels) / sizeof(char*), -1, -1, -1, nullptr,
                  true, target, requestId);
  // Ethernet STA static IP
  dispatchSetting(json, "network/eth-sta", ESP3DSettingIndex::esp3d_eth_sta_ip_value, "ip", nullptr,
                  nullptr, -1, -1, -1, -1, nullptr, true, target, requestId);

  // Ethernet STA static Gateway
  dispatchSetting(json, "network/eth-sta", ESP3DSettingIndex::esp3d_eth_sta_gateway_value, "gw",
                  nullptr, nullptr, -1, -1, -1, -1, nullptr, true, target,
                  requestId);
  // Ethernet STA static Mask
  dispatchSetting(json, "network/eth-sta", ESP3DSettingIndex::esp3d_eth_sta_mask_value, "msk",
                  nullptr, nullptr, -1, -1, -1, -1, nullptr, true, target,
                  requestId);
  // Ethernet STA static DNS
  dispatchSetting(json, "network/eth-sta", ESP3DSettingIndex::esp3d_eth_sta_dns_value, "DNS",
                  nullptr, nullptr, -1, -1, -1, -1, nullptr, true, target,
                  requestId);
  // Ethernet Sta fallback mode
  dispatchSetting(json, "network/eth-sta", ESP3DSettingIndex::esp3d_eth_sta_fallback_mode,
                  "sta fallback mode", EthFallbackValues, EthFallbackLabels,
                  sizeof(EthFallbackValues) / sizeof(char*), -1, -1, -1,
                  nullptr, true, target, requestId);
#endif  // ETH_FEATURE
#ifdef WIFI_FEATURE
  // STA SSID network/sta
  dispatchSetting(json, "network/sta", ESP3DSettingIndex::esp3d_sta_ssid, "SSID", nullptr, nullptr,
                  32, 1, 1, -1, nullptr, true, target, requestId);

  // STA Password network/sta
  dispatchSetting(json, "network/sta", ESP3DSettingIndex::esp3d_sta_password, "pwd", nullptr,
                  nullptr, 64, 8, 0, -1, nullptr, true, target, requestId);

#endif  // WIFI_FEATURE
#if defined(WIFI_FEATURE)
  // STA IP mode
  dispatchSetting(json, "network/sta", ESP3DSettingIndex::esp3d_sta_ip_mode, "ip mode", IpModeValues,
                  IpModeLabels, sizeof(IpModeLabels) / sizeof(char*), -1, -1,
                  -1, nullptr, true, target, requestId);
  // STA static IP
  dispatchSetting(json, "network/sta", ESP3DSettingIndex::esp3d_sta_ip_value, "ip", nullptr, nullptr,
                  -1, -1, -1, -1, nullptr, true, target, requestId);

  // STA static Gateway
  dispatchSetting(json, "network/sta", ESP3DSettingIndex::esp3d_sta_gateway_value, "gw", nullptr,
                  nullptr, -1, -1, -1, -1, nullptr, true, target, requestId);
  // STA static Mask
  dispatchSetting(json, "network/sta", ESP3DSettingIndex::esp3d_sta_mask_value, "msk", nullptr,
                  nullptr, -1, -1, -1, -1, nullptr, true, target, requestId);
  // STA static DNS
  dispatchSetting(json, "network/sta", ESP3DSettingIndex::esp3d_sta_dns_value, "DNS", nullptr,
                  nullptr, -1, -1, -1, -1, nullptr, true, target, requestId);

#endif  // WIFI_FEATURE

#if defined(WIFI_FEATURE)
  // Sta fallback mode
  dispatchSetting(json, "network/sta", ESP3DSettingIndex::esp3d_sta_fallback_mode,
                  "sta fallback mode", FallbackValues, FallbackLabels,
                  sizeof(FallbackValues) / sizeof(char*), -1, -1, -1, nullptr,
                  true, target, requestId);
#endif  // WIFI_FEATURE
#if defined(WIFI_FEATURE)
  // AP SSID network/ap
  dispatchSetting(json, "network/ap", ESP3DSettingIndex::esp3d_ap_ssid, "SSID", nullptr, nullptr, 32,
                  1, 1, -1, nullptr, true, target, requestId);

  // AP password
  dispatchSetting(json, "network/ap", ESP3DSettingIndex::esp3d_ap_password, "pwd", nullptr, nullptr,
                  64, 8, 0, -1, nullptr, true, target, requestId);
  // AP static IP
  dispatchSetting(json, "network/ap", ESP3DSettingIndex::esp3d_ap_ip_value, "ip", nullptr, nullptr,
                  -1, -1, -1, -1, nullptr, true, target, requestId);

  // AP Channel
  dispatchSetting(json, "network/ap", ESP3DSettingIndex::esp3d_ap_channel, "channel",
                  SupportedApChannelsStr, SupportedApChannelsStr,
                  sizeof(SupportedApChannelsStr) / sizeof(char*), -1, -1, -1,
                  nullptr, true, target, requestId);
#endif  // WIFI_FEATURE

#ifdef AUTHENTICATION_FEATURE
  // Admin password
  dispatchSetting(json, "security/security", ESP3DSettingIndex::esp3d_admin_pwd, "adm pwd", nullptr,
                  nullptr, 20, 0, -1, -1, nullptr, true, target, requestId);
  // User password
  dispatchSetting(json, "security/security", ESP3DSettingIndex::esp3d_user_pwd, "user pwd", nullptr,
                  nullptr, 20, 0, -1, -1, nullptr, true, target, requestId);

  // session timeout
  dispatchSetting(json, "security/security", ESP3DSettingIndex::esp3d_session_timeout,
                  "session timeout", nullptr, nullptr, 255, 0, -1, -1, nullptr,
                  true, target, requestId);

#if COMMUNICATION_PROTOCOL == RAW_SERIAL || COMMUNICATION_PROTOCOL == MKS_SERIAL
  // Secure Serial
  dispatchSetting(json, "security/security", ESP3DSettingIndex::esp3d_secure_serial, "serial",
                  YesNoValues, YesNoLabels, sizeof(YesNoValues) / sizeof(char*),
                  -1, -1, -1, nullptr, true, target, requestId);
#endif  // COMMUNICATION_PROTOCOL
#endif  // AUTHENTICATION_FEATURE
#ifdef HTTP_FEATURE
  // HTTP On service/http
  dispatchSetting(json, "service/http", ESP3DSettingIndex::esp3d_http_on, "enable", YesNoValues,
                  YesNoLabels, sizeof(YesNoValues) / sizeof(char*), -1, -1, -1,
                  nullptr, true, target, requestId);
  // HTTP port
  dispatchSetting(json, "service/http", ESP3DSettingIndex::esp3d_http_port, "port", nullptr, nullptr,
                  65535, 1, -1, -1, nullptr, true, target, requestId);
#endif  // HTTP_FEATURE

#ifdef TELNET_FEATURE
  // TELNET On service/telnet
  dispatchSetting(json, "service/telnetp", ESP3DSettingIndex::esp3d_telnet_port, "enable", YesNoValues,
                  YesNoLabels, sizeof(YesNoValues) / sizeof(char*), -1, -1, -1,
                  nullptr, true, target, requestId);

  // TELNET Port
  dispatchSetting(json, "service/telnetp", ESP3DSettingIndex::esp3d_telnet_port, "port", nullptr,
                  nullptr, 65535, 1, -1, -1, nullptr, true, target, requestId);
#endif  // TELNET_FEATURE

#ifdef WS_DATA_FEATURE
  // Websocket On service
  dispatchSetting(json, "service/websocketp", ESP3DSettingIndex::esp3d_websocket_on, "enable",
                  YesNoValues, YesNoLabels, sizeof(YesNoValues) / sizeof(char*),
                  -1, -1, -1, nullptr, true, target, requestId);

  // Websocket Port
  dispatchSetting(json, "service/websocketp", ESP3DSettingIndex::esp3d_websocket_port, "port",
                  nullptr, nullptr, 65535, 1, -1, -1, nullptr, true, target,
                  requestId);
#endif  // WS_DATA_FEATURE

#ifdef WEBDAV_FEATURE
  // WebDav On service
  dispatchSetting(json, "service/webdavp", ESP3DSettingIndex::esp3d_webdav_on, "enable", YesNoValues,
                  YesNoLabels, sizeof(YesNoValues) / sizeof(char*), -1, -1, -1,
                  nullptr, true, target, requestId);

  // WebDav Port
  dispatchSetting(json, "service/webdavp", ESP3DSettingIndex::esp3d_webdav_port, "port", nullptr,
                  nullptr, 65535, 1, -1, -1, nullptr, true, target, requestId);
#endif  // WEBDAV_FEATURE

#ifdef FTP_FEATURE
  // FTP On service/ftp
  dispatchSetting(json, "service/ftp", ESP3DSettingIndex::esp3d_ftp_on, "enable", YesNoValues,
                  YesNoLabels, sizeof(YesNoValues) / sizeof(char*), -1, -1, -1,
                  nullptr, true, target, requestId);

  // FTP Ports
  // CTRL Port
  dispatchSetting(json, "service/ftp", ESP3DSettingIndex::esp3d_ftp_ctrl_port, "control port",
                  nullptr, nullptr, 65535, 1, -1, -1, nullptr, true, target,
                  requestId);

  // Active Port
  dispatchSetting(json, "service/ftp", ESP3DSettingIndex::esp3d_ftp_data_active_port, "active port",
                  nullptr, nullptr, 65535, 1, -1, -1, nullptr, true, target,
                  requestId);

  // Passive Port
  dispatchSetting(json, "service/ftp", ESP3DSettingIndex::esp3d_ftp_data_passive_port,
                  "passive port", nullptr, nullptr, 65535, 1, -1, -1, nullptr,
                  true, target, requestId);
#endif  // FTP_FEATURE

#ifdef TIMESTAMP_FEATURE
  // Internet Time
  dispatchSetting(json, "service/time", ESP3DSettingIndex::esp3d_internet_time, "i-time",
                  YesNoValues, YesNoLabels, sizeof(YesNoValues) / sizeof(char*),
                  -1, -1, -1, nullptr, true, target, requestId);

  // Time zone
  dispatchSetting(json, "service/time", ESP3DSettingIndex::esp3d_time_zone, "tzone",
                  SupportedTimeZones, SupportedTimeZones,
                  SupportedTimeZonesSize, -1, -1, -1, nullptr, true, target,
                  requestId);

  // Time Server1
  dispatchSetting(json, "service/time", ESP3DSettingIndex::esp3d_time_server1, "t-server", nullptr,
                  nullptr, 127, 0, -1, -1, nullptr, true, target, requestId);

  // Time Server2
  dispatchSetting(json, "service/time", ESP3DSettingIndex::esp3d_time_server2, "t-server", nullptr,
                  nullptr, 127, 0, -1, -1, nullptr, true, target, requestId);

  // Time Server3
  dispatchSetting(json, "service/time", ESP3DSettingIndex::esp3d_time_server3, "t-server", nullptr,
                  nullptr, 127, 0, -1, -1, nullptr, true, target, requestId);
#endif  // TIMESTAMP_FEATURE

#ifdef NOTIFICATION_FEATURE
  // Auto notification
  dispatchSetting(json, "service/notification", ESP3DSettingIndex::esp3d_auto_notification,
                  "auto notif", YesNoValues, YesNoLabels,
                  sizeof(YesNoValues) / sizeof(char*), -1, -1, -1, nullptr,
                  true, target, requestId);

  // Notification type
  dispatchSetting(json, "service/notification", ESP3DSettingIndex::esp3d_notification_type,
                  "notification", NotificationsValues, NotificationsLabels,
                  sizeof(NotificationsValues) / sizeof(char*), -1, -1, -1,
                  nullptr, true, target, requestId);

  // Token 1
  dispatchSetting(json, "service/notification", ESP3DSettingIndex::esp3d_notification_token1, "t1",
                  nullptr, nullptr, 250, 0, -1, -1, nullptr, true, target,
                  requestId);

  // Token 2
  dispatchSetting(json, "service/notification", ESP3DSettingIndex::esp3d_notification_token2, "t2",
                  nullptr, nullptr, 63, 0, -1, -1, nullptr, true, target,
                  requestId);

  // Notifications Settings
  dispatchSetting(json, "service/notification", ESP3DSettingIndex::esp3d_notification_settings, "ts",
                  nullptr, nullptr, 128, 0, -1, -1, nullptr, true, target,
                  requestId);
#endif  // NOTIFICATION_FEATURE

#ifdef BUZZER_DEVICE
  // Buzzer state
  dispatchSetting(json, "device/device", ESP3DSettingIndex::esp3d_buzzer, "buzzer", YesNoValues,
                  YesNoLabels, sizeof(YesNoValues) / sizeof(char*), -1, -1, -1,
                  nullptr, true, target, requestId);
#endif  // BUZZER_DEVICE

#ifdef SENSOR_DEVICE
  // Sensor type
  dispatchSetting(json, "device/sensor", ESP3DSettingIndex::esp3d_sensor_type, "type", SensorValues,
                  SensorLabels, sizeof(SensorValues) / sizeof(char*), -1, -1,
                  -1, nullptr, true, target, requestId);

  // Sensor interval
  dispatchSetting(json, "device/sensor", ESP3DSettingIndex::esp3d_sensor_interval, "intervalms",
                  nullptr, nullptr, 60000, 0, -1, -1, nullptr, true, target,
                  requestId);
#endif  // SENSOR_DEVICE
#if defined(SD_DEVICE)
#if SD_DEVICE != ESP_SDIO
  // SPI SD Divider
  dispatchSetting(json, "device/sd", ESP3DSettingIndex::esp3d_sd_speed_div, "speedx",
                  SupportedSPIDividerStr, SupportedSPIDividerStr,
                  SupportedSPIDividerStrSize, -1, -1, -1, nullptr, true, target,
                  requestId);
#endif  // SD_DEVICE != ESP_SDIO
#ifdef SD_UPDATE_FEATURE
  // SD CHECK UPDATE AT BOOT feature
  dispatchSetting(json, "device/sd", ESP3DSettingIndex::esp3d_sd_check_update_at_boot, "SD updater",
                  YesNoValues, YesNoLabels, sizeof(YesNoValues) / sizeof(char*),
                  -1, -1, -1, nullptr, true, target, requestId);
#endif  // SD_UPDATE_FEATURE
#endif  // SD_DEVICE

#if !defined(FIXED_FW_TARGET)
  // Target FW
  dispatchSetting(json, "system/system", ESP3DSettingIndex::esp3d_target_fw, "targetfw",
                  FirmwareValues, FirmwareLabels,
                  sizeof(FirmwareValues) / sizeof(char*), -1, -1, -1, nullptr,
                  true, target, requestId);
#endif  // FIXED_FW_TARGET
#if COMMUNICATION_PROTOCOL == RAW_SERIAL || COMMUNICATION_PROTOCOL == MKS_SERIAL
#if defined(USB_SERIAL_FEATURE)
  dispatchSetting(json, "system/system", ESP3DSettingIndex::esp3d_output_client, "output",
                  OutputClientsValues, OutputClientsLabels,
                  sizeof(OutputClientsValues) / sizeof(char*), -1, -1, -1,
                  nullptr, true, target, requestId);
  // Usb-Serial Baud Rate
  dispatchSetting(json, "system/system", ESP3DSettingIndex::esp3d_usb_serial_baud_rate,
                  "usb-serial baud", SupportedBaudListSizeStr,
                  SupportedBaudListSizeStr,
                  sizeof(SupportedBaudListSizeStr) / sizeof(char*), -1, -1, -1,
                  nullptr, true, target, requestId);
#endif  // defined(USB_SERIAL_FEATURE)
  // Serial Baud Rate
  dispatchSetting(json, "system/system", ESP3DSettingIndex::esp3d_baud_rate, "baud",
                  SupportedBaudListSizeStr, SupportedBaudListSizeStr,
                  sizeof(SupportedBaudListSizeStr) / sizeof(char*), -1, -1, -1,
                  nullptr, true, target, requestId);
#endif  // COMMUNICATION_PROTOCOL == RAW_SERIAL || COMMUNICATION_PROTOCOL == MKS_SERIAL

  // Start delay
  dispatchSetting(json, "system/boot", ESP3DSettingIndex::esp3d_boot_delay, "bootdelay", nullptr,
                  nullptr, 40000, 0, -1, -1, nullptr, true, target, requestId);

  // Verbose boot
  dispatchSetting(json, "system/boot", ESP3DSettingIndex::esp3d_verbose_boot, "verbose", YesNoValues,
                  YesNoLabels, sizeof(YesNoValues) / sizeof(char*), -1, -1, -1,
                  nullptr, true, target, requestId);

  if (json) {
    if (!dispatch(msg, "]}", target, requestId, ESP3DMessageType::tail, msg->authentication_level)) {
      esp3d_log_e("Error sending response to clients");
    }
  } else {
    if (!dispatch(msg, "ok\n", target, requestId, ESP3DMessageType::tail, msg->authentication_level)) {
      esp3d_log_e("Error sending response to clients");
    }
  }
}