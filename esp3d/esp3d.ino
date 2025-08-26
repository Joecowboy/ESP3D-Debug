/*
  esp3d.ino - Main ESP3D firmware file

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
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include <Arduino.h>
#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#endif
#include "src/include/esp3d_config.h"
#include "src/core/esp3d_settings.h"
#include "src/core/esp3d_commands.h"
#include "src/core/esp3d_log.h"
#include "src/modules/http/http_server.h"
#include "src/modules/plugin/esp3d_plugin_manager.h"
#include "src/modules/direct_pin/direct_pin.h"
#include "src/modules/ftp/FtpServer.h"

FtpServer ftp_server; // Instantiate FtpServer here

void setup() {
  Serial.begin(115200);
  Serial.println("ESP3D: Starting setup...");
  Serial.printf("Free heap before begin: %d bytes\n", ESP.getFreeHeap());
  ESP3DSettings::begin();
  esp3d_commands.begin();
  HTTP_Server::begin();
  ESP3DPluginManager::begin();
  ftp_server.begin(); // Start FTP server
#if defined(DIRECT_PIN_FEATURE)
  DirectPin* directPinPlugin = new DirectPin();
  ESP3DPluginManager::registerPlugin(directPinPlugin);
#endif
  Serial.printf("Free heap after begin: %d bytes\n", ESP.getFreeHeap());
  Serial.println("ESP3D: Setup complete");
}

void loop() {
  HTTP_Server::handle();
  esp3d_commands.handle();
  ftp_server.handle(); // Handle FTP server
  static unsigned long lastHeapCheck = 0;
  if (millis() - lastHeapCheck > 30000) {
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    lastHeapCheck = millis();
  }
}