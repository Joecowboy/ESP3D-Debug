/*
 upload-files.cpp - ESP3D http handle

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
#if defined(HTTP_FEATURE) && defined(FILESYSTEM_FEATURE)
#include "../http_server.h"
#include <ESPAsyncWebServer.h>
#include "../../authentication/authentication_service.h"
#include "../../filesystem/esp_filesystem.h"

#if defined(ESP3DLIB_ENV) && COMMUNICATION_PROTOCOL == SOCKET_SERIAL
#include "../../serial2socket/serial2socket.h"
#endif  // ESP3DLIB_ENV && COMMUNICATION_PROTOCOL == SOCKET_SERIAL

#ifdef ESP_BENCHMARK_FEATURE
#include "../../../core/esp3d_benchmark.h"
#endif  // ESP_BENCHMARK_FEATURE

// FS files uploader handle
void HTTP_Server::FSFileupload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
#ifdef ESP_BENCHMARK_FEATURE
  static uint64_t bench_start;
  static size_t bench_transfered;
#endif  // ESP_BENCHMARK_FEATURE
  static String upload_filename;
  static ESP_File fsUploadFile;
  ESP3DAuthenticationLevel auth_level = AuthenticationService::getAuthenticatedLevel();
  
  if (auth_level == ESP3DAuthenticationLevel::guest) {
    pushError(ESP_ERROR_AUTHENTICATION, "Upload rejected", 401);
    _upload_status = UPLOAD_STATUS_FAILED;
    request->send(401, "text/plain", "Upload rejected");
    return;
  }

  if (!index) {
#if defined(ESP3DLIB_ENV) && COMMUNICATION_PROTOCOL == SOCKET_SERIAL
    Serial2Socket.pause();
#endif  // ESP3DLIB_ENV && COMMUNICATION_PROTOCOL == SOCKET_SERIAL
#ifdef ESP_BENCHMARK_FEATURE
    bench_start = millis();
    bench_transfered = 0;
#endif  // ESP_BENCHMARK_FEATURE
    _upload_status = UPLOAD_STATUS_ONGOING;
    if (filename[0] != '/') {
      upload_filename = "/" + filename;
    } else {
      upload_filename = filename;
    }
    if (request->hasParam("rpath")) {
      String rpath = request->getParam("rpath")->value();
      upload_filename = rpath + upload_filename;
      if (upload_filename[0] != '/') {
        upload_filename = "/" + upload_filename;
      }
    }
    if (ESP_FileSystem::exists(upload_filename.c_str())) {
      ESP_FileSystem::remove(upload_filename.c_str());
    }
    String path = request->hasParam("path") ? request->getParam("path")->value() : "/";
    if (path[0] != '/') {
      path = "/" + path;
    }
    if (path[path.length() - 1] != '/') {
      path += "/";
    }
    if (request->hasParam("createPath") && path.length() > 1 && request->getParam("createPath")->value() == "true") {
      int pos = path.indexOf('/', 1);
      while (pos != -1) {
        String currentPath = path.substring(0, pos);
        if (!ESP_FileSystem::exists(currentPath.c_str())) {
          if (!ESP_FileSystem::mkdir(currentPath.c_str())) {
            pushError(ESP_ERROR_FILE_CREATION, "Failed to create path", 500);
            _upload_status = UPLOAD_STATUS_FAILED;
            break;
          }
        }
        if ((uint)(pos + 1) >= path.length() - 1) {
          pos = -1;
          break;
        } else {
          pos = path.indexOf('/', pos + 1);
        }
      }
    }
    if (_upload_status != UPLOAD_STATUS_FAILED) {
      String sizeargname = filename + "S";
      if (request->hasParam(sizeargname.c_str())) {
        size_t freespace = ESP_FileSystem::totalBytes() - ESP_FileSystem::usedBytes();
        size_t filesize = request->getParam(sizeargname.c_str())->value().toInt();
        if (freespace < filesize) {
          _upload_status = UPLOAD_STATUS_FAILED;
          pushError(ESP_ERROR_NOT_ENOUGH_SPACE, "Upload rejected");
        }
      }
      if (_upload_status != UPLOAD_STATUS_FAILED) {
        fsUploadFile = ESP_FileSystem::open(upload_filename.c_str(), ESP_FILE_WRITE);
        if (!fsUploadFile) {
          _upload_status = UPLOAD_STATUS_FAILED;
          pushError(ESP_ERROR_FILE_CREATION, "File creation failed");
        }
      }
    }
  }

  if (_upload_status == UPLOAD_STATUS_ONGOING && len) {
#ifdef ESP_BENCHMARK_FEATURE
    bench_transfered += len;
#endif  // ESP_BENCHMARK_FEATURE
    if (fsUploadFile && fsUploadFile.write(data, len) != len) {
      _upload_status = UPLOAD_STATUS_FAILED;
      pushError(ESP_ERROR_FILE_WRITE, "File write failed");
    }
  }

  if (final) {
    esp3d_log("upload end");
    if (fsUploadFile) {
      fsUploadFile.close();
#ifdef ESP_BENCHMARK_FEATURE
      benchMark("FS upload", bench_start, millis(), bench_transfered);
#endif  // ESP_BENCHMARK_FEATURE
      uint32_t filesize = fsUploadFile.size();
      _upload_status = UPLOAD_STATUS_SUCCESSFUL;
      String sizeargname = filename + "S";
      if (request->hasParam(sizeargname.c_str())) {
        esp3d_log("Size check: %s vs %s",
                  request->getParam(sizeargname.c_str())->value().c_str(),
                  String(filesize).c_str());
        if (request->getParam(sizeargname.c_str())->value() != String(filesize)) {
          esp3d_log_e("Size Error");
          _upload_status = UPLOAD_STATUS_FAILED;
          pushError(ESP_ERROR_SIZE, "File upload failed");
        }
      }
      if (_upload_status == UPLOAD_STATUS_ONGOING) {
        _upload_status = UPLOAD_STATUS_SUCCESSFUL;
      }
    } else {
      esp3d_log_e("Close Error");
      _upload_status = UPLOAD_STATUS_FAILED;
      pushError(ESP_ERROR_FILE_CLOSE, "File close failed");
    }
#if defined(ESP3DLIB_ENV) && COMMUNICATION_PROTOCOL == SOCKET_SERIAL
    Serial2Socket.pause(false);
#endif  // ESP3DLIB_ENV && COMMUNICATION_PROTOCOL == SOCKET_SERIAL
    AsyncWebServerResponse *response = request->beginResponse(_upload_status == UPLOAD_STATUS_SUCCESSFUL ? 200 : 500, "text/plain",
                                                             _upload_status == UPLOAD_STATUS_SUCCESSFUL ? "Upload successful" : "Upload failed");
    response->addHeader("Connection", "close");
    request->send(response);
  }

  if (_upload_status == UPLOAD_STATUS_FAILED) {
    cancelUpload();
    if (fsUploadFile) {
      fsUploadFile.close();
    }
    if (auth_level != ESP3DAuthenticationLevel::guest) {
      if (ESP_FileSystem::exists(upload_filename.c_str())) {
        ESP_FileSystem::remove(upload_filename.c_str());
      }
    }
#if defined(ESP3DLIB_ENV) && COMMUNICATION_PROTOCOL == SOCKET_SERIAL
    Serial2Socket.pause(false);
#endif  // ESP3DLIB_ENV && COMMUNICATION_PROTOCOL == SOCKET_SERIAL
  }
}
#endif  // HTTP_FEATURE && FILESYSTEM_FEATURE