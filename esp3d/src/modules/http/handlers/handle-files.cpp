/*
 handle-files.cpp - ESP3D http handle

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

#ifdef FILESYSTEM_TIMESTAMP_FEATURE
#include "../../time/time_service.h"
#endif  // FILESYSTEM_TIMESTAMP_FEATURE

// Utility function to format file sizes
String formatBytes(size_t bytes) {
  if (bytes < 1024) return String(bytes) + " B";
  else if (bytes < (1024 * 1024)) return String(bytes / 1024.0, 1) + " KB";
  else if (bytes < (1024 * 1024 * 1024)) return String(bytes / (1024.0 * 1024.0), 1) + " MB";
  else return String(bytes / (1024.0 * 1024.0 * 1024.0), 1) + " GB";
}

// Filesystem files list and file commands
void HTTP_Server::handleFSFileList(AsyncWebServerRequest *request) {
  HTTP_Server::set_http_headers(request);

  ESP3DAuthenticationLevel auth_level = AuthenticationService::getAuthenticatedLevel();
  if (auth_level == ESP3DAuthenticationLevel::guest) {
    _upload_status = UPLOAD_STATUS_NONE;
    request->send(401, "text/plain", "Wrong authentication!");
    return;
  }
  String path;
  String status = "ok";
  if (_upload_status == UPLOAD_STATUS_FAILED || _upload_status == UPLOAD_STATUS_CANCELLED) {
    status = "Upload failed";
    _upload_status = UPLOAD_STATUS_NONE;
  }
  if (request->hasParam("quiet") && request->getParam("quiet")->value() == "yes") {
    status = "{\"status\":\"" + status + "\"}";
    request->send(200, "application/json", status);
    return;
  }
  // get current path
  if (request->hasParam("path")) {
    path = request->getParam("path")->value();
  }
  // to have a clean path
  path.trim();
  path.replace("//", "/");
  if (path[path.length() - 1] != '/') {
    path += "/";
  }
  // check if query needs some action
  if (request->hasParam("action")) {
    // delete a file
    if (request->getParam("action")->value() == "delete" && request->hasParam("filename")) {
      String filename = path + request->getParam("filename")->value();
      String shortname = request->getParam("filename")->value();
      shortname.replace("/", "");
      filename.replace("//", "/");
      if (!ESP_FileSystem::exists(filename.c_str())) {
        status = shortname + " does not exist!";
      } else {
        if (ESP_FileSystem::remove(filename.c_str())) {
          status = shortname + " deleted";
          String ptmp = path;
          if (path != "/" && path[path.length() - 1] == '/') {
            ptmp = path.substring(0, path.length() - 1);
          }
          if (!ESP_FileSystem::exists(ptmp.c_str())) {
            ESP_FileSystem::mkdir(ptmp.c_str());
          }
        } else {
          status = "Cannot delete " + shortname;
        }
      }
    }
    // delete a directory
    if (request->getParam("action")->value() == "deletedir" && request->hasParam("filename")) {
      String filename = path + request->getParam("filename")->value() + "/";
      String shortname = request->getParam("filename")->value();
      shortname.replace("/", "");
      filename.replace("//", "/");
      if (filename != "/") {
        if (ESP_FileSystem::rmdir(filename.c_str())) {
          esp3d_log("Deleting %s", filename.c_str());
          status = shortname + " deleted";
        } else {
          status = "Cannot delete " + shortname;
        }
      }
    }
    // create a directory
    if (request->getParam("action")->value() == "createdir" && request->hasParam("filename")) {
      String filename = path + request->getParam("filename")->value();
      String shortname = request->getParam("filename")->value();
      shortname.replace("/", "");
      filename.replace("//", "/");
      if (ESP_FileSystem::exists(filename.c_str())) {
        status = shortname + " already exists!";
      } else {
        if (!ESP_FileSystem::mkdir(filename.c_str())) {
          status = "Cannot create " + shortname;
        } else {
          status = shortname + " created";
        }
      }
    }
  }
  String buffer2send;
  buffer2send.reserve(1200);
  buffer2send = "{\"files\":[";
  String ptmp = path;
  if (path != "/" && path[path.length() - 1] == '/') {
    ptmp = path.substring(0, path.length() - 1);
  }
  AsyncWebServerResponse *response = request->beginResponseStream("application/json");
  response->addHeader("Cache-Control", "no-cache");
  request->send(response);
  if (ESP_FileSystem::exists(ptmp.c_str())) {
    ESP_File f = ESP_FileSystem::open(ptmp.c_str(), ESP_FILE_READ);
    ESP_File sub = f.openNextFile();
    if (f) {
      bool needseparator = false;
      while (sub) {
        if (needseparator) {
          buffer2send += ",";
        } else {
          needseparator = true;
        }
        buffer2send += "{\"name\":\"" + String(sub.name()) + "\",\"size\":\"";
        if (sub.isDirectory()) {
          buffer2send += "-1";
        } else {
          buffer2send += formatBytes(sub.size());
        }
#ifdef FILESYSTEM_TIMESTAMP_FEATURE
        buffer2send += "\",\"time\":\"";
        if (!sub.isDirectory()) {
          buffer2send += timeService.getDateTime((time_t)sub.getLastWrite());
        }
#endif  // FILESYSTEM_TIMESTAMP_FEATURE
        buffer2send += "\"}";
        if (buffer2send.length() > 1100) {
          response->print(buffer2send);
          buffer2send = "";
        }
        sub.close();
        sub = f.openNextFile();
      }
      f.close();
    } else {
      status = (status == "ok") ? "cannot open " + ptmp : status + ", cannot open " + ptmp;
    }
  } else {
    status = (status == "ok") ? ptmp + " does not exist!" : status + ", " + ptmp + " does not exist!";
  }
  buffer2send += "],\"path\":\"" + path + "\",";
  if (ESP_FileSystem::totalBytes() > 0) {
    buffer2send += String("\"occupation\":\"") +
                   String(100 * ESP_FileSystem::usedBytes() / ESP_FileSystem::totalBytes()) + "\",";
  } else {
    status = "FileSystem Error";
    buffer2send += String("\"occupation\":\"0\",");
  }
  buffer2send += String("\"status\":\"") + status + "\",";
  buffer2send += String("\"total\":\"") + formatBytes(ESP_FileSystem::totalBytes()) + "\",";
  buffer2send += String("\"used\":\"") + formatBytes(ESP_FileSystem::usedBytes()) + "\"}";
  response->print(buffer2send);
  response->print(""); // End stream
  _upload_status = UPLOAD_STATUS_NONE;
}
#endif  // HTTP_FEATURE && FILESYSTEM_FEATURE