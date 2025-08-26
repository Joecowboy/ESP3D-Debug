/*
 handle-filenotfound.cpp - ESP3D http handle

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
#include "../../../include/esp3d_config.h"
#if defined(HTTP_FEATURE)
#include "../http_server.h"
#include <ESPAsyncWebServer.h>
#include "../../../core/esp3d_string.h"
#include "../../authentication/authentication_service.h"
#include "../../filesystem/esp_filesystem.h"

#if defined(SD_DEVICE)
#include "../../filesystem/esp_sd.h"
#endif  // SD_DEVICE
#include "../favicon.h"

#if defined(ESP3DLIB_ENV) && COMMUNICATION_PROTOCOL == SOCKET_SERIAL
#include "../../serial2socket/serial2socket.h"
#endif  // ESP3DLIB_ENV && COMMUNICATION_PROTOCOL == SOCKET_SERIAL

// Handle not registered path on FS neither SD
void HTTP_Server::handle_not_found(AsyncWebServerRequest *request) {
  set_http_headers(); // Call without request parameter as per http_server.h

  if (AuthenticationService::getAuthenticatedLevel() == ESP3DAuthenticationLevel::guest) {
    request->send(401, "text/plain", "Wrong authentication!");
    return;
  }
  String path = esp3d_string::urlDecode(request->url()); // urlDecode is correct as per esp3d_string.h
  String contentType = esp3d_string::getContentType(path.c_str());
  String pathWithGz = path + ".gz";
  esp3d_log("URI: %s", path.c_str());
#if defined(FILESYSTEM_FEATURE)
  if (ESP_FileSystem::exists(pathWithGz.c_str()) || ESP_FileSystem::exists(path.c_str())) {
    esp3d_log("Path found `%s`", path.c_str());
    if (ESP_FileSystem::exists(pathWithGz.c_str())) {
      request->send(ESP_FileSystem::open(pathWithGz.c_str(), "r"), contentType, true); // Use send with File and gzip encoding
      esp3d_log("Path is gz `%s`", pathWithGz.c_str());
      return;
    }
    if (!StreamFSFile(path.c_str(), contentType.c_str(), request)) {
      esp3d_log_e("Stream `%s` failed", path.c_str());
      request->send(500, "text/plain", "Stream failed");
    }
    return;
  }
  if (path == "/favicon.ico" || path == "favicon.ico") {
    request->send_P(200, "image/x-icon", favicon, favicon_size); // Cast removed as favicon is uint8_t*
    return;
  }
#endif  // FILESYSTEM_FEATURE

#if defined(SD_DEVICE)
  if (path.startsWith("/sd/")) {
    path = path.substring(3);
    pathWithGz = path + ".gz";
    if (ESP_SD::accessFS()) {
      if (ESP_SD::getState(true) != ESP_SDCARD_NOT_PRESENT) {
        ESP_SD::setState(ESP_SDCARD_BUSY);
        if (ESP_SD::exists(pathWithGz.c_str()) || ESP_SD::exists(path.c_str())) {
          if (ESP_SD::exists(pathWithGz.c_str())) {
            request->send(ESP_SD::open(pathWithGz.c_str(), "r"), contentType, true); // Use send with File and gzip encoding
            path = pathWithGz;
          } else {
            request->send(ESP_SD::open(path.c_str(), "r"), contentType);
          }
#if defined(ESP3DLIB_ENV) && COMMUNICATION_PROTOCOL == SOCKET_SERIAL
          Serial2Socket.pause();
#endif  // ESP3DLIB_ENV && COMMUNICATION_PROTOCOL == SOCKET_SERIAL
          if (!StreamSDFile(path.c_str(), contentType.c_str(), request)) {
            esp3d_log_e("Stream `%s` failed", path.c_str());
            request->send(500, "text/plain", "Stream failed");
          }
#if defined(ESP3DLIB_ENV) && COMMUNICATION_PROTOCOL == SOCKET_SERIAL
          Serial2Socket.pause(false);
#endif  // ESP3DLIB_ENV && COMMUNICATION_PROTOCOL == SOCKET_SERIAL
          ESP_SD::releaseFS();
          return;
        }
      }
      ESP_SD::releaseFS();
    }
  }
#endif  // SD_DEVICE

#ifdef FILESYSTEM_FEATURE
  // Check local 404 page
  path = "/404.htm";
  contentType = esp3d_string::getContentType(path.c_str());
  pathWithGz = path + ".gz";
  if (ESP_FileSystem::exists(pathWithGz.c_str()) || ESP_FileSystem::exists(path.c_str())) {
    if (ESP_FileSystem::exists(pathWithGz.c_str())) {
      request->send(ESP_FileSystem::open(pathWithGz.c_str(), "r"), contentType, true); // Use send with File and gzip encoding
      path = pathWithGz;
    } else {
      if (!StreamFSFile(path.c_str(), contentType.c_str(), request)) {
        esp3d_log_e("Stream `%s` failed", path.c_str());
        request->send(500, "text/plain", "Stream failed");
      }
    }
    return;
  }
#endif  // FILESYSTEM_FEATURE
  // Keep it simple, just send 404
  request->send(404, "text/plain", "Not found");
}
#endif  // HTTP_FEATURE