/*
    This file is part of ESP3D Firmware for 3D printer.

    ESP3D Firmware for 3D printer is free software: you can redistribute it
    and/or modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation, either version 3 of the License,
    or (at your option) any later version.

    ESP3D Firmware for 3D printer is distributed in the hope that it will be
    useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this Firmware.  If not, see <http://www.gnu.org/licenses/>.

    This firmware is using the standard arduino IDE with module to support
    ESP8266/ESP32: https://github.com/esp8266/Arduino
    https://github.com/espressif/arduino-esp32

    Latest version of the code and documentation can be found here :
    https://github.com/luc-github/ESP3D

    Main author: luc lebosse
*/
#include "esp3d.h"

#include "../include/esp3d_config.h"
#include "esp3d_settings.h"
#include "esp3d_commands.h"

#if COMMUNICATION_PROTOCOL != SOCKET_SERIAL || ESP_SERIAL_BRIDGE_OUTPUT
#include "../modules/serial/serial_service.h"
#endif  // COMMUNICATION_PROTOCOL != SOCKET_SERIAL
#if COMMUNICATION_PROTOCOL == SOCKET_SERIAL
#include "../modules/serial2socket/serial2socket.h"
#endif  // COMMUNICATION_PROTOCOL == SOCKET_SERIAL
#if defined(WIFI_FEATURE) || defined(ETH_FEATURE)
#include "../modules/network/netconfig.h"
#endif  // WIFI_FEATURE || ETH_FEATURE
#if defined(FILESYSTEM_FEATURE)
#include "../modules/filesystem/esp_filesystem.h"
#endif  // FILESYSTEM_FEATURE
#ifdef CONNECTED_DEVICES_FEATURE
#include "../modules/devices/devices_services.h"
#endif  // CONNECTED_DEVICES_FEATURE
#ifdef DISPLAY_DEVICE
#include "../modules/display/display.h"
#endif  // DISPLAY_DEVICE
#ifdef GCODE_HOST_FEATURE
#include "../modules/gcode_host/gcode_host.h"
#endif  // GCODE_HOST_FEATURE
#ifdef SD_UPDATE_FEATURE
#include "../modules/update/update_service.h"
#endif  // SD_UPDATE_FEATURE
#include "../modules/boot_delay/boot_delay.h"
#include "esp3d_message.h"
#ifdef ESP_LUA_INTERPRETER_FEATURE
#include "../modules/lua_interpreter/lua_interpreter_service.h"
#endif  // ESP_LUA_INTERPRETER_FEATURE
#if defined(USB_SERIAL_FEATURE)
#include "../modules/usb-serial/usb_serial_service.h"
#endif  // USB_SERIAL_FEATURE

bool Esp3D::restart = false;

// Constructor
Esp3D::Esp3D() { _started = false; }

// Destructor
Esp3D::~Esp3D() { end(); }

// Begin which setup everything
bool Esp3D::begin() {
  esp3d_log("Starting ESP3D");
  BootDelay bd;
  ESP3DHal::begin();
  ESP3D_LOG_INIT_FN
  bool res = true;

#if defined(GCODE_HOST_FEATURE)
  esp3d_log("Starting G-code host");
  if (!esp3d_gcode_host.begin()) {
    esp3d_log_e("Failed to start G-code host");
    res = false;
  }
#endif  // GCODE_HOST_FEATURE

#if COMMUNICATION_PROTOCOL == SOCKET_SERIAL
  esp3d_log("Enabling Serial2Socket");
  Serial2Socket.enable();
#endif  // COMMUNICATION_PROTOCOL == SOCKET_SERIAL

#if defined(CONNECTED_DEVICES_FEATURE)
  esp3d_log("Starting Devices feature");
  if (!DevicesServices::begin()) {
    esp3d_log_e("Error setup connected devices");
    res = false;
  }
#endif  // CONNECTED_DEVICES_FEATURE

  // Delay to avoid disturbing printer
  bd.begin();

#ifdef SD_UPDATE_FEATURE
  esp3d_log("Checking for SD update");
  if (update_service.begin()) {
    esp3d_log("Need restart due to update");
    restart_now();
  }
#endif  // SD_UPDATE_FEATURE

  esp3d_log("Initializing settings");
  if (!ESP3DSettings::begin()) {
    esp3d_log_e("Need reset settings");
    reset();
    restart_now();
  }

  esp3d_commands.getOutputClient(true);

#if defined(USB_SERIAL_FEATURE)
  if (esp3d_commands.getOutputClient() == ESP3DClientType::usb_serial) {
    esp3d_log("Starting USB serial service");
    if (!esp3d_usb_serial_service.begin()) {
      esp3d_log_e("Error with USB serial service");
      res = false;
    }
  }
#endif  // USB_SERIAL_FEATURE

#if COMMUNICATION_PROTOCOL == RAW_SERIAL || COMMUNICATION_PROTOCOL == MKS_SERIAL
  esp3d_log("Starting serial service");
  if (!esp3d_serial_service.begin(ESP_SERIAL_OUTPUT)) {
    esp3d_log_e("Error with serial service");
    res = false;
  }
#endif  // COMMUNICATION_PROTOCOL == RAW_SERIAL || COMMUNICATION_PROTOCOL == MKS_SERIAL

#if defined(ESP_SERIAL_BRIDGE_OUTPUT)
  esp3d_log("Starting serial bridge service");
  if (!serial_bridge_service.begin(ESP_SERIAL_BRIDGE_OUTPUT)) {
    esp3d_log_e("Error with serial bridge service");
    res = false;
  }
#endif  // ESP_SERIAL_BRIDGE_OUTPUT

#if defined(FILESYSTEM_FEATURE)
  esp3d_log("Starting Filesystem feature");
  if (!ESP_FileSystem::begin()) {
    esp3d_log_e("Error with filesystem service");
    res = false;
  }
#endif  // FILESYSTEM_FEATURE

#ifdef DISPLAY_DEVICE
  esp3d_log("Showing main screen");
  esp3d_display.showScreenID(MAIN_SCREEN);
#endif  // DISPLAY_DEVICE

#if defined(WIFI_FEATURE) || defined(ETH_FEATURE) || defined(BLUETOOTH_FEATURE)
  esp3d_log("Starting network, mode: %d", WiFi.getMode());
  if (ESP3DSettings::readByte(static_cast<int>(ESP3DSettingIndex::esp3d_boot_radio_state)) == 1) {
    if (!NetConfig::begin()) {
      esp3d_log_e("Error setup network");
      res = false;
      if (WiFi.getMode() == WIFI_AP_STA && WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
        esp3d_log_e("STA IP not assigned, falling back to AP-only mode");
        WiFi.mode(WIFI_AP);
        WiFi.softAP(ESP3DSettings::readString(static_cast<int>(ESP3DSettingIndex::esp3d_ap_ssid)),
                    ESP3DSettings::readString(static_cast<int>(ESP3DSettingIndex::esp3d_ap_password)));
        esp3d_log("AP IP: %s", WiFi.softAPIP().toString().c_str());
      }
    } else {
      esp3d_log("Network started, AP IP: %s, STA IP: %s",
                WiFi.softAPIP().toString().c_str(),
                WiFi.localIP().toString().c_str());
    }
  } else {
    esp3d_log("Network disabled in settings");
  }
#endif  // WIFI_FEATURE || ETH_FEATURE || BLUETOOTH_FEATURE

#if defined(GCODE_HOST_FEATURE)
#if defined(ESP_AUTOSTART_SCRIPT)
  esp3d_log("Processing autostart script");
  esp3d_gcode_host.processScript(ESP_AUTOSTART_SCRIPT);
#endif  // ESP_AUTOSTART_SCRIPT
#if defined(ESP_AUTOSTART_SCRIPT_FILE)
  esp3d_log("Processing autostart script file");
  esp3d_gcode_host.processFile(ESP_AUTOSTART_SCRIPT_FILE);
#endif  // ESP_AUTOSTART_SCRIPT_FILE
#endif  // GCODE_HOST_FEATURE

  esp3d_log("ESP3D started successfully");
  _started = res;
  return res;
}

// Process which handle all input
void Esp3D::handle() {
  if (!_started) {
    esp3d_log("ESP3D not started, skipping handle");
    return;
  }
  if (restart) {
    esp3d_log("Restart requested");
    restart_now();
  }
  // Periodic heap monitoring
  static unsigned long lastHeapCheck = 0;
  if (millis() - lastHeapCheck > 10000) { // Check every 10 seconds
    esp3d_log("Free heap: %d bytes", ESP.getFreeHeap());
    lastHeapCheck = millis();
  }
#if defined(USB_SERIAL_FEATURE)
  esp3d_usb_serial_service.handle();
#endif  // USB_SERIAL_FEATURE
#if COMMUNICATION_PROTOCOL == RAW_SERIAL || COMMUNICATION_PROTOCOL == MKS_SERIAL
  esp3d_serial_service.handle();
#endif  // COMMUNICATION_PROTOCOL == RAW_SERIAL || COMMUNICATION_PROTOCOL == MKS_SERIAL
#if defined(ESP_SERIAL_BRIDGE_OUTPUT)
  serial_bridge_service.handle();
#endif  // ESP_SERIAL_BRIDGE_OUTPUT
#if COMMUNICATION_PROTOCOL == SOCKET_SERIAL
  Serial2Socket.handle();
#endif  // COMMUNICATION_PROTOCOL == SOCKET_SERIAL
#if defined(WIFI_FEATURE) || defined(ETH_FEATURE)
  NetConfig::handle();
#endif  // WIFI_FEATURE || ETH_FEATURE
#if defined(CONNECTED_DEVICES_FEATURE)
  DevicesServices::handle();
#endif  // CONNECTED_DEVICES_FEATURE
#if defined(GCODE_HOST_FEATURE)
  esp3d_gcode_host.handle();
#endif  // GCODE_HOST_FEATURE
#ifdef ESP_LUA_INTERPRETER_FEATURE
  esp3d_lua_interpreter.handle();
#endif  // ESP_LUA_INTERPRETER_FEATURE
  yield(); // Prevent watchdog resets
}

bool Esp3D::started() { return _started; }

// End ESP3D
bool Esp3D::end() {
  esp3d_log("Stopping ESP3D");
  _started = false;
#if defined(CONNECTED_DEVICES_FEATURE)
  DevicesServices::end();
  esp3d_log("Devices services stopped");
#endif  // CONNECTED_DEVICES_FEATURE
#if defined(ESP_SERIAL_BRIDGE_OUTPUT)
  serial_bridge_service.end();
  esp3d_log("Serial bridge stopped");
#endif  // ESP_SERIAL_BRIDGE_OUTPUT
#if defined(WIFI_FEATURE) || defined(ETH_FEATURE)
  NetConfig::end();
  esp3d_log("Network stopped");
#endif  // WIFI_FEATURE || ETH_FEATURE
#if defined(FILESYSTEM_FEATURE)
  ESP_FileSystem::end();
  esp3d_log("Filesystem stopped");
#endif  // FILESYSTEM_FEATURE
#if defined(USB_SERIAL_FEATURE)
  esp3d_usb_serial_service.end();
  esp3d_log("USB serial stopped");
#endif  // USB_SERIAL_FEATURE
#if COMMUNICATION_PROTOCOL == RAW_SERIAL || COMMUNICATION_PROTOCOL == MKS_SERIAL
  esp3d_serial_service.end();
  esp3d_log("Serial service stopped");
#endif  // COMMUNICATION_PROTOCOL == RAW_SERIAL || COMMUNICATION_PROTOCOL == MKS_SERIAL
  return true;
}

// Reset ESP3D settings
bool Esp3D::reset() {
  bool resetOk = true;
  esp3d_log("Resetting ESP3D settings");
#if COMMUNICATION_PROTOCOL == RAW_SERIAL || COMMUNICATION_PROTOCOL == MKS_SERIAL
  if (!esp3d_serial_service.reset()) {
    resetOk = false;
    esp3d_log_e("Reset serial error");
  }
#endif  // COMMUNICATION_PROTOCOL == RAW_SERIAL || COMMUNICATION_PROTOCOL == MKS_SERIAL
#if defined(ESP_SERIAL_BRIDGE_OUTPUT)
  if (!serial_bridge_service.reset()) {
    resetOk = false;
    esp3d_log_e("Reset serial bridge error");
  }
#endif  // ESP_SERIAL_BRIDGE_OUTPUT
  if (!ESP3DSettings::reset()) {
    esp3d_log_e("Reset settings error");
    resetOk = false;
  }
  return resetOk;
}

// Set Restart flag
void Esp3D::restart_esp(bool need_restart) {
  esp3d_log("Setting restart flag to %d", need_restart);
  restart = need_restart;
}

void Esp3D::restart_now() {
  esp3d_log("Restarting ESP3D");
#if defined(ETH_FEATURE) && defined(ESP3D_ETH_PHY_POWER_PIN)
  digitalWrite(ESP3D_ETH_PHY_POWER_PIN, LOW);
#endif  // ESP3D_ETH_PHY_POWER_PIN
#if COMMUNICATION_PROTOCOL == RAW_SERIAL || COMMUNICATION_PROTOCOL == MKS_SERIAL
  if (!esp3d_serial_service.started()) {
    esp3d_serial_service.begin(ESP_SERIAL_OUTPUT);
  }
  esp3d_serial_service.flush();
#endif  // COMMUNICATION_PROTOCOL == RAW_SERIAL || COMMUNICATION_PROTOCOL == MKS_SERIAL
#if defined(FILESYSTEM_FEATURE)
  ESP_FileSystem::end();
#endif  // FILESYSTEM_FEATURE
#if (COMMUNICATION_PROTOCOL == RAW_SERIAL || COMMUNICATION_PROTOCOL == MKS_SERIAL) && defined(ARDUINO_ARCH_ESP8266)
  esp3d_serial_service.swap();
#endif  // COMMUNICATION_PROTOCOL == RAW_SERIAL || COMMUNICATION_PROTOCOL == MKS_SERIAL
  ESP.restart();
  while (1) {
    delay(1);
  }
}