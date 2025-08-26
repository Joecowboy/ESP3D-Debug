/*
 handle-command.cpp - ESP3D http handle

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
#include "../../../core/esp3d_commands.h"
#include "../../../core/esp3d_message.h"
#include "../../../core/esp3d_settings.h"
#include "../../../core/esp3d_string.h"
#include "../../authentication/authentication_service.h"

// Handle web command query and send answer
void HTTP_Server::handle_web_command(AsyncWebServerRequest *request) {
  ESP3DAuthenticationLevel auth_level = AuthenticationService::getAuthenticatedLevel();
  if (auth_level == ESP3DAuthenticationLevel::guest) {
    request->send(401, "text/plain", "Wrong authentication!");
    return;
  }
  String cmd = "";
  bool isRealTimeCommand = false;
  if (request->hasParam("cmd")) {
    cmd = request->getParam("cmd")->value();
    esp3d_log("Command is %s", cmd.c_str());
    if (!cmd.endsWith("\n")) {
      esp3d_log("Command is not ending with \\n");
      if (ESP3DSettings::GetFirmwareTarget() == GRBL || ESP3DSettings::GetFirmwareTarget() == GRBLHAL) {
        uint len = cmd.length();
        if (!((len == 1 && esp3d_string::isRealTimeCommand(cmd[0])) ||
              (len == 2 && esp3d_string::isRealTimeCommand(cmd[1])))) {
          cmd += "\n";
          esp3d_log("Command is not realtime, adding \\n");
        } else {
          esp3d_log("Command is realtime, no need to add \\n");
          isRealTimeCommand = true;
          if (len == 2 && esp3d_string::isRealTimeCommand(cmd[1]) && cmd[0] == 0xC2) {
            cmd[0] = cmd[1];
            cmd[1] = 0x0;
            esp3d_log("Command is realtime, removing 0xC2");
          }
        }
      } else {
        esp3d_log("Command is not realtime, adding \\n");
        cmd += "\n";
      }
    } else {
      esp3d_log("Command is ending with \\n");
    }
    esp3d_log("Message type is %s for %s", isRealTimeCommand ? "realtimecmd" : "unique", cmd.c_str());
    if (esp3d_commands.is_esp_command((uint8_t *)cmd.c_str(), cmd.length())) {
      ESP3DMessage *msg = esp3d_message_manager.newMsg(
          ESP3DClientType::http, esp3d_commands.getOutputClient(),
          (uint8_t *)cmd.c_str(), cmd.length(), auth_level);
      if (msg) {
        msg->type = ESP3DMessageType::unique;
        msg->request_id.code = 200;
        esp3d_commands.process(msg);
      } else {
        esp3d_log_e("Cannot create message");
        request->send(500, "text/plain", "Cannot create message");
      }
    } else {
      set_http_headers(request);
      request->send(200, "text/plain", "ESP3D says: command forwarded");
      esp3d_commands.dispatch(cmd.c_str(), esp3d_commands.getOutputClient(),
                              no_id, isRealTimeCommand ? ESP3DMessageType::realtimecmd : ESP3DMessageType::unique,
                              ESP3DClientType::http, auth_level);
    }
  } else if (request->hasParam("ping")) {
    request->send(200);
  } else {
    request->send(400, "text/plain", "Invalid command");
  }
}
#endif  // HTTP_FEATURE