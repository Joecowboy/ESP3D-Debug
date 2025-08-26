/*
  esp3d_plugin.h - Plugin interface for ESP3D
  Copyright (c) 2025. All rights reserved.
  Licensed under GNU Lesser General Public License v2.1
*/
#ifndef _ESP3D_PLUGIN_H
#define _ESP3D_PLUGIN_H

#include "../../include/esp3d_config.h"
// Use absolute path from src root to ensure compatibility
#include "../../core/esp3d_log.h"

class ESP3DPlugin {
public:
  virtual bool begin() = 0;
  virtual bool end() = 0;
  virtual bool isEnabled() = 0;
  virtual void setEnabled(bool enabled) = 0;
  virtual const char* getName() = 0;
  virtual bool handleCommand(const char* cmd, const char* args) = 0;
};

#endif // _ESP3D_PLUGIN_H