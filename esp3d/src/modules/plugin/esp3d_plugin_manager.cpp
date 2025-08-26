/*
  esp3d_plugin_manager.cpp - Plugin manager implementation for ESP3D
  Copyright (c) 2025. All rights reserved.
  Licensed under GNU Lesser General Public License v2.1
*/
#include "esp3d_plugin_manager.h"
#include "../../core/esp3d_commands.h"
#include "../http/http_server.h"

std::vector<ESP3DPlugin*> ESP3DPluginManager::plugins_;

bool ESP3DPluginManager::begin() {
  esp3d_log("Initializing plugin manager");
  return true;
}

bool ESP3DPluginManager::registerPlugin(ESP3DPlugin* plugin) {
  if (!plugin) {
    esp3d_log_e("Invalid plugin");
    return false;
  }
  plugins_.push_back(plugin);
  esp3d_log("Registered plugin: %s", plugin->getName());
  return plugin->begin();
}

bool ESP3DPluginManager::handleCommand(const char* cmd, const char* args) {
  if (strcmp(cmd, "900") == 0) { // [ESP900] for plugin management
    if (strncmp(args, "list", 4) == 0) {
      String response;
      listPlugins(response);
      esp3d_commands.dispatch(nullptr, response.c_str(), ESP3DClientType::command, ESP3DRequest::no_request, ESP3DMessageType::unique, ESP3DAuthenticationLevel::guest);
      return true;
    } else if (strncmp(args, "enable=", 7) == 0) {
      String name = args + 7;
      int sep = name.indexOf('&');
      bool enable = (sep != -1 && name.substring(sep + 1) == "1");
      if (sep != -1) name = name.substring(0, sep);
      if (enablePlugin(name.c_str(), enable)) {
        esp3d_commands.dispatch(nullptr, "Plugin state updated", ESP3DClientType::command, ESP3DRequest::no_request, ESP3DMessageType::unique, ESP3DAuthenticationLevel::guest);
      } else {
        esp3d_commands.dispatch(nullptr, "Failed to update plugin state", ESP3DClientType::command, ESP3DRequest::no_request, ESP3DMessageType::unique, ESP3DAuthenticationLevel::guest);
      }
      return true;
    }
  }
  for (auto plugin : plugins_) {
    if (plugin->isEnabled() && plugin->handleCommand(cmd, args)) {
      return true;
    }
  }
  return false;
}

void ESP3DPluginManager::listPlugins(String& response) {
  response = "[";
  for (size_t i = 0; i < plugins_.size(); i++) {
    response += "{\"name\":\"";
    response += plugins_[i]->getName();
    response += "\",\"enabled\":";
    response += plugins_[i]->isEnabled() ? "true" : "false";
    response += "}";
    if (i < plugins_.size() - 1) response += ",";
  }
  response += "]";
}

bool ESP3DPluginManager::enablePlugin(const char* name, bool enable) {
  for (auto plugin : plugins_) {
    if (strcmp(plugin->getName(), name) == 0) {
      plugin->setEnabled(enable);
      return true;
    }
  }
  esp3d_log_e("Plugin %s not found", name);
  return false;
}