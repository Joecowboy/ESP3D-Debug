/*
  netservices.cpp - network services functions class

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
  License along with this code; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "netservices.h"

#include "../../core/esp3d_commands.h"
#include "../../core/esp3d_settings.h"
#include "../../include/esp3d_config.h"
#include "netconfig.h"

#ifdef MDNS_FEATURE
#include "../mDNS/mDNS.h"
#endif  // MDNS_FEATURE
#if defined(ARDUINO_ARCH_ESP8266)
#ifdef SSDP_FEATURE
#include <ESP8266SSDP.h>
#endif  // SSDP_FEATURE
#endif  // ARDUINO_ARCH_ESP8266
#if defined(ARDUINO_ARCH_ESP32)
#ifdef SSDP_FEATURE
#include <ESP32SSDP.h>
#endif  // SSDP_FEATURE
#endif  // ARDUINO_ARCH_ESP32
#ifdef OTA_FEATURE
#include <ArduinoOTA.h>
#endif  // OTA_FEATURE
#if defined(FILESYSTEM_FEATURE)
#include "../filesystem/esp_filesystem.h"
#endif  // FILESYSTEM_FEATURE
#ifdef TELNET_FEATURE
#include "../telnet/telnet_server.h"
#endif  // TELNET_FEATURE
#ifdef FTP_FEATURE
#include "../ftp/FtpServer.h"
#endif  // FTP_FEATURE
#ifdef WEBDAV_FEATURE
#include "../webdav/webdav_server.h"
#endif  // WEBDAV_FEATURE
#ifdef HTTP_FEATURE
#include "../http/http_server.h"
#endif  // HTTP_FEATURE
#if defined(HTTP_FEATURE) || defined(WS_DATA_FEATURE)
#include "../websocket/websocket_server.h"
#endif  // HTTP_FEATURE || WS_DATA_FEATURE
#ifdef CAPTIVE_PORTAL_FEATURE
#include <DNSServer.h>
const byte DNS_PORT = 53;
DNSServer dnsServer;
#endif  // CAPTIVE_PORTAL_FEATURE
#ifdef TIMESTAMP_FEATURE
#include "../time/time_service.h"
#endif  // TIMESTAMP_FEATURE
#ifdef NOTIFICATION_FEATURE
#include "../notifications/notifications_service.h"
#endif  // NOTIFICATION_FEATURE
#ifdef CAMERA_DEVICE
#include "../camera/camera.h"
#endif  // CAMERA_DEVICE
#if COMMUNICATION_PROTOCOL == MKS_SERIAL
#include "../mks/mks_service.h"
#endif  // COMMUNICATION_PROTOCOL == MKS_SERIAL

bool NetServices::_started = false;
bool NetServices::_restart = false;

bool NetServices::begin() {
  bool res = true;
  _started = false;
  String hostname = ESP3DSettings::readString(ESP_HOSTNAME);
  esp3d_log("Starting network services for hostname: %s", hostname.c_str());
  end();

  // Verify network mode and IPs
  if (NetConfig::getMode() == ESP_WIFI_APSTA) {
    if (WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
      esp3d_log_e("STA IP not assigned, services may not bind to STA interface");
      esp3d_commands.dispatch("STA IP not assigned, check WiFi settings",
                              ESP3DClientType::all_clients, no_id,
                              ESP3DMessageType::unique, ESP3DClientType::system,
                              ESP3DAuthenticationLevel::admin);
      res = false;
    } else {
      esp3d_log("APSTA mode: AP IP: %s, STA IP: %s",
                WiFi.softAPIP().toString().c_str(),
                WiFi.localIP().toString().c_str());
    }
  }

#ifdef TIMESTAMP_FEATURE
  if (WiFi.getMode() != WIFI_AP) {
    if (!timeService.begin()) {
      if (timeService.isInternetTime()) {
        esp3d_log_e("Failed to contact time servers");
        esp3d_commands.dispatch("Failed contact time servers!",
                                ESP3DClientType::all_clients, no_id,
                                ESP3DMessageType::unique, ESP3DClientType::system,
                                ESP3DAuthenticationLevel::admin);
      }
    } else {
      String tmp = String("Current time: ");
      tmp += timeService.getCurrentTime();
      esp3d_log("%s", tmp.c_str());
      if (ESP3DSettings::isVerboseBoot()) {
        esp3d_commands.dispatch(tmp.c_str(), ESP3DClientType::all_clients,
                                no_id, ESP3DMessageType::unique,
                                ESP3DClientType::system,
                                ESP3DAuthenticationLevel::admin);
      }
    }
  }
#endif  // TIMESTAMP_FEATURE

#if defined(MDNS_FEATURE) && defined(ARDUINO_ARCH_ESP8266)
  if (!esp3d_mDNS.begin(hostname.c_str())) {
    esp3d_log_e("Failed to start mDNS");
  } else {
    esp3d_log("mDNS started for %s", hostname.c_str());
  }
#endif  // MDNS_FEATURE && ARDUINO_ARCH_ESP8266

#ifdef OTA_FEATURE
  if (WiFi.getMode() != WIFI_AP) {
    ArduinoOTA.onStart([]() {
      String type = String("Start OTA updating ");
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type += "sketch";
      } else {  // U_SPIFFS or any FS
        type += "filesystem";
#if defined(FILESYSTEM_FEATURE)
        ESP_FileSystem::end();
#endif  // FILESYSTEM_FEATURE
      }
      esp3d_log("%s", type.c_str());
      esp3d_commands.dispatch(type.c_str(), ESP3DClientType::all_clients, no_id,
                              ESP3DMessageType::unique, ESP3DClientType::system,
                              ESP3DAuthenticationLevel::admin);
    });
    ArduinoOTA.onEnd([]() {
      esp3d_log("OTA ended");
      esp3d_commands.dispatch("End OTA", ESP3DClientType::all_clients, no_id,
                              ESP3DMessageType::unique, ESP3DClientType::system,
                              ESP3DAuthenticationLevel::admin);
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      String prg = String("OTA Progress ") + String(progress / (total / 100)) + "%";
      esp3d_log("%s", prg.c_str());
      esp3d_commands.dispatch(prg.c_str(), ESP3DClientType::all_clients, no_id,
                              ESP3DMessageType::unique, ESP3DClientType::system,
                              ESP3DAuthenticationLevel::admin);
    });
    ArduinoOTA.onError([](ota_error_t error) {
      String stmp = String("OTA Error: ") + String(error);
      esp3d_log_e("%s", stmp.c_str());
      esp3d_commands.dispatch(stmp.c_str(), ESP3DClientType::all_clients, no_id,
                              ESP3DMessageType::unique, ESP3DClientType::system,
                              ESP3DAuthenticationLevel::admin);
      if (error == OTA_AUTH_ERROR) {
        esp3d_log_e("OTA Auth Failed");
        esp3d_commands.dispatch("Auth Failed", ESP3DClientType::all_clients,
                                no_id, ESP3DMessageType::unique,
                                ESP3DClientType::system,
                                ESP3DAuthenticationLevel::admin);
      } else if (error == OTA_BEGIN_ERROR) {
        esp3d_log_e("OTA Begin Failed");
        esp3d_commands.dispatch("Begin Failed", ESP3DClientType::all_clients,
                                no_id, ESP3DMessageType::unique,
                                ESP3DClientType::system,
                                ESP3DAuthenticationLevel::admin);
      } else if (error == OTA_CONNECT_ERROR) {
        esp3d_log_e("OTA Connect Failed");
        esp3d_commands.dispatch("Connect Failed", ESP3DClientType::all_clients,
                                no_id, ESP3DMessageType::unique,
                                ESP3DClientType::system,
                                ESP3DAuthenticationLevel::admin);
      } else if (error == OTA_RECEIVE_ERROR) {
        esp3d_log_e("OTA Receive Failed");
        esp3d_commands.dispatch("Receive Failed", ESP3DClientType::all_clients,
                                no_id, ESP3DMessageType::unique,
                                ESP3DClientType::system,
                                ESP3DAuthenticationLevel::admin);
      } else if (error == OTA_END_ERROR) {
        esp3d_log_e("OTA End Failed");
        esp3d_commands.dispatch("End Failed", ESP3DClientType::all_clients,
                                no_id, ESP3DMessageType::unique,
                                ESP3DClientType::system,
                                ESP3DAuthenticationLevel::admin);
      }
    });
    String lhostname = hostname;
    lhostname.toLowerCase();
    ArduinoOTA.setHostname(lhostname.c_str());
    if (ArduinoOTA.begin()) {
      esp3d_log("OTA service started");
      if (ESP3DSettings::isVerboseBoot()) {
        esp3d_commands.dispatch("OTA service started",
                                ESP3DClientType::all_clients, no_id,
                                ESP3DMessageType::unique, ESP3DClientType::system,
                                ESP3DAuthenticationLevel::admin);
      }
    } else {
      esp3d_log_e("OTA service failed to start");
    }
  }
#endif  // OTA_FEATURE

#if defined(MDNS_FEATURE) && defined(ARDUINO_ARCH_ESP32)
  if (!esp3d_mDNS.begin(hostname.c_str())) {
    esp3d_log_e("Failed to start mDNS");
  } else {
    esp3d_log("mDNS started for %s", hostname.c_str());
  }
#endif  // MDNS_FEATURE && ARDUINO_ARCH_ESP32

#ifdef CAPTIVE_PORTAL_FEATURE
  if (NetConfig::getMode() == ESP_AP_SETUP) {
    if (dnsServer.start(DNS_PORT, "*", WiFi.softAPIP())) {
      esp3d_log("Captive Portal started on port %d", DNS_PORT);
      if (ESP3DSettings::isVerboseBoot()) {
        esp3d_commands.dispatch("Captive Portal started",
                                ESP3DClientType::all_clients, no_id,
                                ESP3DMessageType::unique, ESP3DClientType::system,
                                ESP3DAuthenticationLevel::admin);
      }
    } else {
      esp3d_log_e("Failed to start Captive Portal");
      esp3d_commands.dispatch("Failed start Captive Portal",
                              ESP3DClientType::all_clients, no_id,
                              ESP3DMessageType::unique, ESP3DClientType::system,
                              ESP3DAuthenticationLevel::admin);
    }
  }
#endif  // CAPTIVE_PORTAL_FEATURE

#ifdef HTTP_FEATURE
  if (!HTTP_Server::begin()) {
    res = false;
    esp3d_log_e("HTTP server failed to start");
    esp3d_commands.dispatch("HTTP server failed", ESP3DClientType::all_clients,
                            no_id, ESP3DMessageType::unique,
                            ESP3DClientType::system,
                            ESP3DAuthenticationLevel::admin);
  } else {
    if (HTTP_Server::started()) {
      String stmp = String("HTTP server started port ") + String(HTTP_Server::port());
      esp3d_log("%s", stmp.c_str());
      if (ESP3DSettings::isVerboseBoot()) {
        esp3d_commands.dispatch(stmp.c_str(), ESP3DClientType::all_clients,
                                no_id, ESP3DMessageType::unique,
                                ESP3DClientType::system,
                                ESP3DAuthenticationLevel::admin);
      }
    }
  }
#endif  // HTTP_FEATURE

#ifdef TELNET_FEATURE
  if (!telnet_server.begin()) {
    res = false;
    esp3d_log_e("Telnet server failed to start");
    esp3d_commands.dispatch("Telnet server failed",
                            ESP3DClientType::all_clients, no_id,
                            ESP3DMessageType::unique, ESP3DClientType::system,
                            ESP3DAuthenticationLevel::admin);
  } else {
    if (telnet_server.started()) {
      String stmp = String("Telnet server started port ") + String(telnet_server.port());
      esp3d_log("%s", stmp.c_str());
      if (ESP3DSettings::isVerboseBoot()) {
        esp3d_commands.dispatch(stmp.c_str(), ESP3DClientType::all_clients,
                                no_id, ESP3DMessageType::unique,
                                ESP3DClientType::system,
                                ESP3DAuthenticationLevel::admin);
      }
    }
  }
#endif  // TELNET_FEATURE

#ifdef FTP_FEATURE
  if (!ftp_server.begin()) {
    res = false;
    esp3d_log_e("FTP server failed to start");
    esp3d_commands.dispatch("FTP server failed", ESP3DClientType::all_clients,
                            no_id, ESP3DMessageType::unique,
                            ESP3DClientType::system,
                            ESP3DAuthenticationLevel::admin);
  } else {
    if (ftp_server.started()) {
      String stmp = String("FTP server started ports: ") + String(ftp_server.ctrlport()) +
                    "," + String(ftp_server.dataactiveport()) + "," +
                    String(ftp_server.datapassiveport());
      esp3d_log("%s", stmp.c_str());
      if (ESP3DSettings::isVerboseBoot()) {
        esp3d_commands.dispatch(stmp.c_str(), ESP3DClientType::all_clients,
                                no_id, ESP3DMessageType::unique,
                                ESP3DClientType::system,
                                ESP3DAuthenticationLevel::admin);
      }
    }
  }
#endif  // FTP_FEATURE

#ifdef WS_DATA_FEATURE
  uint32_t ws_port = ESP3DSettings::readUint32(ESP_WEBSOCKET_PORT);
  if (!websocket_data_server.begin(ws_port)) {
    res = false;
    esp3d_log_e("WebSocket data server failed to start on port %d", ws_port);
    esp3d_commands.dispatch("Failed start WebSocket data server",
                            ESP3DClientType::all_clients, no_id,
                            ESP3DMessageType::unique, ESP3DClientType::system,
                            ESP3DAuthenticationLevel::admin);
  } else {
    if (websocket_data_server.started()) {
      String stmp = String("WebSocket data server started port ") +
                    String(websocket_data_server.port());
      esp3d_log("%s", stmp.c_str());
      if (ESP3DSettings::isVerboseBoot()) {
        esp3d_commands.dispatch(stmp.c_str(), ESP3DClientType::all_clients,
                                no_id, ESP3DMessageType::unique,
                                ESP3DClientType::system,
                                ESP3DAuthenticationLevel::admin);
      }
    }
  }
#endif  // WS_DATA_FEATURE

#ifdef WEBDAV_FEATURE
  if (!webdav_server.begin()) {
    res = false;
    esp3d_log_e("WebDAV server failed to start");
    esp3d_commands.dispatch("Failed start WebDAV server",
                            ESP3DClientType::all_clients, no_id,
                            ESP3DMessageType::unique, ESP3DClientType::system,
                            ESP3DAuthenticationLevel::admin);
  } else {
    if (webdav_server.started()) {
      String stmp = String("WebDAV server started port ") + String(webdav_server.port());
      esp3d_log("%s", stmp.c_str());
      if (ESP3DSettings::isVerboseBoot()) {
        esp3d_commands.dispatch(stmp.c_str(), ESP3DClientType::all_clients,
                                no_id, ESP3DMessageType::unique,
                                ESP3DClientType::system,
                                ESP3DAuthenticationLevel::admin);
      }
    }
  }
#endif  // WEBDAV_FEATURE

#ifdef HTTP_FEATURE
  if (!websocket_terminal_server.begin()) {
    res = false;
    esp3d_log_e("WebSocket terminal server failed to start");
    esp3d_commands.dispatch("Failed start WebSocket terminal server",
                            ESP3DClientType::all_clients, no_id,
                            ESP3DMessageType::unique, ESP3DClientType::system,
                            ESP3DAuthenticationLevel::admin);
  } else {
    if (websocket_terminal_server.started()) {
      String stmp = String("WebSocket terminal server started port ") +
                    String(websocket_terminal_server.port());
      esp3d_log("%s", stmp.c_str());
      if (ESP3DSettings::isVerboseBoot()) {
        esp3d_commands.dispatch(stmp.c_str(), ESP3DClientType::all_clients,
                                no_id, ESP3DMessageType::unique,
                                ESP3DClientType::system,
                                ESP3DAuthenticationLevel::admin);
      }
    }
  }
#endif  // HTTP_FEATURE

#if defined(MDNS_FEATURE) && defined(HTTP_FEATURE)
  if (HTTP_Server::started()) {
    esp3d_mDNS.addESP3DServices(HTTP_Server::port());
    esp3d_log("mDNS services added for HTTP port %d", HTTP_Server::port());
  }
#endif  // MDNS_FEATURE && HTTP_FEATURE

#ifdef SSDP_FEATURE
  if (WiFi.getMode() != WIFI_AP && HTTP_Server::started()) {
    String stmp = String(ESP3DHal::getChipID());
    SSDP.setSchemaURL("description.xml");
    SSDP.setHTTPPort(HTTP_Server::port());
    SSDP.setName(hostname.c_str());
    SSDP.setURL("/");
    SSDP.setDeviceType("upnp:rootdevice");
    SSDP.setSerialNumber(stmp.c_str());
    SSDP.setModelName(ESP_MODEL_NAME);
#if defined(ESP_MODEL_DESCRIPTION)
    SSDP.setModelDescription(ESP_MODEL_DESCRIPTION);
#endif  // ESP_MODEL_DESCRIPTION
    SSDP.setModelURL(ESP_MODEL_URL);
    SSDP.setModelNumber(ESP_MODEL_NUMBER);
    SSDP.setManufacturer(ESP_MANUFACTURER_NAME);
    SSDP.setManufacturerURL(ESP_MANUFACTURER_URL);
    if (SSDP.begin()) {
      String ssdpMsg = String("SSDP started with '") + hostname + "'";
      esp3d_log("SSDP started with hostname %s", hostname.c_str());
      if (ESP3DSettings::isVerboseBoot()) {
        esp3d_commands.dispatch(ssdpMsg.c_str(), ESP3DClientType::all_clients, no_id,
                                ESP3DMessageType::unique, ESP3DClientType::system,
                                ESP3DAuthenticationLevel::admin);
      }
    } else {
      esp3d_log_e("Failed to start SSDP");
    }
  }
#endif  // SSDP_FEATURE

#ifdef NOTIFICATION_FEATURE
  if (!notificationsservice.begin()) {
    esp3d_log_e("Failed to start notifications service");
  } else {
    notificationsservice.sendAutoNotification(NOTIFICATION_ESP_ONLINE);
    esp3d_log("Notifications service started");
  }
#endif  // NOTIFICATION_FEATURE

#ifdef CAMERA_DEVICE
  if (!esp3d_camera.begin()) {
    res = false;
    esp3d_log_e("Failed to start camera streaming server");
    esp3d_commands.dispatch("Failed start camera streaming server",
                            ESP3DClientType::all_clients, no_id,
                            ESP3DMessageType::unique, ESP3DClientType::system,
                            ESP3DAuthenticationLevel::admin);
  } else {
    esp3d_log("Camera streaming server started");
  }
#endif  // CAMERA_DEVICE

#if COMMUNICATION_PROTOCOL == MKS_SERIAL
  if (!MKSService::begin()) {
    res = false;
    esp3d_log_e("Failed to start MKS service");
  } else {
    esp3d_log("MKS service started");
  }
#endif  // COMMUNICATION_PROTOCOL == MKS_SERIAL

  if (!res) {
    esp3d_log_e("One or more services failed to start, stopping all services");
    end();
  } else {
    esp3d_log("All services started successfully");
  }

  ESP3DHal::wait(1000);
#if COMMUNICATION_PROTOCOL != MKS_SERIAL
  String ipMsg = String("Network IPs - AP: ") + NetConfig::softAPIP() +
                 ", STA: " + NetConfig::localIP();
  esp3d_log("%s", ipMsg.c_str());
  esp3d_commands.dispatch(ipMsg.c_str(), ESP3DClientType::all_clients, no_id,
                          ESP3DMessageType::unique, ESP3DClientType::system,
                          ESP3DAuthenticationLevel::admin);
#endif  // COMMUNICATION_PROTOCOL != MKS_SERIAL

  _started = res;
  return _started;
}

void NetServices::end() {
  _restart = false;
  if (!_started) {
    esp3d_log("Services already stopped");
    return;
  }
  esp3d_log("Stopping all services");
  _started = false;

#if COMMUNICATION_PROTOCOL == MKS_SERIAL
  MKSService::end();
  esp3d_log("MKS service stopped");
#endif  // COMMUNICATION_PROTOCOL == MKS_SERIAL

#ifdef CAMERA_DEVICE
  esp3d_camera.end();
  esp3d_log("Camera streaming server stopped");
#endif  // CAMERA_DEVICE

#ifdef NOTIFICATION_FEATURE
  notificationsservice.end();
  esp3d_log("Notifications service stopped");
#endif  // NOTIFICATION_FEATURE

#ifdef CAPTIVE_PORTAL_FEATURE
  if (NetConfig::getMode() == ESP_AP_SETUP) {
    dnsServer.stop();
    esp3d_log("Captive Portal stopped");
  }
#endif  // CAPTIVE_PORTAL_FEATURE

#ifdef SSDP_FEATURE
#if defined(ARDUINO_ARCH_ESP32)
  SSDP.end();
  esp3d_log("SSDP stopped");
#endif  // ARDUINO_ARCH_ESP32
#endif  // SSDP_FEATURE

#ifdef MDNS_FEATURE
  esp3d_mDNS.end();
  esp3d_log("mDNS stopped");
#endif  // MDNS_FEATURE

#ifdef OTA_FEATURE
#if defined(ARDUINO_ARCH_ESP32)
  if (WiFi.getMode() != WIFI_AP) {
    ArduinoOTA.end();
    esp3d_log("OTA service stopped");
  }
#endif  // ARDUINO_ARCH_ESP32
#endif  // OTA_FEATURE

#ifdef HTTP_FEATURE
  websocket_terminal_server.end();
  esp3d_log("WebSocket terminal server stopped");
#endif  // HTTP_FEATURE

#ifdef WEBDAV_FEATURE
  webdav_server.end();
  esp3d_log("WebDAV server stopped");
#endif  // WEBDAV_FEATURE

#ifdef HTTP_FEATURE
  HTTP_Server::end();
  esp3d_log("HTTP server stopped");
#endif  // HTTP_FEATURE

#ifdef WS_DATA_FEATURE
  websocket_data_server.end();
  esp3d_log("WebSocket data server stopped");
#endif  // WS_DATA_FEATURE

#ifdef TELNET_FEATURE
  telnet_server.end();
  esp3d_log("Telnet server stopped");
#endif  // TELNET_FEATURE

#ifdef FTP_FEATURE
  ftp_server.end();
  esp3d_log("FTP server stopped");
#endif  // FTP_FEATURE
}

void NetServices::handle() {
  if (_started) {
    esp3d_log("Handling network services");
#if COMMUNICATION_PROTOCOL == MKS_SERIAL
    MKSService::handle();
#endif  // COMMUNICATION_PROTOCOL == MKS_SERIAL
#ifdef MDNS_FEATURE
    esp3d_mDNS.handle();
#endif  // MDNS_FEATURE
#ifdef OTA_FEATURE
    ArduinoOTA.handle();
#endif  // OTA_FEATURE
#ifdef CAPTIVE_PORTAL_FEATURE
    if (NetConfig::getMode() == ESP_AP_SETUP) {
      dnsServer.processNextRequest();
    }
#endif  // CAPTIVE_PORTAL_FEATURE
#ifdef HTTP_FEATURE
    HTTP_Server::handle();
#endif  // HTTP_FEATURE
#ifdef WEBDAV_FEATURE
    webdav_server.handle();
#endif  // WEBDAV_FEATURE
#ifdef WS_DATA_FEATURE
    websocket_data_server.handle();
#endif  // WS_DATA_FEATURE
#ifdef HTTP_FEATURE
    websocket_terminal_server.handle();
#endif  // HTTP_FEATURE
#ifdef TELNET_FEATURE
    telnet_server.handle();
#endif  // TELNET_FEATURE
#ifdef FTP_FEATURE
    ftp_server.handle();
#endif  // FTP_FEATURE
#ifdef NOTIFICATION_FEATURE
    notificationsservice.handle();
#endif  // NOTIFICATION_FEATURE
#if defined(TIMESTAMP_FEATURE) && \
    (defined(ESP_GOT_IP_HOOK) || defined(ESP_GOT_DATE_TIME_HOOK))
    timeService.handle();
#endif  // TIMESTAMP_FEATURE
  }
  if (_restart) {
    esp3d_log("Restarting services");
    begin();
  }
}