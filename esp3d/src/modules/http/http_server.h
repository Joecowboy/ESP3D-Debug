/*
  http_server.h - http server functions class

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

#ifndef _HTTP_SERVER_H
#define _HTTP_SERVER_H
#include "../../core/esp3d_commands.h"
#include "../../core/esp3d_message.h"
#include "../../include/esp3d_config.h"

#define ASYNCWEBSERVER
#include <ESPAsyncWebServer.h>
#ifndef WEBSERVER
#define WEBSERVER AsyncWebServer
#endif

// Upload status
typedef enum {
  UPLOAD_STATUS_NONE = 0,
  UPLOAD_STATUS_FAILED = 1,
  UPLOAD_STATUS_CANCELLED = 2,
  UPLOAD_STATUS_SUCCESSFUL = 3,
  UPLOAD_STATUS_ONGOING = 4
} upload_status_type;

class HTTP_Server {
 public:
  static bool begin();
  static void end();
  static void handle();
  static bool started() { return _started; }
  static uint16_t port() { return _port; }
  static void set_http_headers(AsyncWebServerRequest *request);
  static bool dispatch(ESP3DMessage* msg);
  static void handle_logs(AsyncWebServerRequest *request);

 private:
  static void pushError(int code, const char* st, uint16_t web_error = 500,
                        uint16_t timeout = 1000);
  static void cancelUpload();
  static bool _started;
  static WEBSERVER* _webserver;
  static uint16_t _port;
  static uint8_t _upload_status;
  static const char* get_Splited_Value(String data, char separator, int index);
#ifdef SSDP_FEATURE
  static void handle_SSDP(AsyncWebServerRequest *request);
#endif  // SSDP_FEATURE
#ifdef CAMERA_DEVICE
  static void handle_snap(AsyncWebServerRequest *request);
#endif  // CAMERA_DEVICE
  static void init_handlers();
  static bool StreamFSFile(const char* filename, const char* contentType, AsyncWebServerRequest *request);
  static void handle_root(AsyncWebServerRequest *request);
  static void handle_login(AsyncWebServerRequest *request);
  static void handle_not_found(AsyncWebServerRequest *request);
  static void handle_web_command(AsyncWebServerRequest *request);
  static void handle_config(AsyncWebServerRequest *request);
#ifdef FILESYSTEM_FEATURE
  static void FSFileupload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final);
  static void handleFSFileList(AsyncWebServerRequest *request);
#endif  // FILESYSTEM_FEATURE
#ifdef WEB_UPDATE_FEATURE
  static void handleUpdate(AsyncWebServerRequest *request);
  static void WebUpdateUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final);
#endif  // WEB_UPDATE_FEATURE
#ifdef SD_DEVICE
  static void SDFileupload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final);
  static void handleSDFileList(AsyncWebServerRequest *request);
  static bool StreamSDFile(const char* filename, const char* contentType, AsyncWebServerRequest *request);
#endif  // SD_DEVICE
#if COMMUNICATION_PROTOCOL == MKS_SERIAL
  static void MKSFileupload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final);
  static void handleMKSUpload(AsyncWebServerRequest *request);
#endif  // COMMUNICATION_PROTOCOL == MKS_SERIAL
};

extern int code_200;
extern int code_500;
extern int code_404;
extern int code_401;

#endif  //_HTTP_SERVER_H