/*
 handle-updatefw.cpp - ESP3D http handle

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
#include "../../../core/esp3d_log.h"
#include "../../../core/esp3d_hal.h"
#include "../../authentication/authentication_service.h"
#if defined(ARDUINO_ARCH_ESP32)
#include <Update.h>
#endif  // ARDUINO_ARCH_ESP32
#if defined(ARDUINO_ARCH_ESP8266)
#include <Updater.h>
#endif  // ARDUINO_ARCH_ESP8266
#include <ESPAsyncWebServer.h>

// Web Update handler
void HTTP_Server::handleUpdate(AsyncWebServerRequest *request) {
  esp3d_log("Handling firmware update request");
#ifdef AUTHENTICATION_FEATURE
  if (!AuthenticationService::is_authenticated(request, ESP3DAuthenticationLevel::admin)) {
    esp3d_log_e("Unauthorized firmware update attempt");
    pushError(401, "Unauthorized", 401, 1000);
    cancelUpload();
    request->send(401, "text/plain", "Unauthorized");
    return;
  }
#endif
  _upload_status = UPLOAD_STATUS_NONE;
  AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "Ready for update");
  response->addHeader("Connection", "close");
  request->send(response);
}

#endif  // HTTP_FEATURE && WEB_UPDATE_FEATURE