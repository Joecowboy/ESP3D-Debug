/*
 handle-root.cpp - ESP3D http handle

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
#include "../embedded.h"
#include "../../../core/esp3d_string.h"
#include "../../filesystem/esp_filesystem.h"

// Root of Webserver
void HTTP_Server::handle_root(AsyncWebServerRequest *request) {
  set_http_headers(request);

  String path = ESP3D_HOST_PATH;
  if (path[0] != '/') {
    path = "/" + path;
  }
  if (path[path.length() - 1] != '/') {
    path += "/";
  }
  path += "index.html";
  String contentType = esp3d_string::getContentType(path.c_str());
  String pathWithGz = path + ".gz";
  if ((ESP_FileSystem::exists(pathWithGz.c_str()) || ESP_FileSystem::exists(path.c_str())) &&
      (!request->hasParam("forcefallback") || request->getParam("forcefallback")->value() != "yes")) {
    esp3d_log("Path found `%s`", path.c_str());
    if (ESP_FileSystem::exists(pathWithGz.c_str())) {
      request->addResponseHeader("Content-Encoding", "gzip");
      path = pathWithGz;
      esp3d_log("Path is gz `%s`", path.c_str());
    }
    if (!StreamFSFile(path.c_str(), contentType.c_str(), request)) {
      esp3d_log_e("Stream `%s` failed", path.c_str());
      request->send(500, "text/plain", "Stream failed");
    }
    return;
  }
  request->addResponseHeader("Content-Encoding", "gzip");
  request->send_P(200, "text/html", (const char *)tool_html_gz, tool_html_gz_size);
}
#endif  // HTTP_FEATURE