/*
  telnet_server.cpp - telnet server functions class

  Copyright (c) 2014 Luc Lebosse. All rights reserved.

  This code is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This code is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with This code; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "../../include/esp3d_config.h"

#if defined(TELNET_FEATURE) || \
    (defined(ESP_LOG_FEATURE) && ESP_LOG_FEATURE == LOG_OUTPUT_TELNET)

#include "telnet_server.h" // Added missing include
#include "../../core/esp3d_commands.h"
#include "../../core/esp3d_message.h"
#include "../../core/esp3d_settings.h"
#include "../../core/esp3d_string.h"
#include "../../include/esp3d_version.h"
#ifdef ARDUINO_ARCH_ESP8266
#include <ESP8266WiFi.h> // Use ESP8266WiFi for ESP8266
#else
#include <WiFi.h> // Use WiFi for ESP32
#endif

Telnet_Server telnet_server;

#define TIMEOUT_TELNET_FLUSH 1500

#if defined(AUTHENTICATION_FEATURE)
#define TELNET_WELCOME_MESSAGE         \
  ";Welcome to ESP3D V" FW_VERSION \
  ", please enter a command with credentials.\r\n"
#else
#define TELNET_WELCOME_MESSAGE ";Welcome to ESP3D V" FW_VERSION ".\r\n"
#endif  // AUTHENTICATION_FEATURE

void Telnet_Server::closeClient() {
  if (_telnetClients) {
    _telnetClients.stop();
  }
}

bool Telnet_Server::isConnected() {
  if (!_started || _telnetserver == NULL) {
    return false;
  }
  // Check if there are any new clients
  if (_telnetserver->hasClient()) {
    // Find free/disconnected spot
    if (!_telnetClients || !_telnetClients.connected()) {
      if (_telnetClients) {
        _telnetClients.stop();
      }
      _telnetClients = _telnetserver->accept();
      esp3d_log("New Telnet client connected from %s", _telnetClients.remoteIP().toString().c_str());
#ifndef DISABLE_TELNET_WELCOME_MESSAGE
      // New client
      writeBytes((uint8_t *)TELNET_WELCOME_MESSAGE, strlen(TELNET_WELCOME_MESSAGE));
#endif  // DISABLE_TELNET_WELCOME_MESSAGE
      initAuthentication();
    }
  }
  if (_telnetserver->hasClient()) {
    // No free/disconnected spot, so reject
    _telnetserver->accept().stop();
    esp3d_log("Rejected additional Telnet client");
  }
  return _telnetClients.connected();
}

const char *Telnet_Server::clientIPAddress() {
  static String res;
  res = "0.0.0.0";
  if (_telnetClients && _telnetClients.connected()) {
    res = _telnetClients.remoteIP().toString();
  }
  return res.c_str();
}

Telnet_Server::Telnet_Server() {
  _buffer_size = 0;
  _started = false;
  _isdebug = false;
  _port = 0;
  _buffer = nullptr;
  _telnetserver = nullptr;
  initAuthentication();
}

Telnet_Server::~Telnet_Server() { end(); }

/**
 * Begin Telnet setup
 */
bool Telnet_Server::begin(uint16_t port, bool debug) {
  end();
  if (ESP3DSettings::readByte(ESP_TELNET_ON) != 1) {
    esp3d_log("Telnet disabled in settings");
    return true;
  }
  // Get Telnet port
  if (port == 0) {
    _port = ESP3DSettings::readUint32(ESP_TELNET_PORT);
  } else {
    _port = port;
  }
  esp3d_log("Starting Telnet server on port %d", _port);
  _isdebug = debug;
  if (!_isdebug) {
    _buffer = (uint8_t *)malloc(ESP3D_TELNET_BUFFER_SIZE + 1);
    if (!_buffer) {
      esp3d_log_e("Failed to allocate Telnet buffer");
      return false;
    }
  }
  // Create instance
  _telnetserver = new WiFiServer(_port);
  if (!_telnetserver) {
    esp3d_log_e("Failed to create Telnet server");
    if (_buffer) {
      free(_buffer);
      _buffer = nullptr;
    }
    return false;
  }
  _telnetserver->setNoDelay(true);
  // Start Telnet server
  _telnetserver->begin();
  // Support APSTA mode by ensuring server listens on both interfaces
  if (WiFi.getMode() == WIFI_AP_STA) {
    _telnetserver->setNoDelay(true); // Ensure optimal performance
    esp3d_log("Telnet server configured for APSTA mode");
  }
  _started = true;
  _lastflush = millis();
  esp3d_log("Telnet server started on port %d", _port);
  return _started;
}

/**
 * End Telnet
 */
void Telnet_Server::end() {
  _started = false;
  _buffer_size = 0;
  _port = 0;
  _isdebug = false;
  closeClient();
  if (_telnetserver) {
    delete _telnetserver;
    _telnetserver = nullptr;
  }
  if (_buffer) {
    free(_buffer);
    _buffer = nullptr;
  }
#if defined(AUTHENTICATION_FEATURE)
  _auth = ESP3DAuthenticationLevel::guest;
#else
  _auth = ESP3DAuthenticationLevel::admin;
#endif  // AUTHENTICATION_FEATURE
}

/**
 * Reset Telnet
 */
bool Telnet_Server::reset() {
  // Nothing to reset
  return true;
}

bool Telnet_Server::started() { return _started; }

void Telnet_Server::handle() {
  ESP3DHal::wait(0);
  if (isConnected()) {
    // Check clients for data
    size_t len = _telnetClients.available();
    if (len > 0) {
      // If yes, read them
      uint8_t *sbuf = (uint8_t *)malloc(len);
      if (sbuf) {
        size_t count = _telnetClients.read(sbuf, len);
        // Push to buffer
        if (count > 0) {
          esp3d_log("Received %d bytes from Telnet client", count);
          push2buffer(sbuf, count);
        }
        // Free buffer
        free(sbuf);
      } else {
        esp3d_log_e("Failed to allocate buffer for Telnet data");
      }
    }
  }
  // We cannot leave data in buffer too long
  // in case some commands "forget" to add \n
  if (((millis() - _lastflush) > TIMEOUT_TELNET_FLUSH) && (_buffer_size > 0)) {
    flushBuffer();
  }
}

bool Telnet_Server::dispatch(ESP3DMessage *message) {
  if (!message || !_started) {
    esp3d_log_e("Cannot dispatch message: server not started or message null");
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

void Telnet_Server::initAuthentication() {
#if defined(AUTHENTICATION_FEATURE)
  _auth = ESP3DAuthenticationLevel::guest;
#else
  _auth = ESP3DAuthenticationLevel::admin;
#endif  // AUTHENTICATION_FEATURE
}

ESP3DAuthenticationLevel Telnet_Server::getAuthentication() { return _auth; }

void Telnet_Server::flushData(const uint8_t *data, size_t size, ESP3DMessageType type) {
  ESP3DMessage *message = esp3d_message_manager.newMsg(
      ESP3DClientType::telnet, esp3d_commands.getOutputClient(), data, size, _auth);
  if (message) {
    message->type = type;
    esp3d_log("Processing Telnet message of size %d", size);
    esp3d_commands.process(message);
  } else {
    esp3d_log_e("Cannot create Telnet message");
  }
  _lastflush = millis();
}

void Telnet_Server::flushChar(char c) {
  flushData((uint8_t *)&c, 1, ESP3DMessageType::realtimecmd);
}

void Telnet_Server::flushBuffer() {
  _buffer[_buffer_size] = 0x0;
  flushData((uint8_t *)_buffer, _buffer_size, ESP3DMessageType::unique);
  _buffer_size = 0;
}

void Telnet_Server::push2buffer(uint8_t *sbuf, size_t len) {
  if (!_buffer || !_started) {
    esp3d_log_e("Cannot push to buffer: server not started or buffer null");
    return;
  }
  for (size_t i = 0; i < len; i++) {
    _lastflush = millis();
    if (esp3d_string::isRealTimeCommand(sbuf[i])) {
      flushChar(sbuf[i]);
    } else {
      _buffer[_buffer_size] = sbuf[i];
      _buffer_size++;
      if (_buffer_size > ESP3D_TELNET_BUFFER_SIZE || _buffer[_buffer_size - 1] == '\n') {
        flushBuffer();
      }
    }
  }
}

size_t Telnet_Server::writeBytes(const uint8_t *buffer, size_t size) {
  if (isConnected() && (size > 0) && _started) {
    if ((size_t)availableForWrite() >= size) {
      // Push data to connected Telnet client
      size_t sent = _telnetClients.write(buffer, size);
      esp3d_log("Sent %d bytes to Telnet client", sent);
      return sent;
    } else {
      size_t sizetosend = size;
      size_t sizesent = 0;
      uint8_t *buffertmp = (uint8_t *)buffer;
      uint32_t starttime = millis();
      // Loop until all is sent or timeout
      while (sizetosend > 0 && ((millis() - starttime) < 100)) {
        size_t available = availableForWrite();
        if (available > 0) {
          // In case less is sent
          available = _telnetClients.write(
              &buffertmp[sizesent],
              (available >= sizetosend) ? sizetosend : available);
          sizetosend -= available;
          sizesent += available;
          starttime = millis();
          esp3d_log("Sent %d bytes, %d remaining", available, sizetosend);
        } else {
          ESP3DHal::wait(5);
        }
      }
      return sizesent;
    }
  }
  esp3d_log_e("Cannot write: not connected or zero size");
  return 0;
}

int Telnet_Server::availableForWrite() {
  if (!isConnected()) {
    return 0;
  }
#ifdef ARDUINO_ARCH_ESP32
  return 128;  // Hard code for ESP32
#endif         // ARDUINO_ARCH_ESP32
#ifdef ARDUINO_ARCH_ESP8266
  return _telnetClients.availableForWrite();
#endif  // ARDUINO_ARCH_ESP8266
}

int Telnet_Server::available() {
  if (isConnected()) {
    return _telnetClients.available();
  }
  return 0;
}

size_t Telnet_Server::readBytes(uint8_t *sbuf, size_t len) {
  if (isConnected()) {
    if (_telnetClients.available() > 0) {
      return _telnetClients.read(sbuf, len);
    }
  }
  return 0;
}

void Telnet_Server::flush() { _telnetClients.flush(); }

#endif  // TELNET_FEATURE