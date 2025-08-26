/*
  websocket_server.cpp - websocket functions class

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

#if defined(HTTP_FEATURE) || defined(WS_DATA_FEATURE) || \
    (defined(ESP_LOG_FEATURE) && ESP_LOG_FEATURE == LOG_OUTPUT_WEBSOCKET)

#include <WebSocketsServer.h>

#ifdef ARDUINO_ARCH_ESP8266
#include <ESP8266WiFi.h> // Use ESP8266WiFi for ESP8266
#else
#include <WiFi.h> // Use WiFi for ESP32
#endif

#include "../../core/esp3d_commands.h"
#include "../../core/esp3d_message.h"
#include "../../core/esp3d_settings.h"
#include "../../core/esp3d_string.h"
#include "../authentication/authentication_service.h"
#include "websocket_server.h"

WebSocket_Server websocket_terminal_server("webui-v3",
                                           ESP3DClientType::webui_websocket);
#if defined(WS_DATA_FEATURE)
WebSocket_Server websocket_data_server("arduino", ESP3DClientType::websocket);
#endif  // WS_DATA_FEATURE

bool WebSocket_Server::pushMSG(const char *data) {
  if (_websocket_server) {
    esp3d_log("[%u]Broadcast %s on port %d", _current_id, data, _port);
    return _websocket_server->broadcastTXT(data);
  }
  esp3d_log_e("Cannot broadcast: server not initialized");
  return false;
}

void WebSocket_Server::enableOnly(uint num) {
  if (_websocket_server) {
    for (uint8_t i = 0; i < WEBSOCKETS_SERVER_CLIENT_MAX; i++) {
      if (i != num && _websocket_server->clientIsConnected(i)) {
        _websocket_server->disconnect(i);
        esp3d_log("Disconnected client %d to enable client %d", i, num);
      }
    }
  }
}

bool WebSocket_Server::dispatch(ESP3DMessage *message) {
  if (!message || !_started) {
    esp3d_log_e("Cannot dispatch: server not started or message null");
    return false;
  }
  if (message->size > 0 && message->data) {
    size_t sentcnt = writeBytes(message->data, message->size);
    if (sentcnt != message->size) {
      esp3d_log_e("Failed to send %d bytes, sent %d", message->size, sentcnt);
      return false;
    }
    esp3d_message_manager.deleteMsg(message);
    return true;
  }
  return false;
}

bool WebSocket_Server::pushMSG(uint num, const char *data) {
  if (_websocket_server) {
    esp3d_log("[%u]Send %s on port %d", num, data, _port);
    return _websocket_server->sendTXT(num, data);
  }
  esp3d_log_e("Cannot send to client %d: server not initialized", num);
  return false;
}

bool WebSocket_Server::isConnected() {
  if (_websocket_server) {
    return _websocket_server->connectedClients() > 0;
  }
  return false;
}

void WebSocket_Server::closeClients() {
  if (_websocket_server) {
    _websocket_server->disconnect();
    esp3d_log("Closed all WebSocket clients on port %d", _port);
  }
}

#if defined(WS_DATA_FEATURE)
// Events for WebSocket bridge
void handle_Websocket_Server_Event(uint8_t num, uint8_t type, uint8_t *payload,
                                   size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      esp3d_log("[%u] Disconnected from port %d", num,
                websocket_data_server.port());
      break;
    case WStype_CONNECTED: {
      websocket_data_server.initAuthentication();
      esp3d_log("[%u] Connected to port %d, URL: %s", num,
                websocket_data_server.port(), payload);
    } break;
    case WStype_TEXT:
      esp3d_log("[%u] Received Text: %s on port %d", num, payload,
                websocket_data_server.port());
      websocket_data_server.push2RXbuffer(payload, length);
      break;
    case WStype_BIN:
      esp3d_log("[%u] Received binary length: %u on port %d", num, length,
                websocket_data_server.port());
      websocket_data_server.push2RXbuffer(payload, length);
      break;
    default:
      esp3d_log("Unknown WebSocket event type %d on port %d", type,
                websocket_data_server.port());
      break;
  }
}
#endif  // WS_DATA_FEATURE

#if defined(HTTP_FEATURE)
// Events for WebSocket used in WebUI for events and serial bridge
void handle_Websocket_Terminal_Event(uint8_t num, uint8_t type,
                                     uint8_t *payload, size_t length) {
  String msg;
  switch (type) {
    case WStype_DISCONNECTED:
      esp3d_log("[%u] Disconnected from port %d", num,
                websocket_terminal_server.port());
      break;
    case WStype_CONNECTED: {
      esp3d_log("[%u] Connected to port %d, URL: %s", num,
                websocket_terminal_server.port(), (const char *)payload);
      msg = "currentID:" + String(num);
      websocket_terminal_server.set_currentID(num);
      websocket_terminal_server.pushMSG(num, msg.c_str());
      msg = "activeID:" + String(num);
      websocket_terminal_server.pushMSG(msg.c_str());
      websocket_terminal_server.enableOnly(num);
      esp3d_log("[%u] WebSocket connected on port %d", num,
                websocket_terminal_server.port());
    } break;
    case WStype_TEXT:
#if defined(AUTHENTICATION_FEATURE)
      // Handle ping for session timeout
      if (AuthenticationService::getSessionTimeout() != 0) {
        msg = (const char *)payload;
        if (msg.startsWith("PING:")) {
          String session = msg.substring(5);
          String response =
              "PING:" + String(AuthenticationService::getSessionRemaining(
                            session.c_str()));
          response += ":" + String(AuthenticationService::getSessionTimeout());
          websocket_terminal_server.pushMSG(num, response.c_str());
          esp3d_log("[%u] Sent PING response on port %d", num,
                    websocket_terminal_server.port());
        }
      }
#else
      esp3d_log("[%u] Ignored Text: %s on port %d", num, payload,
                websocket_terminal_server.port());
#endif  // AUTHENTICATION_FEATURE
      break;
    case WStype_BIN:
      esp3d_log("[%u] Ignored binary length: %u on port %d", num, length,
                websocket_terminal_server.port());
      break;
    default:
      esp3d_log("Unknown WebSocket event type %d on port %d", type,
                websocket_terminal_server.port());
      break;
  }
}
#endif  // HTTP_FEATURE

int WebSocket_Server::available() { return _RXbufferSize; }

int WebSocket_Server::availableForWrite() {
  return TXBUFFERSIZE - _TXbufferSize;
}

WebSocket_Server::WebSocket_Server(const char *protocol, ESP3DClientType type) {
  _websocket_server = nullptr;
  _started = false;
  _port = 0;
  _current_id = 0;
  _RXbuffer = nullptr;
  _RXbufferSize = 0;
  _protocol = protocol;
  _type = type;
  initAuthentication();
}

WebSocket_Server::~WebSocket_Server() { end(); }

bool WebSocket_Server::begin(uint16_t port) {
  end();
  if (port == 0) {
    if (_type == ESP3DClientType::webui_websocket) {
      _port = ESP3DSettings::readUint32(ESP_HTTP_PORT) + 1;
      esp3d_log("Starting WebSocket terminal server on port %d", _port);
    } else {
      _port = ESP3DSettings::readUint32(ESP_WEBSOCKET_PORT);
      esp3d_log("Starting WebSocket data server on port %d", _port);
      if (ESP3DSettings::readByte(ESP_WEBSOCKET_ON) == 0) {
        esp3d_log("WebSocket data server disabled in settings");
        return true;
      }
    }
  } else {
    _port = port;
    esp3d_log("Starting WebSocket server on custom port %d", _port);
  }
  _websocket_server = new WebSocketsServer(_port, "", _protocol.c_str());
  if (!_websocket_server) {
    esp3d_log_e("Failed to create WebSocket server on port %d", _port);
    end();
    return false;
  }
  _websocket_server->begin();
  // Support APSTA mode by ensuring server listens on both interfaces
  #if defined(ARDUINO_ARCH_ESP8266)
  if (WiFi.getMode() == WIFI_AP_STA) {
    esp3d_log("WebSocket server configured for APSTA mode on port %d", _port);
  }
  #endif
  #if defined(HTTP_FEATURE)
  if (_type == ESP3DClientType::webui_websocket) {
    _websocket_server->onEvent(handle_Websocket_Terminal_Event);
    esp3d_log("Registered terminal event handler for port %d", _port);
  }
  #endif
  #if defined(WS_DATA_FEATURE)
  if (_type == ESP3DClientType::websocket && _protocol != "log") {
    _websocket_server->onEvent(handle_Websocket_Server_Event);
    _RXbuffer = (uint8_t *)malloc(RXBUFFERSIZE + 1);
    if (!_RXbuffer) {
      esp3d_log_e("Failed to allocate RX buffer for WebSocket data server");
      end();
      return false;
    }
    esp3d_log("Allocated RX buffer for WebSocket data server");
  }
  #endif
  _started = true;
  esp3d_log("WebSocket server started on port %d", _port);
  return _started;
}

void WebSocket_Server::end() {
  _current_id = 0;
  _TXbufferSize = 0;
  if (_RXbuffer) {
    free(_RXbuffer);
    _RXbuffer = nullptr;
  }
  _RXbufferSize = 0;
  if (_websocket_server) {
    _websocket_server->close();
    delete _websocket_server;
    _websocket_server = nullptr;
    esp3d_log("WebSocket server closed on port %d", _port);
    _port = 0;
  }
  _started = false;
  initAuthentication();
}

WebSocket_Server::operator bool() const { return _started; }

void WebSocket_Server::set_currentID(uint8_t current_id) {
  _current_id = current_id;
}

uint8_t WebSocket_Server::get_currentID() { return _current_id; }

size_t WebSocket_Server::writeBytes(const uint8_t *buffer, size_t size) {
  if (_started) {
    if ((buffer == nullptr) || (!_websocket_server) || (size == 0)) {
      esp3d_log_e("Cannot write: invalid buffer or server not initialized");
      return 0;
    }
    if (_TXbufferSize == 0) {
      _lastTXflush = millis();
    }
    // Send full line
    if (_TXbufferSize + size > TXBUFFERSIZE) {
      flushTXbuffer();
    }
    if (_websocket_server->connectedClients() == 0) {
      esp3d_log_e("No clients connected for write on port %d", _port);
      return 0;
    }
    // Need periodic check to force flush in case of no end
    for (uint i = 0; i < size; i++) {
      // Add a sanity check to avoid buffer overflow
      if (_TXbufferSize >= TXBUFFERSIZE) {
        flushTXbuffer();
      }
      _TXbuffer[_TXbufferSize] = buffer[i];
      _TXbufferSize++;
    }
    esp3d_log("Buffered %d bytes for WebSocket on port %d", size, _port);
    return size;
  }
  esp3d_log_e("Cannot write: WebSocket server not started");
  return 0;
}

void WebSocket_Server::push2RXbuffer(uint8_t *sbuf, size_t len) {
  if (!_RXbuffer || !_started || !sbuf) {
    esp3d_log_e("Cannot push to RX buffer: server not started or buffer null");
    return;
  }
  for (size_t i = 0; i < len; i++) {
    _lastRXflush = millis();
    if (esp3d_string::isRealTimeCommand(sbuf[i])) {
      flushRXChar(sbuf[i]);
    } else {
      _RXbuffer[_RXbufferSize] = sbuf[i];
      _RXbufferSize++;
      if (_RXbufferSize > RXBUFFERSIZE || _RXbuffer[_RXbufferSize - 1] == '\n') {
        flushRXbuffer();
      }
    }
  }
}

void WebSocket_Server::initAuthentication() {
#if defined(AUTHENTICATION_FEATURE)
  _auth = ESP3DAuthenticationLevel::guest;
#else
  _auth = ESP3DAuthenticationLevel::admin;
#endif  // AUTHENTICATION_FEATURE
}

ESP3DAuthenticationLevel WebSocket_Server::getAuthentication() { return _auth; }

void WebSocket_Server::flushRXChar(char c) {
  flushRXData((uint8_t *)&c, 1, ESP3DMessageType::realtimecmd);
}

void WebSocket_Server::flushRXbuffer() {
  _RXbuffer[_RXbufferSize] = 0x0;
  flushRXData((uint8_t *)_RXbuffer, _RXbufferSize, ESP3DMessageType::unique);
  _RXbufferSize = 0;
}

void WebSocket_Server::flushRXData(const uint8_t *data, size_t size,
                                   ESP3DMessageType type) {
  if (!data || !_started) {
    esp3d_log_e("Cannot flush RX data: invalid data or server not started");
    return;
  }
  ESP3DMessage *message = esp3d_message_manager.newMsg(
      _type, esp3d_commands.getOutputClient(), data, size, _auth);
  if (message) {
    message->type = type;
    esp3d_log("Processing WebSocket message of size %d on port %d", size, _port);
    esp3d_commands.process(message);
  } else {
    esp3d_log_e("Cannot create WebSocket message");
  }
  _lastRXflush = millis();
}

void WebSocket_Server::handle() {
  ESP3DHal::wait(0);
  if (_started) {
    if (_TXbufferSize > 0) {
      if ((_TXbufferSize >= TXBUFFERSIZE) ||
          ((millis() - _lastTXflush) > FLUSHTIMEOUT)) {
        flushTXbuffer();
      }
    }
    if (_RXbufferSize > 0) {
      if ((_RXbufferSize >= RXBUFFERSIZE) ||
          ((millis() - _lastRXflush) > FLUSHTIMEOUT)) {
        flushRXbuffer();
      }
    }
    if (_websocket_server) {
      _websocket_server->loop();
      esp3d_log("WebSocket server loop executed on port %d", _port);
    }
  }
}

void WebSocket_Server::flush(void) {
  flushTXbuffer();
  flushRXbuffer();
}

void WebSocket_Server::flushTXbuffer(void) {
  if (_started) {
    if ((_TXbufferSize > 0) && (_websocket_server->connectedClients() > 0)) {
      if (_websocket_server) {
        _websocket_server->broadcastBIN(_TXbuffer, _TXbufferSize);
        esp3d_log("Broadcasted %d bytes on WebSocket port %d", _TXbufferSize, _port);
      }
      // Refresh timeout
      _lastTXflush = millis();
    }
  }
  // Reset buffer
  _TXbufferSize = 0;
}

#endif  // HTTP_FEATURE || WS_DATA_FEATURE