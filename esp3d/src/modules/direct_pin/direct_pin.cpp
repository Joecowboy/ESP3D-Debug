/*
  direct_pin.cpp - Direct pin control implementation for ESP3D
  Copyright (c) 2014 Luc Lebosse. All rights reserved.
  Licensed under GNU Lesser General Public License v2.1
*/
#include "direct_pin.h"
#include "../../core/esp3d_log.h"
#include "../../core/esp3d_settings.h"
#include "../../core/esp3d_commands.h"
#include "../../core/esp3d_string.h"

#if defined(DIRECT_PIN_FEATURE)
bool DirectPin::begin() {
  esp3d_log("Initializing DirectPin plugin, free heap: %u", ESP.getFreeHeap());
  enabled_ = ESP3DSettings::readByte(static_cast<int>(ESP3DSettingIndex::esp3d_plugin_direct_pin_enabled));
  return true;
}

bool DirectPin::end() {
  esp3d_log("Shutting down DirectPin plugin");
  enabled_ = false;
  return true;
}

bool DirectPin::isEnabled() {
  return enabled_;
}

void DirectPin::setEnabled(bool enabled) {
  enabled_ = enabled;
  ESP3DSettings::writeByte(static_cast<int>(ESP3DSettingIndex::esp3d_plugin_direct_pin_enabled), enabled);
  esp3d_log("DirectPin plugin %s", enabled ? "enabled" : "disabled");
}

bool DirectPin::handleCommand(const char* cmd, const char* args) {
  if (!enabled_) return false;
  if (strcmp(cmd, "800") == 0) { // [ESP800] for GPIO control
    String argsStr = args;
    int pinIdx = argsStr.indexOf("pin=");
    int stateIdx = argsStr.indexOf("state=");
    int readIdx = argsStr.indexOf("read=");
    if (pinIdx != -1) {
      String pinStr = argsStr.substring(pinIdx + 4, argsStr.indexOf('&', pinIdx));
      uint8_t pin = pinStr.toInt();
      if (stateIdx != -1) {
        uint8_t state = argsStr.substring(stateIdx + 6, argsStr.indexOf('&', stateIdx)).toInt();
        if (setPinState(pin, state)) {
          esp3d_commands.dispatch("Pin set successfully", ESP3DClientType::all_clients, 0, ESP3DMessageType::unique, ESP3DAuthenticationLevel::admin);
          return true;
        }
      } else if (readIdx != -1) {
        uint8_t state = getPinState(pin);
        String response = "Pin " + String(pin) + " state: " + String(state);
        esp3d_commands.dispatch(response.c_str(), ESP3DClientType::all_clients, 0, ESP3DMessageType::unique, ESP3DAuthenticationLevel::admin);
        return true;
      }
    }
    esp3d_commands.dispatch("Invalid GPIO command", ESP3DClientType::all_clients, 0, ESP3DMessageType::unique, ESP3DAuthenticationLevel::admin);
    return true;
  }
  return false;
}

bool DirectPin::setPinState(uint8_t pin, uint8_t state) {
  esp3d_log("Setting pin %d to state %d", pin, state);
#if defined(ESP8266) || defined(ESP32)
  pinMode(pin, OUTPUT);
  digitalWrite(pin, state);
  return true;
#else
  esp3d_log_e("Direct pin control not supported on this platform");
  return false;
#endif
}

uint8_t DirectPin::getPinState(uint8_t pin) {
  esp3d_log("Reading state of pin %d", pin);
#if defined(ESP8266) || defined(ESP32)
  pinMode(pin, INPUT);
  return digitalRead(pin);
#else
  esp3d_log_e("Direct pin control not supported on this platform");
  return 0;
#endif
}
#endif // DIRECT_PIN_FEATURE