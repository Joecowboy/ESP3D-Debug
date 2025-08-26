/*
  esp3d_log.cpp - log esp3d functions class

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

#include "../include/esp3d_config.h"
#if defined(ESP_LOG_FEATURE)
#include <vector>
#include <string>
#include "../modules/telnet/telnet_server.h"
#include "../modules/websocket/websocket_server.h"

#ifndef LOG_ESP3D_BAUDRATE
#define LOG_ESP3D_BAUDRATE 115200
#endif

#define LOG_BUFFER_SIZE 1000 // Store up to 1000 log messages
static std::vector<std::string> log_buffer;
static size_t log_buffer_index = 0;

#if defined(ARDUINO_ARCH_ESP8266)
#define pathToFileName(p) p
#endif

#if (ESP_LOG_FEATURE == LOG_OUTPUT_SERIAL0) || \
    (ESP_LOG_FEATURE == LOG_OUTPUT_SERIAL1) || \
    (ESP_LOG_FEATURE == LOG_OUTPUT_SERIAL2)
#ifndef ESP3DLIB_ENV
#if ESP_LOG_FEATURE == LOG_OUTPUT_SERIAL0
#define LOG_OUTPUT_SERIAL Serial
#endif
#if ESP_LOG_FEATURE == LOG_OUTPUT_SERIAL1
#define LOG_OUTPUT_SERIAL Serial1
#endif
#if ESP_LOG_FEATURE == LOG_OUTPUT_SERIAL2
#define LOG_OUTPUT_SERIAL Serial2
#endif
#else
#define LOG_OUTPUT_SERIAL MYSERIAL1
#endif
#endif

#if ESP_LOG_FEATURE == LOG_OUTPUT_TELNET
Telnet_Server telnet_log;
#endif
#if ESP_LOG_FEATURE == LOG_OUTPUT_WEBSOCKET
WebSocket_Server websocket_log("log");
#endif

void esp3d_logf(uint8_t level, const char* format, ...) {
#if (((ESP_LOG_FEATURE == LOG_OUTPUT_SERIAL0) || \
      (ESP_LOG_FEATURE == LOG_OUTPUT_SERIAL1) || \
      (ESP_LOG_FEATURE == LOG_OUTPUT_SERIAL2)) && \
     !defined(ESP3DLIB_ENV))
  if (!LOG_OUTPUT_SERIAL.availableForWrite()) return;
#endif
#if ESP_LOG_FEATURE == LOG_OUTPUT_TELNET
  if (!telnet_log.started() || !telnet_log.isConnected()) return;
#endif
#if ESP_LOG_FEATURE == LOG_OUTPUT_WEBSOCKET
  if (!websocket_log.started()) return;
#endif

  size_t len = 0;
  char default_buffer[64];
  char* buffer_ptr = default_buffer;
  va_list arg;
  va_start(arg, format);
  va_list copy;
  va_copy(copy, arg);

  len = vsnprintf(NULL, 0, format, arg);
  va_end(copy);

  if (len >= sizeof(default_buffer)) {
    buffer_ptr = (char*)malloc((len + 1) * sizeof(char));
    if (buffer_ptr == NULL) return;
  }

  len = vsnprintf(buffer_ptr, len + 1, format, arg);

  // Store in buffer
  std::string log_message(buffer_ptr);
  if (log_buffer.size() < LOG_BUFFER_SIZE) {
    log_buffer.push_back(log_message);
  } else {
    log_buffer[log_buffer_index] = log_message;
    log_buffer_index = (log_buffer_index + 1) % LOG_BUFFER_SIZE;
  }

#if (((ESP_LOG_FEATURE == LOG_OUTPUT_SERIAL0) || \
      (ESP_LOG_FEATURE == LOG_OUTPUT_SERIAL1) || \
      (ESP_LOG_FEATURE == LOG_OUTPUT_SERIAL2)) && \
     !defined(ESP3DLIB_ENV))
  LOG_OUTPUT_SERIAL.write((uint8_t*)buffer_ptr, strlen(buffer_ptr));
  LOG_OUTPUT_SERIAL.write((uint8_t*)"\r\n", 2);
  LOG_OUTPUT_SERIAL.flush();
#endif
#if ESP_LOG_FEATURE == LOG_OUTPUT_TELNET
  telnet_log.writeBytes((uint8_t*)buffer_ptr, strlen(buffer_ptr));
  telnet_log.writeBytes((uint8_t*)"\r\n", 2);
#endif
#if ESP_LOG_FEATURE == LOG_OUTPUT_WEBSOCKET
  websocket_log.writeBytes((uint8_t*)buffer_ptr, strlen(buffer_ptr));
  websocket_log.writeBytes((uint8_t*)"\r\n", 2);
#endif

  va_end(arg);
  if (buffer_ptr != default_buffer) {
    free(buffer_ptr);
  }
}

void esp3d_log_init() {
#if (ESP_LOG_FEATURE == LOG_OUTPUT_SERIAL0) || \
    (ESP_LOG_FEATURE == LOG_OUTPUT_SERIAL1) || \
    (ESP_LOG_FEATURE == LOG_OUTPUT_SERIAL2)
#ifdef ARDUINO_ARCH_ESP8266
  LOG_OUTPUT_SERIAL.begin(LOG_ESP3D_BAUDRATE, SERIAL_8N1, SERIAL_FULL,
                          (ESP_LOG_TX_PIN == -1) ? 1 : ESP_LOG_TX_PIN);
#if ESP_LOG_RX_PIN != -1
  LOG_OUTPUT_SERIAL.pins((ESP_LOG_TX_PIN == -1) ? 1 : ESP_LOG_TX_PIN, ESP_LOG_RX_PIN);
#endif
#endif
#if defined(ARDUINO_ARCH_ESP32)
  LOG_OUTPUT_SERIAL.begin(LOG_ESP3D_BAUDRATE, SERIAL_8N1, ESP_LOG_RX_PIN, ESP_LOG_TX_PIN);
#endif
#endif
  log_buffer.reserve(LOG_BUFFER_SIZE);
}

void esp3d_network_log_init() {
#if ESP_LOG_FEATURE == LOG_OUTPUT_TELNET
  telnet_log.begin(LOG_ESP3D_OUTPUT_PORT, true);
#endif
#if ESP_LOG_FEATURE == LOG_OUTPUT_WEBSOCKET
  websocket_log.begin(LOG_ESP3D_OUTPUT_PORT);
#endif
}

void esp3d_network_log_handle() {
#if ESP_LOG_FEATURE == LOG_OUTPUT_TELNET
  telnet_log.handle();
#endif
#if ESP_LOG_FEATURE == LOG_OUTPUT_WEBSOCKET
  websocket_log.handle();
#endif
}

void esp3d_network_log_end() {
#if ESP_LOG_FEATURE == LOG_OUTPUT_TELNET
  telnet_log.end();
#endif
#if ESP_LOG_FEATURE == LOG_OUTPUT_WEBSOCKET
  websocket_log.end();
#endif
}

std::vector<std::string> esp3d_get_log_buffer() {
  std::vector<std::string> ordered_buffer;
  ordered_buffer.reserve(log_buffer.size());
  for (size_t i = 0; i < log_buffer.size(); ++i) {
    size_t index = (log_buffer_index + i) % log_buffer.size();
    if (index < log_buffer.size()) {
      ordered_buffer.push_back(log_buffer[index]);
    }
  }
  return ordered_buffer;
}

void esp3d_clear_log_buffer() {
  log_buffer.clear();
  log_buffer_index = 0;
}

#endif  // ESP_LOG_FEATURE