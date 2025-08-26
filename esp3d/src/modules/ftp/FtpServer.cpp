/*
 * FTP Server for Arduino Due or Mega 2580
 * and Ethernet shield W5100, W5200 or W5500
 * or for Esp8266 with external SD card or SpiFfs
 * Copyright (c) 2014-2018 by Jean-Michel Gallego
 *
 * Modified version for ESP3D by Luc LEBOSSE @luc-github
 * support for ESP8266 and ESP32 in ESP3D project
 */
#include "../../include/esp3d_config.h"
#if defined(FTP_FEATURE)
#include "FtpServer.h"
#include "../../core/esp3d_log.h"
#include "../filesystem/esp_filesystem.h"
#if defined(SD_DEVICE)
#include "../filesystem/esp_sd.h"
#endif
#include <Arduino.h>
#include <FS.h>
#if ESP_FILESYSTEM_TYPE == ESP_FILESYSTEM_LITTLEFS
#include <LittleFS.h>
#define FILESYSTEM LittleFS
#elif ESP_FILESYSTEM_TYPE == ESP_SPIFFS_FILESYSTEM
#include <SPIFFS.h>
#define FILESYSTEM SPIFFS
#else
#error "No valid filesystem defined (ESP_FILESYSTEM_LITTLEFS or ESP_SPIFFS_FILESYSTEM)"
#endif
#if defined(HTTP_FEATURE)
#include "../http/http_server.h"
#endif
#if defined(WS_DATA_FEATURE)
#include "../websocket/websocket_server.h"
#endif

FtpServer ftp_server;

void FtpServer::handle() {
  if (!_started || !ftpServer) return;
  if (!client.connected()) {
    client = ftpServer->available();
    if (client) {
      clientConnected();
    }
  }
  if (client.connected()) {
    if (cmdStage == FTP_Stop) return;
    if (millis() - millisEndConnection > FTP_TIME_OUT * 1000) {
      disconnectClient();
      return;
    }
    if (client.available()) {
      processCommand();
      millisEndConnection = millis(); // Reset inactivity timer
    }
  }
  // Auto-disable FTP after 5 minutes of inactivity
  if (_started && millis() - millisEndConnection > 5 * 60 * 1000) {
    esp3d_log("FTP inactive for 5 minutes, disabling");
    disableFtp();
  }
}

void FtpServer::closeClient() {
  if (client.connected()) {
    abortTransfer();
    client.println("221 Goodbye");
    client.stop();
    cmdStage = FTP_Stop;
  }
}

void FtpServer::enableFtp() {
  if (_started) return;
  if (!ESP3DSettings::writeByte(static_cast<int>(ESP3DSettingIndex::esp3d_ftp_on), 1)) {
    esp3d_log_e("Failed to enable FTP");
    return;
  }
#if defined(HTTP_FEATURE)
  if (!ESP3DSettings::writeByte(static_cast<int>(ESP3DSettingIndex::esp3d_http_on), 0)) {
    esp3d_log_e("Failed to disable HTTP");
  } else {
    http_server.end();
    esp3d_log("HTTP server disabled for FTP");
  }
#endif
#if defined(WS_DATA_FEATURE)
  if (!ESP3DSettings::writeByte(static_cast<int>(ESP3DSettingIndex::esp3d_websocket_on), 0)) {
    esp3d_log_e("Failed to disable WebSocket");
  } else {
    websocket_server.end();
    esp3d_log("WebSocket server disabled for FTP");
  }
#endif
  if (begin()) {
    esp3d_log("FTP server enabled, free heap: %u", ESP.getFreeHeap());
  }
}

void FtpServer::disableFtp() {
  if (!_started) return;
  end();
  if (!ESP3DSettings::writeByte(static_cast<int>(ESP3DSettingIndex::esp3d_ftp_on), 0)) {
    esp3d_log_e("Failed to disable FTP");
  }
#if defined(HTTP_FEATURE)
  if (!ESP3DSettings::writeByte(static_cast<int>(ESP3DSettingIndex::esp3d_http_on), 1)) {
    esp3d_log_e("Failed to re-enable HTTP");
  } else {
    http_server.begin();
    esp3d_log("HTTP server re-enabled");
  }
#endif
#if defined(WS_DATA_FEATURE)
  if (!ESP3DSettings::writeByte(static_cast<int>(ESP3DSettingIndex::esp3d_websocket_on), 1)) {
    esp3d_log_e("Failed to re-enable WebSocket");
  } else {
    websocket_server.begin();
    esp3d_log("WebSocket server re-enabled");
  }
#endif
  esp3d_log("FTP server disabled, free heap: %u", ESP.getFreeHeap());
}

bool FtpServer::processFtpCommand(const char* cmd) {
  if (strcmp_P(cmd, PSTR("[ESP410]ENABLE")) == 0) {
    enableFtp();
    Serial.println("FTP enabled");
    return true;
  } else if (strcmp_P(cmd, PSTR("[ESP410]DISABLE")) == 0) {
    disableFtp();
    Serial.println("FTP disabled");
    return true;
  }
  return false;
}

bool FtpServer::isConnected() {
  return client.connected();
}

const char* FtpServer::clientIPAddress() {
  static char ipStr[16];
  snprintf(ipStr, sizeof(ipStr), "%s", client.remoteIP().toString().c_str());
  return ipStr;
}

bool FtpServer::isUser(const char* user) {
#if defined(AUTHENTICATION_FEATURE)
  return strcmp(user, "anonymous") == 0 || 
         strcmp(user, ESP3DSettings::readString(static_cast<int>(ESP3DSettingIndex::esp3d_admin_pwd)).c_str()) == 0 || 
         strcmp(user, ESP3DSettings::readString(static_cast<int>(ESP3DSettingIndex::esp3d_user_pwd)).c_str()) == 0;
#else
  return strcmp(user, "anonymous") == 0 || strcmp(user, _currentUser.c_str()) == 0;
#endif
}

bool FtpServer::isPassword(const char* password) {
#if defined(AUTHENTICATION_FEATURE)
  bool isAdmin = strcmp(password, ESP3DSettings::readString(static_cast<int>(ESP3DSettingIndex::esp3d_admin_pwd)).c_str()) == 0;
  bool isUser = strcmp(password, ESP3DSettings::readString(static_cast<int>(ESP3DSettingIndex::esp3d_user_pwd)).c_str()) == 0;
  return isAdmin || isUser;
#else
  return true; // Simplified for anonymous or no password
#endif
}

bool FtpServer::accessFS(const char* path) {
  _fsType = 1; // Assuming LittleFS/SPIFFS
#if defined(SD_DEVICE)
  if (ESP_SD::accessFS()) {
    _fsType = 2;
    return true;
  }
#endif
  return ESP_FileSystem::accessFS();
}

void FtpServer::releaseFS() {
#if defined(SD_DEVICE)
  if (_fsType == 2) {
    ESP_SD::releaseFS();
    return;
  }
#endif
  ESP_FileSystem::releaseFS();
}

void FtpServer::iniVariables() {
  cmdStage = FTP_Stop;
  transferStage = FTP_Close;
  dataConn = FTP_NoConn;
  iCL = 0;
  nbMatch = 0;
  millisEndConnection = 0;
  millisBeginTrans = 0;
  bytesTransfered = 0;
  rnfrCmd = false;
  strcpy(cwdName, "/");
  _currentUser = "";
}

void FtpServer::clientConnected() {
  esp3d_log("Client connected: %s, free heap: %u", clientIPAddress(), ESP.getFreeHeap());
  cmdStage = FTP_User;
  millisEndConnection = millis();
  client.println("220 ESP3D FTP Server");
  abortTransfer();
}

void FtpServer::disconnectClient() {
  esp3d_log("Disconnecting client");
  abortTransfer();
  client.println("221 Goodbye");
  client.stop();
  cmdStage = FTP_Stop;
}

bool FtpServer::processCommand() {
  if (!client.available()) return false;
  char c;
  while (client.available() && iCL < FTP_CMD_SIZE - 1) {
    c = readChar();
    if (c == -1) continue;
    if (c == '\r') continue;
    if (c == '\n') {
      cmdLine[iCL] = '\0';
      iCL = 0;
      char* pos = strchr(cmdLine, ' ');
      if (pos) {
        strncpy(command, cmdLine, pos - cmdLine);
        command[pos - cmdLine] = '\0';
        parameter = pos + 1;
      } else {
        strncpy(command, cmdLine, sizeof(command) - 1);
        command[sizeof(command) - 1] = '\0';
        parameter = nullptr;
      }
      if (cmdStage == FTP_User) {
        if (CommandIs("USER")) {
          if (isUser(parameter)) {
            _currentUser = parameter;
            client.println("331 Please specify the password");
            cmdStage = FTP_Pass;
            return true;
          } else {
            client.println("530 User not recognized");
            return false;
          }
        }
      } else if (cmdStage == FTP_Pass) {
        if (CommandIs("PASS")) {
          if (isPassword(parameter)) {
            client.println("230 Login successful");
            cmdStage = FTP_Cmd;
            millisEndConnection = millis();
            return true;
          } else {
            client.println("530 Password incorrect");
            return false;
          }
        }
      } else if (cmdStage == FTP_Cmd) {
        if (CommandIs("RETR")) {
          return doRetrieve();
        } else if (CommandIs("STOR")) {
          return doStore();
        } else if (CommandIs("LIST")) {
          return doList();
        } else if (CommandIs("NLST")) {
          return doList();
        } else if (CommandIs("MLSD")) {
          return doMlsd();
        } else if (CommandIs("CWD")) {
          if (haveParameter()) {
            char path[FTP_CWD_SIZE];
            if (makePath(path)) {
              strcpy(cwdName, path);
              client.println("250 Directory changed");
            } else {
              client.println("550 Failed to change directory");
            }
          } else {
            client.println("501 No directory specified");
          }
        } else if (CommandIs("PWD")) {
          client.print("257 \"");
          client.print(cwdName);
          client.println("\" is current directory");
        } else if (CommandIs("RNFR")) {
          if (haveParameter()) {
            if (makeExistsPath(rnfrName)) {
              rnfrCmd = true;
              client.println("350 Ready for RNTO");
            } else {
              client.println("550 File or directory does not exist");
            }
          } else {
            client.println("501 No file specified");
          }
        } else if (CommandIs("RNTO")) {
          if (rnfrCmd && haveParameter()) {
            char path[FTP_CWD_SIZE];
            if (makePath(path)) {
              if (ESP_FileSystem::rename(rnfrName, path)) {
                client.println("250 Rename successful");
              } else {
                client.println("550 Rename failed");
              }
            } else {
              client.println("550 Invalid path");
            }
            rnfrCmd = false;
          } else {
            client.println("503 RNFR required first");
          }
        } else if (CommandIs("DELE")) {
          if (haveParameter()) {
            char path[FTP_CWD_SIZE];
            if (makeExistsPath(path)) {
              if (ESP_FileSystem::remove(path)) {
                client.println("250 File deleted");
              } else {
                client.println("550 Delete failed");
              }
            } else {
              client.println("550 File does not exist");
            }
          } else {
            client.println("501 No file specified");
          }
        } else if (CommandIs("MKD")) {
          if (haveParameter()) {
            char path[FTP_CWD_SIZE];
            if (makePath(path)) {
              if (ESP_FileSystem::mkdir(path)) {
                client.println("257 Directory created");
              } else {
                client.println("550 Create directory failed");
              }
            } else {
              client.println("550 Invalid path");
            }
          } else {
            client.println("501 No directory specified");
          }
        } else if (CommandIs("RMD")) {
          if (haveParameter()) {
            char path[FTP_CWD_SIZE];
            if (makeExistsPath(path)) {
              if (ESP_FileSystem::rmdir(path)) {
                client.println("250 Directory deleted");
              } else {
                client.println("550 Delete directory failed");
              }
            } else {
              client.println("550 Directory does not exist");
            }
          } else {
            client.println("501 No directory specified");
          }
        } else if (CommandIs("QUIT")) {
          disconnectClient();
        } else if (CommandIs("PORT")) {
          if (haveParameter()) {
            unsigned int p1, p2;
            sscanf(parameter, "%u,%u,%u,%u,%u,%u", &dataIp[0], &dataIp[1], &dataIp[2], &dataIp[3], &p1, &p2);
            dataPort = (p1 << 8) + p2;
            dataConn = FTP_Active;
            client.println("200 PORT command successful");
          } else {
            client.println("501 No port specified");
          }
        } else if (CommandIs("PASV")) {
          dataConn = FTP_Pasive;
          client.print("227 Entering Passive Mode (");
          client.print(WiFi.localIP()[0]);
          client.print(",");
          client.print(WiFi.localIP()[1]);
          client.print(",");
          client.print(WiFi.localIP()[2]);
          client.print(",");
          client.print(WiFi.localIP()[3]);
          client.print(",");
          client.print(passivePort >> 8);
          client.print(",");
          client.print(passivePort & 255);
          client.println(")");
        } else if (CommandIs("TYPE")) {
          if (haveParameter() && (ParameterIs("A") || ParameterIs("I"))) {
            client.println("200 Type set to I");
          } else {
            client.println("504 Only TYPE I is supported");
          }
        } else {
          client.println("502 Command not implemented");
        }
        return true;
      }
      return false;
    }
    cmdLine[iCL++] = c;
  }
  if (iCL >= FTP_CMD_SIZE - 1) {
    cmdLine[iCL] = '\0';
    iCL = 0;
    client.println("500 Command too long");
    return false;
  }
  return true;
}

bool FtpServer::haveParameter() {
  return parameter && strlen(parameter) > 0;
}

int FtpServer::dataConnect(bool out150) {
  if (dataConn == FTP_Pasive && !dataServer) {
    dataServer = new FTP_SERVER(passivePort);
    if (!dataServer) {
      esp3d_log_e("Failed to create data server");
      return 0;
    }
    dataServer->begin();
  }
  if (dataConn == FTP_Pasive) {
    data = dataServer->available();
    if (data.connected()) {
      if (out150) client.println("150 Accepted data connection");
      return 1;
    }
  } else if (dataConn == FTP_Active) {
    data = FTP_CLIENT();
    if (data.connect(dataIp, dataPort)) {
      if (out150) client.println("150 Accepted data connection");
      return 1;
    } else {
      client.println("425 Can't open data connection");
      return 0;
    }
  }
  return 0;
}

bool FtpServer::dataConnected() {
  if (data.connected()) return true;
  if (dataConn == FTP_Pasive && dataServer) {
    data = dataServer->available();
    return data.connected();
  }
  return false;
}

bool FtpServer::doRetrieve() {
  if (!haveParameter()) {
    client.println("501 No file specified");
    return false;
  }
  char path[FTP_CWD_SIZE];
  if (!makeExistsPath(path)) {
    client.println("550 File does not exist");
    return false;
  }
  if (!dataConnect()) {
    return false;
  }
  File file = FILESYSTEM.open(path, "r");
  if (!file) {
    client.println("550 Failed to open file");
    closeTransfer();
    return false;
  }
  millisBeginTrans = millis();
  bytesTransfered = 0;
  transferStage = FTP_Retrieve;
  while (file.available()) {
    int readBytes = file.read(buf, FTP_BUF_SIZE);
    if (readBytes <= 0) break;
    data.write(buf, readBytes);
    bytesTransfered += readBytes;
    if (millis() - millisBeginTrans > FTP_TIME_OUT * 1000) {
      client.println("552 Transfer timeout");
      file.close();
      closeTransfer();
      return false;
    }
  }
  file.close();
  closeTransfer();
  client.println("226 Transfer complete");
  return true;
}

bool FtpServer::doStore() {
  if (!haveParameter()) {
    client.println("501 No file specified");
    return false;
  }
  char path[FTP_CWD_SIZE];
  if (!makePath(path)) {
    client.println("550 Invalid path");
    return false;
  }
  if (!dataConnect()) {
    return false;
  }
  File file = FILESYSTEM.open(path, "w");
  if (!file) {
    client.println("550 Failed to create file");
    closeTransfer();
    return false;
  }
  millisBeginTrans = millis();
  bytesTransfered = 0;
  transferStage = FTP_Store;
  while (data.available()) {
    int readBytes = data.read(buf, FTP_BUF_SIZE);
    if (readBytes <= 0) break;
    file.write(buf, readBytes);
    bytesTransfered += readBytes;
    if (millis() - millisBeginTrans > FTP_TIME_OUT * 1000) {
      client.println("552 Transfer timeout");
      file.close();
      closeTransfer();
      return false;
    }
  }
  file.close();
  closeTransfer();
  client.println("226 Transfer complete");
  return true;
}

bool FtpServer::doList() {
  if (!dataConnect()) {
    return false;
  }
  char path[FTP_CWD_SIZE];
  if (!makePath(path)) {
    client.println("550 Invalid path");
    closeTransfer();
    return false;
  }
  File dir = FILESYSTEM.open(path, "r");
  if (!dir || !dir.isDirectory()) {
    client.println("550 Not a directory");
    closeTransfer();
    return false;
  }
  transferStage = FTP_List;
  while (File file = dir.openNextFile()) {
    String fn = file.name();
    char attr[5] = "rw-";
    if (file.isDirectory()) attr[0] = 'd';
    time_t t = file.getLastWrite();
    char dtStr[15];
    makeDateTimeStr(dtStr, t);
    char szStr[20];
    snprintf(szStr, sizeof(szStr), "%10u", file.size());
    char out[256];
    snprintf(out, sizeof(out), "%s %3d %-8s %-8s %s %s %s\r\n",
             attr, 1, _currentUser.c_str(), _currentUser.c_str(), szStr, dtStr, fn.c_str());
    data.print(out);
  }
  dir.close();
  closeTransfer();
  client.println("226 Directory listing complete");
  return true;
}

bool FtpServer::doMlsd() {
  if (!dataConnect()) {
    return false;
  }
  char path[FTP_CWD_SIZE];
  if (!makePath(path)) {
    client.println("550 Invalid path");
    closeTransfer();
    return false;
  }
  File dir = FILESYSTEM.open(path, "r");
  if (!dir || !dir.isDirectory()) {
    client.println("550 Not a directory");
    closeTransfer();
    return false;
  }
  transferStage = FTP_Mlsd;
  while (File file = dir.openNextFile()) {
    String fn = file.name();
    char type[10] = "file";
    if (file.isDirectory()) strcpy(type, "dir");
    time_t t = file.getLastWrite();
    char dtStr[30];
    makeDateTimeString(dtStr, t);
    char out[256];
    snprintf(out, sizeof(out), "type=%s;size=%u;modify=%s; %s\r\n",
             type, file.size(), dtStr, fn.c_str());
    data.print(out);
  }
  dir.close();
  closeTransfer();
  client.println("226 MLSD complete");
  return true;
}

void FtpServer::closeTransfer() {
  if (data.connected()) {
    data.stop();
  }
  transferStage = FTP_Close;
  dataConn = FTP_NoConn;
}

void FtpServer::abortTransfer() {
  if (data.connected()) {
    data.stop();
  }
  transferStage = FTP_Close;
  dataConn = FTP_NoConn;
  client.println("426 Transfer aborted");
}

bool FtpServer::makePath(char* fullName, char* param) {
  param = param ? param : parameter;
  if (!param || strlen(param) == 0) return false;
  if (param[0] == '/') {
    strncpy(fullName, param, FTP_CWD_SIZE - 1);
    fullName[FTP_CWD_SIZE - 1] = '\0';
  } else {
    strncpy(fullName, cwdName, FTP_CWD_SIZE - 1);
    size_t len = strlen(fullName);
    if (len > 1 && fullName[len - 1] != '/') {
      fullName[len++] = '/';
      fullName[len] = '\0';
    }
    strncat(fullName, param, FTP_CWD_SIZE - len - 1);
  }
  return true;
}

bool FtpServer::makeExistsPath(char* path, char* param) {
  if (!makePath(path, param)) return false;
  if (ESP_FileSystem::exists(path)) return true;
  return false;
}

char* FtpServer::makeDateTimeStr(char* tstr, time_t timefile) {
  struct tm* t = gmtime(&timefile);
  snprintf(tstr, 15, "%04d%02d%02d%02d%02d%02d",
           t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
           t->tm_hour, t->tm_min, t->tm_sec);
  return tstr;
}

char* FtpServer::makeDateTimeString(char* tstr, time_t timefile) {
  struct tm* t = gmtime(&timefile);
  snprintf(tstr, 30, "%04d%02d%02dT%02d%02d%02d",
           t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
           t->tm_hour, t->tm_min, t->tm_sec);
  return tstr;
}

uint8_t FtpServer::getDateTime(char* dt, uint16_t* pyear, uint8_t* pmonth, uint8_t* pday,
                               uint8_t* phour, uint8_t* pminute, uint8_t* psecond) {
  char* p;
  uint16_t year;
  uint8_t month, day, hour, minute, second;
  if (strlen(dt) < 14) return 0;
  year = atoi(dt);
  p = dt + 4;
  month = atoi(p);
  p += 2;
  day = atoi(p);
  p += 2;
  hour = atoi(p);
  p += 2;
  minute = atoi(p);
  p += 2;
  second = atoi(p);
  if (year < 1970 || month > 12 || day > 31 || hour > 23 || minute > 59 || second > 59) return 0;
  *pyear = year;
  *pmonth = month;
  *pday = day;
  *phour = hour;
  *pminute = minute;
  *psecond = second;
  return 1;
}

bool FtpServer::getFileModTime(const char* path, time_t& time) {
  File file = FILESYSTEM.open(path, "r");
  if (!file) return false;
  time = file.getLastWrite();
  file.close();
  return true;
}

bool FtpServer::timeStamp(const char* path, uint16_t year, uint8_t month, uint8_t day,
                          uint8_t hour, uint8_t minute, uint8_t second) {
  // Not implemented for simplicity
  return false;
}

int8_t FtpServer::readChar() {
  if (!client.available()) return -1;
  return client.read();
}

#endif  // FTP_FEATURE