/*
  esp3d_plugin_manager.h - Plugin manager for ESP3D
  Copyright (c) 2025. All rights reserved.
  Licensed under GNU Lesser General Public License v2.1
*/
#ifndef _ESP3D_PLUGIN_MANAGER_H
#define _ESP3D_PLUGIN_MANAGER_H

#include "../../include/esp3d_config.h"
#include "../../core/esp3d_settings.h"
#include "../../core/esp3d_string.h"
#include "esp3d_plugin.h"
#include <vector>

class ESP3DPluginManager {
public:
  static bool begin();
  static bool registerPlugin(ESP3DPlugin* plugin);
  static bool handleCommand(const char* cmd, const char* args);
  static void listPlugins(String& response);
  static bool enablePlugin(const char* name, bool enable);
private:
  static std::vector<ESP3DPlugin*> plugins_;
};

#endif // _ESP3D_PLUGIN_MANAGER_H