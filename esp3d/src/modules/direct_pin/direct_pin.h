/*
  direct_pin.h - Direct pin control for ESP3D
  Copyright (c) 2014 Luc Lebosse. All rights reserved.
  Licensed under GNU Lesser General Public License v2.1
*/
#ifndef _DIRECT_PIN_H
#define _DIRECT_PIN_H

#include "../../include/esp3d_config.h"
#include "../plugin/esp3d_plugin.h"
#if defined(DIRECT_PIN_FEATURE)
#include <Arduino.h>

class DirectPin : public ESP3DPlugin {
public:
  bool begin() override;
  bool end() override;
  bool isEnabled() override;
  void setEnabled(bool enabled) override;
  const char* getName() override { return "DirectPin"; }
  bool handleCommand(const char* cmd, const char* args) override;
  static bool setPinState(uint8_t pin, uint8_t state);
  static uint8_t getPinState(uint8_t pin);
private:
  bool enabled_;
};

#endif // DIRECT_PIN_FEATURE
#endif // _DIRECT_PIN_H