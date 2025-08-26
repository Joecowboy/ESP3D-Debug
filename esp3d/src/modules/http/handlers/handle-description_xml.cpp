/*
  handle-description_xml.cpp - SSDP description handler for ESP3D

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
#if defined(HTTP_FEATURE) && defined(SSDP_FEATURE)
#include "../http_server.h"
#include "../../network/netconfig.h"
#include "../../../core/esp3d_settings.h"
#include <stdio.h>

void HTTP_Server::handle_SSDP(AsyncWebServerRequest *request) {
  String answer;
  answer = "<?xml version=\"1.0\"?>\n";
  answer += "<root xmlns=\"urn:schemas-upnp-org:device-1-0\">\n";
  answer += "<specVersion><major>1</major><minor>0</minor></specVersion>\n";
  answer += "<URLBase>http://";
  answer += NetConfig::localIP();
  answer += ":";
  answer += String(HTTP_Server::port());
  answer += "/</URLBase>\n";
  answer += "<device>\n";
  answer += "<deviceType>urn:schemas-upnp-org:device:Basic:1</deviceType>\n";
  answer += "<friendlyName>";
  answer += NetConfig::hostname();
  answer += "</friendlyName>\n";
  answer += "<presentationURL>/</presentationURL>\n";
  answer += "<serialNumber>1</serialNumber>\n";
  answer += "<modelName>ESP3D</modelName>\n";
  answer += "<modelNumber>3.0</modelNumber>\n";
  answer += "<modelURL>https://www.esp3d.io/</modelURL>\n";
  answer += "<manufacturer>ESP3D</manufacturer>\n";
  answer += "<manufacturerURL>https://www.esp3d.io/</manufacturerURL>\n";
  answer += "<UDN>uuid:38323636-4558-4dda-9188-cda0e6";
  char tmp[7];
  sprintf(tmp, "%02X%02X%02X", 
          ESP3DSettings::readByte(static_cast<int>(ESP3DSettingIndex::esp3d_hostname), 0),
          ESP3DSettings::readByte(static_cast<int>(ESP3DSettingIndex::esp3d_hostname), 1),
          ESP3DSettings::readByte(static_cast<int>(ESP3DSettingIndex::esp3d_hostname), 2));
  answer += tmp;
  answer += "</UDN>\n</device>\n</root>\n";
  request->send(200, "text/xml", answer.c_str());
}
#endif // HTTP_FEATURE && SSDP_FEATURE