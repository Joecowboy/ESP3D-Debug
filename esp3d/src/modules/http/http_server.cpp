/*
  http_server.cpp - http server functions class

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

#include "../../include/esp3d_config.h"
#include "http_server.h"
#include "../../core/esp3d_log.h"
#include "../../modules/authentication/authentication_service.h"
#ifdef FILESYSTEM_FEATURE
#include "../../modules/filesystem/esp_filesystem.h"
#endif
#ifdef SD_DEVICE
#include "../../modules/filesystem/esp_sd.h"
#endif
#ifdef WEB_UPDATE_FEATURE
#if defined(ARDUINO_ARCH_ESP32)
#include <Update.h>
#endif  // ARDUINO_ARCH_ESP32
#if defined(ARDUINO_ARCH_ESP8266)
#include <Updater.h>
#endif  // ARDUINO_ARCH_ESP8266
#endif

bool HTTP_Server::_started = false;
WEBSERVER* HTTP_Server::_webserver = nullptr;
uint16_t HTTP_Server::_port = 80;
uint8_t HTTP_Server::_upload_status = UPLOAD_STATUS_NONE;

int code_200 = 200;
int code_500 = 500;
int code_404 = 404;
int code_401 = 401;

void HTTP_Server::init_handlers() {
  if (!_webserver) {
    esp3d_log_e("Webserver not initialized");
    return;
  }
  _webserver->onNotFound([](AsyncWebServerRequest *request) { handle_not_found(request); });
  _webserver->on("/login", HTTP_ANY, [](AsyncWebServerRequest *request) { handle_login(request); });
  _webserver->on("/command", HTTP_ANY, [](AsyncWebServerRequest *request) { handle_web_command(request); });
  _webserver->on("/config", HTTP_ANY, [](AsyncWebServerRequest *request) { handle_config(request); });
#ifdef WEB_UPDATE_FEATURE
  _webserver->on(
      "/updatefw", HTTP_ANY,
      [](AsyncWebServerRequest *request) { handleUpdate(request); },
      [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
        WebUpdateUpload(request, filename, index, data, len, final);
      });
#endif
#ifdef SSDP_FEATURE
  _webserver->on("/description.xml", HTTP_GET, [](AsyncWebServerRequest *request) { handle_SSDP(request); });
#endif
  _webserver->on("/generate_204", HTTP_ANY, [](AsyncWebServerRequest *request) { handle_root(request); });
  _webserver->on("/gconnectivitycheck.gstatic.com", HTTP_ANY, [](AsyncWebServerRequest *request) { handle_root(request); });
  _webserver->on("/fwlink/", HTTP_ANY, [](AsyncWebServerRequest *request) { handle_root(request); });
#ifdef FILESYSTEM_FEATURE
  _webserver->on(
      "/files", HTTP_ANY,
      [](AsyncWebServerRequest *request) { handleFSFileList(request); },
      [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
        FSFileupload(request, filename, index, data, len, final);
      });
#endif
  _webserver->on("/logs", HTTP_ANY, [](AsyncWebServerRequest *request) { handle_logs(request); });
#ifdef SD_DEVICE
  _webserver->on(
      "/sdfiles", HTTP_ANY,
      [](AsyncWebServerRequest *request) { handleSDFileList(request); },
      [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
        SDFileupload(request, filename, index, data, len, final);
      });
#endif
#if COMMUNICATION_PROTOCOL == MKS_SERIAL
  _webserver->on(
      "/mksupload", HTTP_POST,
      [](AsyncWebServerRequest *request) { handleMKSUpload(request); },
      [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
        MKSFileupload(request, filename, index, data, len, final);
      });
#endif
  esp3d_log("HTTP handlers initialized");
}

bool HTTP_Server::StreamFSFile(const char* filename, const char* contentType, AsyncWebServerRequest *request) {
#ifdef FILESYSTEM_FEATURE
  if (!ESP_FileSystem::exists(filename)) {
    esp3d_log_e("File %s does not exist", filename);
    request->send(404, "text/plain", "File not found");
    return false;
  }
  AsyncWebServerResponse *response = request->beginResponse(ESP_FileSystem::open(filename, ESP_FILE_READ), filename, contentType);
  request->send(response);
  esp3d_log("Streaming file %s with content type %s", filename, contentType);
  return true;
#else
  esp3d_log_e("Filesystem feature not enabled");
  request->send(500, "text/plain", "Filesystem not enabled");
  return false;
#endif
}

#ifdef WEB_UPDATE_FEATURE
void HTTP_Server::handleUpdate(AsyncWebServerRequest *request) {
  _upload_status = UPLOAD_STATUS_NONE;
  AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "Ready for update");
  response->addHeader("Connection", "close");
  request->send(response);
}

void HTTP_Server::WebUpdateUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (!index) {
    _upload_status = UPLOAD_STATUS_ONGOING;
    size_t updateSize = request->hasParam("size") ? request->getParam("size")->value().toInt() : UPDATE_SIZE_UNKNOWN;
    if (!Update.begin(updateSize)) {
      _upload_status = UPLOAD_STATUS_FAILED;
      esp3d_log_e("Update failed to begin: %s", Update.getErrorString());
      request->send(500, "text/plain", String("Update failed: ") + Update.getErrorString());
      return;
    }
  }
  if (_upload_status == UPLOAD_STATUS_ONGOING && len) {
    if (Update.write(data, len) != len) {
      _upload_status = UPLOAD_STATUS_FAILED;
      esp3d_log_e("Update write failed: %s", Update.getErrorString());
      request->send(500, "text/plain", String("Update failed: ") + Update.getErrorString());
      return;
    }
  }
  if (final) {
    if (Update.end(true)) {
      _upload_status = UPLOAD_STATUS_SUCCESSFUL;
      esp3d_log("Update successful");
      AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "Update successful");
      response->addHeader("Connection", "close");
      request->send(response);
      ESP3DHal::wait(1000);
      ESP.restart();
    } else {
      _upload_status = UPLOAD_STATUS_FAILED;
      esp3d_log_e("Update failed to end: %s", Update.getErrorString());
      AsyncWebServerResponse *response = request->beginResponse(500, "text/plain", String("Update failed: ") + Update.getErrorString());
      response->addHeader("Connection", "close");
      request->send(response);
    }
  }
}
#endif

void HTTP_Server::set_http_headers(AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginResponse(200);
  response->addHeader("Content-Type", "application/json");
  response->addHeader("Cache-Control", "no-cache");
  request->send(response);
}

void HTTP_Server::handle_not_found(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

void HTTP_Server::handle_login(AsyncWebServerRequest *request) {
  request->send(200, "text/plain", "Login endpoint");
}

void HTTP_Server::handle_web_command(AsyncWebServerRequest *request) {
  request->send(200, "text/plain", "Web command endpoint");
}

void HTTP_Server::handle_config(AsyncWebServerRequest *request) {
  request->send(200, "text/plain", "Config endpoint");
}

#ifdef SSDP_FEATURE
void HTTP_Server::handle_SSDP(AsyncWebServerRequest *request) {
  request->send(200, "text/xml", "<xml>SSDP description</xml>");
}
#endif

void HTTP_Server::handle_logs(AsyncWebServerRequest *request) {
  request->send(200, "text/plain", "Logs endpoint");
}

#ifdef FILESYSTEM_FEATURE
void HTTP_Server::handleFSFileList(AsyncWebServerRequest *request) {
  request->send(200, "text/plain", "File list endpoint");
}
#endif

#ifdef SD_DEVICE
void HTTP_Server::SDFileupload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
  request->send(200, "text/plain", "SD file upload endpoint");
}

void HTTP_Server::handleSDFileList(AsyncWebServerRequest *request) {
  request->send(200, "text/plain", "SD file list endpoint");
}

bool HTTP_Server::StreamSDFile(const char* filename, const char* contentType, AsyncWebServerRequest *request) {
  request->send(200, "text/plain", "Stream SD file endpoint");
  return true;
}
#endif

#if COMMUNICATION_PROTOCOL == MKS_SERIAL
void HTTP_Server::MKSFileupload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
  request->send(200, "text/plain", "MKS file upload endpoint");
}

void HTTP_Server::handleMKSUpload(AsyncWebServerRequest *request) {
  request->send(200, "text/plain", "MKS upload endpoint");
}
#endif

bool HTTP_Server::begin() {
  if (_started) return true;
  _webserver = new WEBSERVER(_port);
  if (!_webserver) {
    esp3d_log_e("Failed to create webserver");
    return false;
  }
  init_handlers();
  _webserver->begin();
  _started = true;
  esp3d_log("HTTP server started on port %d", _port);
  return true;
}

void HTTP_Server::end() {
  if (_webserver) {
    _webserver->end();
    delete _webserver;
    _webserver = nullptr;
    _started = false;
    esp3d_log("HTTP server stopped");
  }
}

void HTTP_Server::handle() {
}

bool HTTP_Server::dispatch(ESP3DMessage* msg) {
  return true;
}

const char* HTTP_Server::get_Splited_Value(String data, char separator, int index) {
  return "";
}

void HTTP_Server::pushError(int code, const char* st, uint16_t web_error, uint16_t timeout) {
}

void HTTP_Server::cancelUpload() {
  _upload_status = UPLOAD_STATUS_CANCELLED;
  esp3d_log("Upload cancelled");
}
#endif