/*
 handle-login.cpp - ESP3D http handle

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
#include "../../../core/esp3d_message.h"
#include "../../../core/esp3d_settings.h"
#include "../../authentication/authentication_service.h"

// login status check
void HTTP_Server::handle_login(AsyncWebServerRequest *request) {
  set_http_headers(request);

#ifdef AUTHENTICATION_FEATURE
  int code = 401;
  String status = "Wrong authentication!";
  // Disconnect can be done anytime no need to check credential
  if (request->hasParam("DISCONNECT") && request->getParam("DISCONNECT")->value() == "YES") {
    AuthenticationService::ClearCurrentHttpSession();
    request->addResponseHeader("Set-Cookie", "ESPSESSIONID=0");
    request->addResponseHeader("Cache-Control", "no-cache");
    request->send(401, "application/json",
                  "{\"status\":\"disconnected\",\"authentication_lvl\":\"guest\"}");
    return;
  }
  ESP3DAuthenticationLevel auth_level = AuthenticationService::getAuthenticatedLevel();
  // check if it is a submission or a query
  if (request->hasParam("SUBMIT")) {
    // check if correct list of query parameters
    if (request->hasParam("PASSWORD") && request->hasParam("USER")) {
      // User
      String sUser = request->getParam("USER")->value();
      // Password
      String sPassword = request->getParam("PASSWORD")->value();
      if (((sUser == DEFAULT_ADMIN_LOGIN && AuthenticationService::isadmin(sPassword.c_str())) ||
           (sUser == DEFAULT_USER_LOGIN && AuthenticationService::isuser(sPassword.c_str())))) {
        // check if it is to change password or login
        if (request->hasParam("NEWPASSWORD")) {
          String newpassword = request->getParam("NEWPASSWORD")->value();
          // check new password
          if (ESP3DSettings::isValidStringSetting(newpassword.c_str(), ESP_ADMIN_PWD)) {
            if (!ESP3DSettings::writeString(ESP_ADMIN_PWD, newpassword.c_str())) {
              code = 500;
              status = "Set failed!";
            } else {
              code = 200;
              status = "ok";
            }
          } else {
            code = 500;
            status = "Incorrect password!";
          }
        } else {  // do authentication
          // allow to change session timeout when login
          if (request->hasParam("TIMEOUT")) {
            String timeout = request->getParam("TIMEOUT")->value();
            AuthenticationService::setSessionTimeout(timeout.toInt());
          }
          // it is a change or same level
          if ((auth_level == ESP3DAuthenticationLevel::user && sUser == DEFAULT_USER_LOGIN) ||
              (auth_level == ESP3DAuthenticationLevel::admin && sUser == DEFAULT_ADMIN_LOGIN)) {
            code = 200;
            status = "ok";
          } else {  // new authentication
            String session = AuthenticationService::create_session_ID();
            if (AuthenticationService::CreateSession(
                    (sUser == DEFAULT_ADMIN_LOGIN) ? ESP3DAuthenticationLevel::admin
                                                  : ESP3DAuthenticationLevel::user,
                    ESP3DClientType::http, session.c_str())) {
              AuthenticationService::ClearCurrentHttpSession();
              code = 200;
              status = "ok";
              String tmps = "ESPSESSIONID=" + session;
              request->addResponseHeader("Set-Cookie", tmps);
            }
          }
        }
      }
    }
  } else {
    if (auth_level == ESP3DAuthenticationLevel::user || auth_level == ESP3DAuthenticationLevel::admin) {
      status = "Identified";
      code = 200;
    }
  }
  request->addResponseHeader("Cache-Control", "no-cache");
  String smsg = "{\"status\":\"" + status + "\",\"authentication_lvl\":\"";
  smsg += (auth_level == ESP3DAuthenticationLevel::user) ? "user" :
          (auth_level == ESP3DAuthenticationLevel::admin) ? "admin" : "guest";
  smsg += "\"}";
  request->send(code, "application/json", smsg);
#else   // No AUTHENTICATION_FEATURE
  request->addResponseHeader("Cache-Control", "no-cache");
  request->send(200, "application/json",
                "{\"status\":\"ok\",\"authentication_lvl\":\"admin\"}");
#endif  // AUTHENTICATION_FEATURE
}
#endif  // HTTP_FEATURE