/*
 upload-updatefw.cpp - ESP3D http handle

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
#if defined(HTTP_FEATURE) && defined(WEB_UPDATE_FEATURE)
#include "../http_server.h"
#include <ESPAsyncWebServer.h>
#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#include <Updater.h>
#define UPDATE_CLASS Updater
#elif defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#include <Update.h>
#define UPDATE_CLASS Update
#else
#error "Unsupported platform: Neither ESP8266 nor ESP32 defined"
#endif
#include "../../../core/esp3d_commands.h"
#include "../../authentication/authentication_service.h"
#include "../../filesystem/esp_filesystem.h"

#if defined(ESP3DLIB_ENV) && COMMUNICATION_PROTOCOL == SOCKET_SERIAL
#include "../../serial2socket/serial2socket.h"
#endif  // ESP3DLIB_ENV && COMMUNICATION_PROTOCOL == SOCKET_SERIAL

// File upload for Web update
void HTTP_Server::WebUpdateUpload(AsyncWebServerRequest *request) {
  static size_t last_upload_update;
  static uint32_t downloadsize = 0;

  if (AuthenticationService::getAuthenticatedLevel() != ESP3DAuthenticationLevel::admin) {
    _upload_status = UPLOAD_STATUS_FAILED;
    pushError(ESP_ERROR_AUTHENTICATION, "Upload rejected", 401);
    esp3d_commands.dispatch("Update rejected!", ESP3DClientType::remote_screen,
                            no_id, ESP3DMessageType::unique, ESP3DClientType::system,
                            ESP3DAuthenticationLevel::admin);
    request->send(401, "text/plain", "Upload rejected");
    return;
  }
  AsyncWebServerUpload &upload = request->getUpload();
  if (_upload_status != UPLOAD_STATUS_FAILED || upload.status == UPLOAD_FILE_START) {
#if defined(ESP3DLIB_ENV) && COMMUNICATION_PROTOCOL == SOCKET_SERIAL
    Serial2Socket.pause();
#endif  // ESP3DLIB_ENV && COMMUNICATION_PROTOCOL == SOCKET_SERIAL
    if (upload.status == UPLOAD_FILE_START) {
      esp3d_commands.dispatch("Update Firmware", ESP3DClientType::remote_screen, no_id,
                              ESP3DMessageType::unique, ESP3DClientType::system,
                              ESP3DAuthenticationLevel::admin);
      _upload_status = UPLOAD_STATUS_ONGOING;
      String sizeargname = upload.filename + "S";
      downloadsize = request->hasParam(sizeargname.c_str()) ? request->getParam(sizeargname)->value().toInt() : 0;
      if (downloadsize > ESP_FileSystem::max_update_size()) {
        _upload_status = UPLOAD_STATUS_FAILED;
        esp3d_commands.dispatch("Update rejected", ESP3DClientType::remote_screen, no_id,
                                ESP3DMessageType::unique, ESP3DClientType::system,
                                ESP3DAuthenticationLevel::admin);
        pushError(ESP_ERROR_NOT_ENOUGH_SPACE, "Upload rejected");
        request->send(500, "text/plain", "Upload rejected: not enough space");
        return;
      }
      last_upload_update = 0;
      if (_upload_status != UPLOAD_STATUS_FAILED) {
        if (!UPDATE_CLASS.begin(downloadsize ? downloadsize : ESP_FileSystem::max_update_size(), U_FLASH)) {
          _upload_status = UPLOAD_STATUS_FAILED;
          esp3d_commands.dispatch("Update rejected", ESP3DClientType::remote_screen, no_id,
                                  ESP3DMessageType::unique, ESP3DClientType::system,
                                  ESP3DAuthenticationLevel::admin);
          pushError(ESP_ERROR_NOT_ENOUGH_SPACE, "Upload rejected");
          request->send(500, "text/plain", "Upload rejected: cannot start update");
        } else {
          esp3d_commands.dispatch("Update 0%", ESP3DClientType::remote_screen,
                                  no_id, ESP3DMessageType::unique, ESP3DClientType::system,
                                  ESP3DAuthenticationLevel::admin);
        }
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (_upload_status == UPLOAD_STATUS_ONGOING) {
        if (downloadsize != 0) {
          size_t progress = (100 * upload.totalSize) / downloadsize;
          if (progress != last_upload_update) {
            last_upload_update = progress;
            String s = "Update " + String(last_upload_update) + "%";
            esp3d_commands.dispatch(s.c_str(), ESP3DClientType::remote_screen,
                                    no_id, ESP3DMessageType::unique, ESP3DClientType::system,
                                    ESP3DAuthenticationLevel::admin);
          }
        }
        if (UPDATE_CLASS.write(upload.buf, upload.currentSize) != upload.currentSize) {
          _upload_status = UPLOAD_STATUS_FAILED;
          esp3d_commands.dispatch("Update write failed", ESP3DClientType::remote_screen, no_id,
                                  ESP3DMessageType::unique, ESP3DClientType::system,
                                  ESP3DAuthenticationLevel::admin);
          pushError(ESP_ERROR_FILE_WRITE, "File write failed");
        }
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (_upload_status == UPLOAD_STATUS_ONGOING) {
        if (downloadsize != 0 && downloadsize != upload.totalSize) {
          _upload_status = UPLOAD_STATUS_FAILED;
          esp3d_commands.dispatch("Update size mismatch", ESP3DClientType::remote_screen, no_id,
                                  ESP3DMessageType::unique, ESP3DClientType::system,
                                  ESP3DAuthenticationLevel::admin);
          pushError(ESP_ERROR_SIZE, "Update size mismatch");
          request->send(500, "text/plain", "Update failed: size mismatch");
        } else if (UPDATE_CLASS.end(true)) {
          _upload_status = UPLOAD_STATUS_SUCCESSFUL;
          esp3d_commands.dispatch("Update successful, restarting...", ESP3DClientType::remote_screen,
                                  no_id, ESP3DMessageType::unique, ESP3DClientType::system,
                                  ESP3DAuthenticationLevel::admin);
          request->send(200, "text/plain", "Update successful, restarting...");
          ESP3DHal::wait(1000); // Allow time for response to be sent
#if defined(ARDUINO_ARCH_ESP8266)
          ESP.reset();
#elif defined(ARDUINO_ARCH_ESP32)
          ESP.restart();
#endif
        } else {
          _upload_status = UPLOAD_STATUS_FAILED;
          esp3d_commands.dispatch("Update finalization failed", ESP3DClientType::remote_screen, no_id,
                                  ESP3DMessageType::unique, ESP3DClientType::system,
                                  ESP3DAuthenticationLevel::admin);
          pushError(ESP_ERROR_UPDATE, "Update finalization failed");
          request->send(500, "text/plain", "Update failed: finalization error");
        }
      } else {
        _upload_status = UPLOAD_STATUS_FAILED;
        esp3d_commands.dispatch("Update failed", ESP3DClientType::remote_screen, no_id,
                                ESP3DMessageType::unique, ESP3DClientType::system,
                                ESP3DAuthenticationLevel::admin);
        pushError(ESP_ERROR_UPDATE, "Update failed");
        request->send(500, "text/plain", "Update failed");
      }
#if defined(ESP3DLIB_ENV) && COMMUNICATION_PROTOCOL == SOCKET_SERIAL
      Serial2Socket.pause(false);
#endif  // ESP3DLIB_ENV && COMMUNICATION_PROTOCOL == SOCKET_SERIAL
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
      _upload_status = UPLOAD_STATUS_CANCELLED;
      UPDATE_CLASS.abort();
      esp3d_commands.dispatch("Update cancelled", ESP3DClientType::remote_screen, no_id,
                              ESP3DMessageType::unique, ESP3DClientType::system,
                              ESP3DAuthenticationLevel::admin);
      pushError(ESP_ERROR_UPLOAD_CANCELLED, "Update cancelled");
      request->send(500, "text/plain", "Update cancelled");
#if defined(ESP3DLIB_ENV) && COMMUNICATION_PROTOCOL == SOCKET_SERIAL
      Serial2Socket.pause(false);
#endif  // ESP3DLIB_ENV && COMMUNICATION_PROTOCOL == SOCKET_SERIAL
    }
  }
}
#endif  // HTTP_FEATURE && WEB_UPDATE_FEATURE