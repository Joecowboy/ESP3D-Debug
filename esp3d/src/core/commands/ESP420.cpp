/*/*
 ESP420.cpp - ESP3D command class

 Copyright (c) 2014 Luc Lebosse. All rights reserved.

 This code is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This code is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with This code; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "../../include/esp3d_config.h"
#include "../../modules/authentication/authentication_service.h"
#include "../esp3d_commands.h"
#include "../esp3d_settings.h"
#include "../esp3d_string.h"
#include "../../core/esp3d_log.h" // Added for enhanced logging

#if COMMUNICATION_PROTOCOL != SOCKET_SERIAL || defined(ESP_SERIAL_BRIDGE_OUTPUT)
#include "../../modules/serial/serial_service.h"
#endif  // COMMUNICATION_PROTOCOL != SOCKET_SERIAL
#ifdef FILESYSTEM_FEATURE
#include "../../modules/filesystem/esp_filesystem.h"
#endif  // FILESYSTEM_FEATURE
#if defined(WIFI_FEATURE) || defined(ETH_FEATURE) || defined(BLUETOOTH_FEATURE)
#include "../../modules/network/netconfig.h"
#if defined(WIFI_FEATURE)
#include "../../modules/wifi/wificonfig.h"
#endif  // WIFI_FEATURE
#if defined(ETH_FEATURE)
#include "../../modules/ethernet/ethconfig.h"
#endif  // ETH_FEATURE
#if defined(BLUETOOTH_FEATURE)
#include "../../modules/bluetooth/BT_service.h"
#endif  // BLUETOOTH_FEATURE
#endif  // WIFI_FEATURE || ETH_FEATURE || BLUETOOTH_FEATURE
#ifdef HTTP_FEATURE
#include "../../modules/http/http_server.h"
#endif  // HTTP_FEATURE
#ifdef TELNET_FEATURE
#include "../../modules/telnet/telnet_server.h"
#endif  // TELNET_FEATURE
#ifdef FTP_FEATURE
#include "../../modules/ftp/FtpServer.h"
#endif  // FTP_FEATURE
#ifdef WS_DATA_FEATURE
#include "../../modules/websocket/websocket_server.h"
#endif  // WS_DATA_FEATURE
#ifdef WEBDAV_FEATURE
#include "../../modules/webdav/webdav_server.h"
#endif  // WEBDAV_FEATURE
#ifdef TIMESTAMP_FEATURE
#include "../../modules/time/time_service.h"
#endif  // TIMESTAMP_FEATURE
#if defined(SENSOR_DEVICE)
#include "../../modules/sensor/sensor.h"
#endif  // SENSOR_DEVICE
#ifdef NOTIFICATION_FEATURE
#include "../../modules/notifications/notifications_service.h"
#endif  // NOTIFICATION_FEATURE
#ifdef BUZZER_DEVICE
#include "../../modules/buzzer/buzzer.h"
#endif  // BUZZER_DEVICE
#ifdef CAMERA_DEVICE
#include "../../modules/camera/camera.h"
#endif  // CAMERA_DEVICE
#ifdef SD_DEVICE
#include "../../modules/filesystem/esp_sd.h"
#endif  // SD_DEVICE
#if defined(DISPLAY_DEVICE)
#include "../../modules/display/display.h"
#endif  // DISPLAY_DEVICE
#if defined(AUTHENTICATION_FEATURE)
#include "../../modules/authentication/authentication_service.h"
#endif  // AUTHENTICATION_FEATURE
#if defined(SSDP_FEATURE) && defined(ARDUINO_ARCH_ESP32)
#include <ESP32SSDP.h>
#endif  // SSDP_FEATURE
#if defined(MDNS_FEATURE)
#include "../../modules/mDNS/mDNS.h"
#endif  // MDNS_FEATURE
#if defined(USB_SERIAL_FEATURE)
#include "../../modules/usb-serial/usb_serial_service.h"
#endif  // USB_SERIAL_FEATURE

#define COMMAND_ID 420

#if defined(ESP_SAVE_SETTINGS)
// Get ESP current status
// output is JSON or plain text according to parameter
// [ESP420]json=<no>
void ESP3DCommands::ESP420(int cmd_params_pos, ESP3DMessage* msg) {
  ESP3DClientType target = msg->origin;
  ESP3DRequest requestId = msg->request_id;
  msg->target = target;
  msg->origin = ESP3DClientType::command;
  bool json = hasTag(msg, cmd_params_pos, "json");
  bool addPreTag = hasTag(msg, cmd_params_pos, "addPreTag");
  String tmpstr;

  esp3d_log("Processing [ESP420] command, json=%d, addPreTag=%d", json, addPreTag);

#if defined(AUTHENTICATION_FEATURE)
  if (msg->authentication_level == ESP3DAuthenticationLevel::guest) {
    esp3d_log_e("Authentication error for [ESP420]");
    dispatchAuthenticationError(msg, COMMAND_ID, json);
    return;
  }
#endif  // AUTHENTICATION_FEATURE

  if (json) {
    tmpstr = "{\"cmd\":\"420\",\"status\":\"ok\",\"data\":[";
  } else {
    if (addPreTag) {
      tmpstr = "<pre>\n";
    } else {
      tmpstr = "Configuration:\n";
    }
  }
  msg->type = ESP3DMessageType::head;
  if (!dispatch(msg, tmpstr.c_str())) {
    esp3d_log_e("Error sending response header to clients");
    return;
  }

  // Chip ID
  tmpstr = ESP3DHal::getChipID();
  esp3d_log("Chip ID: %s", tmpstr.c_str());
  if (!dispatchIdValue(json, "chip id", tmpstr.c_str(), target, requestId, true)) {
    esp3d_log_e("Error dispatching chip id");
    return;
  }

  // CPU Freq
  tmpstr = String(ESP.getCpuFreqMHz()) + "Mhz";
  esp3d_log("CPU Freq: %s", tmpstr.c_str());
  if (!dispatchIdValue(json, "CPU Freq", tmpstr.c_str(), target, requestId, false)) {
    esp3d_log_e("Error dispatching CPU Freq");
    return;
  }

  // CPU Temp
  if (ESP3DHal::has_temperature_sensor()) {
    tmpstr = String(ESP3DHal::temperature(), 1) + "C";
    esp3d_log("CPU Temp: %s", tmpstr.c_str());
    if (!dispatchIdValue(json, "CPU Temp", tmpstr.c_str(), target, requestId, false)) {
      esp3d_log_e("Error dispatching CPU Temp");
      return;
    }
  }

  // Free Memory
  tmpstr = esp3d_string::formatBytes(ESP.getFreeHeap());
#ifdef ARDUINO_ARCH_ESP32
#ifdef BOARD_HAS_PSRAM
  tmpstr += " - PSRAM:" + esp3d_string::formatBytes(ESP.getFreePsram());
#endif  // BOARD_HAS_PSRAM
#endif  // ARDUINO_ARCH_ESP32
  esp3d_log("Free Memory: %s", tmpstr.c_str());
  if (!dispatchIdValue(json, "free mem", tmpstr.c_str(), target, requestId, false)) {
    esp3d_log_e("Error dispatching free mem");
    return;
  }

  // FW architecture
  tmpstr = ESP3DSettings::TargetBoard();
#ifdef ARDUINO_ARCH_ESP32
  tmpstr = String(ESP.getChipModel()) + "-" + String(ESP.getChipRevision()) + "-" + String(ESP.getChipCores()) + "@";
#endif  // ARDUINO_ARCH_ESP32
  esp3d_log("FW arch: %s", tmpstr.c_str());
  if (!dispatchIdValue(json, "FW arch", tmpstr.c_str(), target, requestId, false)) {
    esp3d_log_e("Error dispatching FW arch");
    return;
  }

  // SDK Version
  tmpstr = ESP.getSdkVersion();
  esp3d_log("SDK Version: %s", tmpstr.c_str());
  if (!dispatchIdValue(json, "SDK", tmpstr.c_str(), target, requestId, false)) {
    esp3d_log_e("Error dispatching SDK version");
    return;
  }

  // Arduino Version
  tmpstr = ESP3DHal::arduinoVersion();
  esp3d_log("Arduino Version: %s", tmpstr.c_str());
  if (!dispatchIdValue(json, "Arduino", tmpstr.c_str(), target, requestId, false)) {
    esp3d_log_e("Error dispatching Arduino version");
    return;
  }

  // Flash size
  tmpstr = esp3d_string::formatBytes(ESP.getFlashChipSize());
  esp3d_log("Flash size: %s", tmpstr.c_str());
  if (!dispatchIdValue(json, "flash size", tmpstr.c_str(), target, requestId, false)) {
    esp3d_log_e("Error dispatching flash size");
    return;
  }

#if (defined(WIFI_FEATURE) || defined(ETH_FEATURE)) && (defined(OTA_FEATURE) || defined(WEB_UPDATE_FEATURE))
  // Update space
  tmpstr = esp3d_string::formatBytes(ESP_FileSystem::max_update_size());
  esp3d_log("Size for update: %s", tmpstr.c_str());
  if (!dispatchIdValue(json, "size for update", tmpstr.c_str(), target, requestId, false)) {
    esp3d_log_e("Error dispatching size for update");
    return;
  }
#endif  // WIFI_FEATURE || ETH_FEATURE

#if defined(FILESYSTEM_FEATURE)
  // FileSystem type
  tmpstr = ESP_FileSystem::FilesystemName();
  esp3d_log("FS type: %s", tmpstr.c_str());
  if (!dispatchIdValue(json, "FS type", tmpstr.c_str(), target, requestId, false)) {
    esp3d_log_e("Error dispatching FS type");
    return;
  }
  // FileSystem capacity
  tmpstr = String(esp3d_string::formatBytes(ESP_FileSystem::usedBytes())) + "/" + String(esp3d_string::formatBytes(ESP_FileSystem::totalBytes()));
  esp3d_log("FS usage: %s", tmpstr.c_str());
  if (!dispatchIdValue(json, "FS usage", tmpstr.c_str(), target, requestId, false)) {
    esp3d_log_e("Error dispatching FS usage");
    return;
  }
#endif  // FILESYSTEM_FEATURE

#if defined(USB_SERIAL_FEATURE)
  tmpstr = (esp3d_commands.getOutputClient() == ESP3DClientType::usb_serial) ? "usb port" : 
           (esp3d_commands.getOutputClient() == ESP3DClientType::serial) ? "serial port" : "???";
  esp3d_log("Output: %s", tmpstr.c_str());
  if (!dispatchIdValue(json, "output", tmpstr.c_str(), target, requestId, false)) {
    esp3d_log_e("Error dispatching output");
    return;
  }
  if (esp3d_commands.getOutputClient() == ESP3DClientType::usb_serial) {
    tmpstr = String(esp3d_usb_serial_service.baudRate());
    esp3d_log("USB Serial Baud: %s", tmpstr.c_str());
    if (!dispatchIdValue(json, "baud", tmpstr.c_str(), target, requestId, false)) {
      esp3d_log_e("Error dispatching USB serial baud");
      return;
    }
  }
  if (esp3d_usb_serial_service.isConnected()) {
    tmpstr = esp3d_usb_serial_service.getVIDString() + ":" + esp3d_usb_serial_service.getPIDString();
    esp3d_log("Vid/Pid: %s", tmpstr.c_str());
    if (!dispatchIdValue(json, "Vid/Pid", tmpstr.c_str(), target, requestId, false)) {
      esp3d_log_e("Error dispatching Vid/Pid");
      return;
    }
  }
#endif  // USB_SERIAL_FEATURE

#if COMMUNICATION_PROTOCOL == RAW_SERIAL || COMMUNICATION_PROTOCOL == MKS_SERIAL
  if (esp3d_commands.getOutputClient() == ESP3DClientType::serial) {
    tmpstr = String(esp3d_serial_service.baudRate());
    esp3d_log("Serial Baud: %s", tmpstr.c_str());
    if (!dispatchIdValue(json, "baud", tmpstr.c_str(), target, requestId, false)) {
      esp3d_log_e("Error dispatching serial baud");
      return;
    }
  }
#endif  // COMMUNICATION_PROTOCOL == RAW_SERIAL || MKS_SERIAL

#if defined(WIFI_FEATURE)
  if (WiFi.getMode() != WIFI_OFF) {
    // Sleep mode
    tmpstr = WiFiConfig::getSleepModeString();
    esp3d_log("Sleep mode: %s", tmpstr.c_str());
    if (!dispatchIdValue(json, "sleep mode", tmpstr.c_str(), target, requestId, false)) {
      esp3d_log_e("Error dispatching sleep mode");
      return;
    }
  }
#endif  // WIFI_FEATURE

#if defined(WIFI_FEATURE)
  // Wifi enabled
  tmpstr = (WiFi.getMode() == WIFI_OFF) ? "OFF" : "ON";
  esp3d_log("WiFi enabled: %s", tmpstr.c_str());
  if (!dispatchIdValue(json, "wifi", tmpstr.c_str(), target, requestId, false)) {
    esp3d_log_e("Error dispatching WiFi enabled status");
    return;
  }
#endif  // WIFI_FEATURE

#if defined(ETH_FEATURE)
  // Ethernet enabled
  tmpstr = (EthConfig::started()) ? "ON" : "OFF";
  esp3d_log("Ethernet enabled: %s", tmpstr.c_str());
  if (!dispatchIdValue(json, "ethernet", tmpstr.c_str(), target, requestId, false)) {
    esp3d_log_e("Error dispatching Ethernet enabled status");
    return;
  }
#endif  // ETH_FEATURE

#if defined(BLUETOOTH_FEATURE)
  // BT enabled
  tmpstr = (bt_service.started()) ? "ON" : "OFF";
  esp3d_log("Bluetooth enabled: %s", tmpstr.c_str());
  if (!dispatchIdValue(json, "bt", tmpstr.c_str(), target, requestId, false)) {
    esp3d_log_e("Error dispatching Bluetooth enabled status");
    return;
  }
#endif  // BLUETOOTH_FEATURE

#if defined(WIFI_FEATURE) || defined(ETH_FEATURE)
  // Hostname
  tmpstr = NetConfig::hostname();
  esp3d_log("Hostname: %s", tmpstr.c_str());
  if (!dispatchIdValue(json, "hostname", tmpstr.c_str(), target, requestId, false)) {
    esp3d_log_e("Error dispatching hostname");
    return;
  }

  #if defined(SSDP_FEATURE)
  // SSDP enabled
  #if defined(ARDUINO_ARCH_ESP32)
  tmpstr = SSDP.started() ? "ON (" + String(SSDP.localIP().toString()) + ")" : "OFF";
  #endif  // ARDUINO_ARCH_ESP32
  #if defined(ARDUINO_ARCH_ESP8266)
  tmpstr = "ON";
  #endif  // ARDUINO_ARCH_ESP8266
  esp3d_log("SSDP: %s", tmpstr.c_str());
  if (!dispatchIdValue(json, "SSDP", tmpstr.c_str(), target, requestId, false)) {
    esp3d_log_e("Error dispatching SSDP status");
    return;
  }
  #endif  // SSDP_FEATURE

  #if defined(MDNS_FEATURE)
  // MDNS enabled
  tmpstr = esp3d_mDNS.started() ? "ON" : "OFF";
  esp3d_log("MDNS: %s", tmpstr.c_str());
  if (!dispatchIdValue(json, "MDNS", tmpstr.c_str(), target, requestId, false)) {
    esp3d_log_e("Error dispatching MDNS status");
    return;
  }
  #endif  // MDNS_FEATURE
#endif  // WIFI_FEATURE || ETH_FEATURE

#if defined(HTTP_FEATURE)
  if (HTTP_Server::started()) {
    // HTTP port
    tmpstr = String(HTTP_Server::port());
    esp3d_log("HTTP port: %s", tmpstr.c_str());
    if (!dispatchIdValue(json, "HTTP port", tmpstr.c_str(), target, requestId, false)) {
      esp3d_log_e("Error dispatching HTTP port");
      return;
    }
    // Check HTTP accessibility on STA interface
    #if defined(WIFI_FEATURE)
    if (WiFi.getMode() == WIFI_AP_STA && WiFi.isConnected()) {
      tmpstr = WiFi.localIP().toString() + ":" + String(HTTP_Server::port());
      esp3d_log("HTTP STA accessibility: %s", tmpstr.c_str());
      if (!dispatchIdValue(json, "HTTP STA address", tmpstr.c_str(), target, requestId, false)) {
        esp3d_log_e("Error dispatching HTTP STA address");
        return;
      }
    }
    #endif  // WIFI_FEATURE
  }
#endif  // HTTP_FEATURE

#if defined(TELNET_FEATURE)
  if (telnet_server.started()) {
    // Telnet port
    tmpstr = String(telnet_server.port());
    esp3d_log("Telnet port: %s", tmpstr.c_str());
    if (!dispatchIdValue(json, "Telnet port", tmpstr.c_str(), target, requestId, false)) {
      esp3d_log_e("Error dispatching Telnet port");
      return;
    }
    // Check Telnet accessibility on STA interface
    #if defined(WIFI_FEATURE)
    if (WiFi.getMode() == WIFI_AP_STA && WiFi.isConnected()) {
      tmpstr = WiFi.localIP().toString() + ":" + String(telnet_server.port());
      esp3d_log("Telnet STA accessibility: %s", tmpstr.c_str());
      if (!dispatchIdValue(json, "Telnet STA address", tmpstr.c_str(), target, requestId, false)) {
        esp3d_log_e("Error dispatching Telnet STA address");
        return;
      }
    }
    #endif  // WIFI_FEATURE
    if (telnet_server.isConnected()) {
      tmpstr = telnet_server.clientIPAddress();
      esp3d_log("Telnet client IP: %s", tmpstr.c_str());
      if (!dispatchIdValue(json, "Telnet Client", tmpstr.c_str(), target, requestId, false)) {
        esp3d_log_e("Error dispatching Telnet client IP");
        return;
      }
    }
#endif  // TELNET_FEATURE

#if defined(WEBDAV_FEATURE)
  if (webdav_server.started()) {
    // WebDav port
    tmpstr = String(webdav_server.port());
    esp3d_log("WebDav port: %s", tmpstr.c_str());
    if (!dispatchIdValue(json, "WebDav port", tmpstr.c_str(), target, requestId, false)) {
      esp3d_log_e("Error dispatching WebDav port");
      return;
    }
    // Check WebDav accessibility on STA interface
    #if defined(WIFI_FEATURE)
    if (WiFi.getMode() == WIFI_AP_STA && WiFi.isConnected()) {
      tmpstr = WiFi.localIP().toString() + ":" + String(webdav_server.port());
      esp3d_log("WebDav STA accessibility: %s", tmpstr.c_str());
      if (!dispatchIdValue(json, "WebDav STA address", tmpstr.c_str(), target, requestId, false)) {
        esp3d_log_e("Error dispatching WebDav STA address");
        return;
      }
    }
    #endif  // WIFI_FEATURE
  }
#endif  // WEBDAV_FEATURE

#if defined(FTP_FEATURE)
  if (ftp_server.started()) {
    // FTP ports
    tmpstr = String(ftp_server.ctrlport()) + "," + String(ftp_server.dataactiveport()) + "," + String(ftp_server.datapassiveport());
    esp3d_log("FTP ports: %s", tmpstr.c_str());
    if (!dispatchIdValue(json, "Ftp ports", tmpstr.c_str(), target, requestId, false)) {
      esp3d_log_e("Error dispatching FTP ports");
      return;
    }
    // Check FTP accessibility on STA interface
    #if defined(WIFI_FEATURE)
    if (WiFi.getMode() == WIFI_AP_STA && WiFi.isConnected()) {
      tmpstr = WiFi.localIP().toString() + ":" + String(ftp_server.ctrlport());
      esp3d_log("FTP STA accessibility: %s", tmpstr.c_str());
      if (!dispatchIdValue(json, "FTP STA address", tmpstr.c_str(), target, requestId, false)) {
        esp3d_log_e("Error dispatching FTP STA address");
        return;
      }
    }
    #endif  // WIFI_FEATURE
  }
#endif  // FTP_FEATURE

#if defined(WS_DATA_FEATURE)
  if (websocket_data_server.started()) {
    // Websocket port
    tmpstr = String(websocket_data_server.port());
    esp3d_log("Websocket port: %s", tmpstr.c_str());
    if (!dispatchIdValue(json, "Websocket port", tmpstr.c_str(), target, requestId, false)) {
      esp3d_log_e("Error dispatching Websocket port");
      return;
    }
    // Check Websocket accessibility on STA interface
    #if defined(WIFI_FEATURE)
    if (WiFi.getMode() == WIFI_AP_STA && WiFi.isConnected()) {
      tmpstr = WiFi.localIP().toString() + ":" + String(websocket_data_server.port());
      esp3d_log("Websocket STA accessibility: %s", tmpstr.c_str());
      if (!dispatchIdValue(json, "Websocket STA address", tmpstr.c_str(), target, requestId, false)) {
        esp3d_log_e("Error dispatching Websocket STA address");
        return;
      }
    }
    #endif  // WIFI_FEATURE
  }
#endif  // WS_DATA_FEATURE

#if defined(CAMERA_DEVICE)
  if (esp3d_camera.started()) {
    tmpstr = String(esp3d_camera.GetModelString()) + "(" + String(esp3d_camera.GetModel()) + ")";
    esp3d_log("Camera name: %s", tmpstr.c_str());
    if (!dispatchIdValue(json, "camera name", tmpstr.c_str(), target, requestId, false)) {
      esp3d_log_e("Error dispatching camera name");
      return;
    }
  }
#endif  // CAMERA_DEVICE

#if defined(DISPLAY_DEVICE)
  tmpstr = esp3d_display.getModelString();
  esp3d_log("Display: %s", tmpstr.c_str());
  if (!dispatchIdValue(json, "display", tmpstr.c_str(), target, requestId, false)) {
    esp3d_log_e("Error dispatching display");
    return;
  }
#endif  // DISPLAY_DEVICE

#if defined(SENSOR_DEVICE)
  tmpstr = esp3d_sensor.getTypeString();
  esp3d_log("Sensor Type: %s", tmpstr.c_str());
  if (!dispatchIdValue(json, "sensor type", tmpstr.c_str(), target, requestId, false)) {
    esp3d_log_e("Error dispatching sensor type");
    return;
  }
#endif  // SENSOR_DEVICE

#if defined(BUZZER_DEVICE)
  tmpstr = (esp3d_buzzer.started()) ? "ON" : "OFF";
  esp3d_log("Buzzer: %s", tmpstr.c_str());
  if (!dispatchIdValue(json, "buzzer", tmpstr.c_str(), target, requestId, false)) {
    esp3d_log_e("Error dispatching buzzer status");
    return;
  }
#endif  // BUZZER_DEVICE

#if defined(SD_DEVICE)
  tmpstr = (ESP_SD::getState(true) == ESP_SDCARD_NOT_PRESENT) ? "Not present" : "Present";
  esp3d_log("SD Card: %s", tmpstr.c_str());
  if (!dispatchIdValue(json, "sd card", tmpstr.c_str(), target, requestId, false)) {
    esp3d_log_e("Error dispatching SD card status");
    return;
  }
#endif  // SD_DEVICE

#if defined(TIMESTAMP_FEATURE)
  tmpstr = timeService.getCurrentTime();
  esp3d_log("Time: %s", tmpstr.c_str());
  if (!dispatchIdValue(json, "time", tmpstr.c_str(), target, requestId, false)) {
    esp3d_log_e("Error dispatching time");
    return;
  }
#endif  // TIMESTAMP_FEATURE

#if defined(NOTIFICATION_FEATURE)
  tmpstr = notificationsservice.getTypeString();
  esp3d_log("Notification Type: %s", tmpstr.c_str());
  if (!dispatchIdValue(json, "notification type", tmpstr.c_str(), target, requestId, false)) {
    esp3d_log_e("Error dispatching notification type");
    return;
  }
#endif  // NOTIFICATION_FEATURE

  // End of response
  if (json) {
    tmpstr = "]}";
  } else {
    if (addPreTag) {
      tmpstr = "</pre>";
    } else {
      tmpstr = "ok\n";
    }
  }
  esp3d_log("Sending response footer: %s", tmpstr.c_str());
  msg->type = ESP3DMessageType::tail;
  if (!dispatch(msg, tmpstr.c_str())) {
    esp3d_log_e("Error sending response footer to clients");
    return;
  }
}
#endif  // ESP_SAVE_SETTINGS
}