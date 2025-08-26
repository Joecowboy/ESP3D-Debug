/*
  esp_filesystem.cpp - ESP3D filesystem configuration class

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
#ifdef FILESYSTEM_FEATURE
#include <FS.h>
#include <time.h>
#include "../ftp/FtpServer.h" // Include for FF_MAX_LFN

#if ESP_FILESYSTEM_TYPE == ESP_FILESYSTEM_LITTLEFS
#include <LittleFS.h>
#define FILESYSTEM LittleFS
#elif ESP_FILESYSTEM_TYPE == ESP_SPIFFS_FILESYSTEM
#include <SPIFFS.h>
#define FILESYSTEM SPIFFS
#else
#error "No valid filesystem defined (ESP_FILESYSTEM_LITTLEFS or ESP_SPIFFS_FILESYSTEM)"
#endif

#include "esp_filesystem.h"

#ifdef ARDUINO_ARCH_ESP32
#include <esp_ota_ops.h>
#endif  // ARDUINO_ARCH_ESP32

#define ESP_MAX_OPENHANDLE 4
File tFile_handle[ESP_MAX_OPENHANDLE];

bool ESP_FileSystem::_started = false;

bool ESP_FileSystem::begin() {
  if (_started) return true;
  _started = FILESYSTEM.begin();
  return _started;
}

void ESP_FileSystem::end() {
  if (_started) {
    FILESYSTEM.end();
    _started = false;
  }
}

size_t ESP_FileSystem::totalBytes() {
  FSInfo fs_info;
  if (!_started || !FILESYSTEM.info(fs_info)) return 0;
  return fs_info.totalBytes;
}

size_t ESP_FileSystem::usedBytes() {
  FSInfo fs_info;
  if (!_started || !FILESYSTEM.info(fs_info)) return 0;
  return fs_info.usedBytes;
}

size_t ESP_FileSystem::freeBytes() {
  FSInfo fs_info;
  if (!_started || !FILESYSTEM.info(fs_info)) return 0;
  return fs_info.totalBytes - fs_info.usedBytes;
}

uint ESP_FileSystem::maxPathLength() {
  return FF_MAX_LFN; // Defined in FtpServer.h
}

size_t ESP_FileSystem::max_update_size() {
  size_t flashsize = 0;
#if defined(ARDUINO_ARCH_ESP8266)
  flashsize = ESP.getFlashChipSize();
  // if higher than 1MB or not (no more support for 512KB flash)
  if (flashsize <= 1024 * 1024) {
    flashsize = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
  } else {
    flashsize = flashsize - ESP.getSketchSize() - totalBytes() - 1024;
    // max OTA partition is 1019Kb
    if (flashsize > 1024 * 1024) {
      flashsize = (1024 * 1024) - 1024;
    }
  }
#endif  // ARDUINO_ARCH_ESP8266
#if defined(ARDUINO_ARCH_ESP32)
  // Is OTA available ?
  const esp_partition_t* mainpartition = esp_ota_get_running_partition();
  if (mainpartition) {
    const esp_partition_t* partition =
        esp_ota_get_next_update_partition(mainpartition);
    if (partition) {
      const esp_partition_t* partition2 =
          esp_ota_get_next_update_partition(partition);
      if (partition2 && (partition->address != partition2->address)) {
        flashsize = partition2->size;
      }
    }
  }
#endif  // ARDUINO_ARCH_ESP32
  return flashsize;
}

const char* ESP_FileSystem::FilesystemName() {
#if ESP_FILESYSTEM_TYPE == ESP_FILESYSTEM_LITTLEFS
  return "LittleFS";
#elif ESP_FILESYSTEM_TYPE == ESP_SPIFFS_FILESYSTEM
  return "SPIFFS";
#else
  return "Unknown";
#endif
}

bool ESP_FileSystem::format() {
  return FILESYSTEM.format();
}

ESP_File ESP_FileSystem::open(const char* path, uint8_t mode) {
  File file;
  if (!_started) return ESP_File();
  if (mode == ESP_FILE_READ) {
    file = FILESYSTEM.open(path, "r");
  } else if (mode == ESP_FILE_WRITE) {
    file = FILESYSTEM.open(path, "w");
  } else if (mode == ESP_FILE_APPEND) {
    file = FILESYSTEM.open(path, "a");
  }
  if (!file) return ESP_File();
  for (int i = 0; i < ESP_MAX_OPENHANDLE; i++) {
    if (!tFile_handle[i]) {
      tFile_handle[i] = file;
      return ESP_File(&tFile_handle[i], file.isDirectory(), mode != ESP_FILE_READ, path);
    }
  }
  file.close();
  return ESP_File();
}

bool ESP_FileSystem::exists(const char* path) {
  if (!_started) return false;
  return FILESYSTEM.exists(path);
}

bool ESP_FileSystem::remove(const char* path) {
  if (!_started) return false;
  return FILESYSTEM.remove(path);
}

bool ESP_FileSystem::mkdir(const char* path) {
  if (!_started) return false;
  return FILESYSTEM.mkdir(path);
}

bool ESP_FileSystem::rmdir(const char* path) {
  if (!_started) return false;
  return FILESYSTEM.rmdir(path);
}

bool ESP_FileSystem::rename(const char* oldpath, const char* newpath) {
  if (!_started) return false;
  return FILESYSTEM.rename(oldpath, newpath);
}

void ESP_FileSystem::closeAll() {
  for (int i = 0; i < ESP_MAX_OPENHANDLE; i++) {
    if (tFile_handle[i]) {
      tFile_handle[i].close();
      tFile_handle[i] = File();
    }
  }
}

uint8_t ESP_FileSystem::getFSType(const char* path) {
  (void)path;
  return FS_FLASH;
}

bool ESP_FileSystem::accessFS(uint8_t FS) {
  (void)FS;
  if (!_started) {
    _started = begin();
  }
  return _started;
}

void ESP_FileSystem::releaseFS(uint8_t FS) {
  // nothing to do
  (void)FS;
}

ESP_File::ESP_File(void* handle, bool isdir, bool iswritemode, const char* path) {
  _isdir = isdir;
  _isfakedir = isdir;
  _iswritemode = iswritemode;
  _filename = path ? path : "";
  _name = _filename.substring(_filename.lastIndexOf("/") + 1);
  _size = 0;
  _lastwrite = 0;
  _index = -1;
  if (handle) {
    for (int i = 0; i < ESP_MAX_OPENHANDLE; i++) {
      if (!tFile_handle[i]) {
        tFile_handle[i] = *(File*)handle;
        _index = i;
        _size = tFile_handle[i].size();
        break;
      }
    }
  }
}

ESP_File::ESP_File(const char* name, const char* filename, bool isdir, size_t size) {
  _isdir = isdir;
  _dirlist = "";
  _isfakedir = isdir;
  _index = -1;
  _filename = filename;
  _name = name;
  _lastwrite = 0;
  _iswritemode = false;
  _size = size;
}

ESP_File::~ESP_File() {
  close();
}

ESP_File::operator bool() const {
  if ((_index != -1) || (_filename.length() > 0)) {
    return true;
  }
  return false;
}

bool ESP_File::isOpen() {
  return !(_index == -1);
}

const char* ESP_File::name() const {
  return _name.c_str();
}

const char* ESP_File::filename() const {
  return _filename.c_str();
}

bool ESP_File::isDirectory() {
  return _isdir;
}

size_t ESP_File::size() {
  return _size;
}

time_t ESP_File::getLastWrite() {
  return _lastwrite;
}

int ESP_File::available() {
  if (_index == -1 || _isdir) {
    return 0;
  }
  return tFile_handle[_index].available();
}

size_t ESP_File::write(uint8_t i) {
  if ((_index == -1) || _isdir) {
    return 0;
  }
  return tFile_handle[_index].write(i);
}

size_t ESP_File::write(const uint8_t* buf, size_t size) {
  if ((_index == -1) || _isdir) {
    return 0;
  }
  return tFile_handle[_index].write(buf, size);
}

int ESP_File::read() {
  if ((_index == -1) || _isdir) {
    return -1;
  }
  return tFile_handle[_index].read();
}

size_t ESP_File::read(uint8_t* buf, size_t size) {
  if ((_index == -1) || _isdir) {
    return -1;
  }
  return tFile_handle[_index].read(buf, size);
}

void ESP_File::flush() {
  if ((_index == -1) || _isdir) {
    return;
  }
  tFile_handle[_index].flush();
}

ESP_File ESP_File::openNextFile() {
  if (!_isdir || _index == -1) {
    return ESP_File();
  }
  Dir dir = FILESYSTEM.openDir(_filename.c_str());
  while (dir.next()) {
    String name = dir.fileName();
    String fullPath = _filename + "/" + name;
    return ESP_File(name.c_str(), fullPath.c_str(), dir.isDirectory(), dir.fileSize());
  }
  return ESP_File();
}

bool ESP_File::seek(uint32_t pos, uint8_t mode) {
  if (_index == -1 || _isdir) {
    return false;
  }
  SeekMode seekMode;
  switch (mode) {
    case ESP_SEEK_SET:
      seekMode = SeekSet;
      break;
    case ESP_SEEK_CUR:
      seekMode = SeekCur;
      break;
    case ESP_SEEK_END:
      seekMode = SeekEnd;
      break;
    default:
      return false;
  }
  return tFile_handle[_index].seek(pos, seekMode);
}

void ESP_File::close() {
  if (_index != -1) {
    tFile_handle[_index].close();
    tFile_handle[_index] = File();
    _index = -1;
  }
}

ESP_File& ESP_File::operator=(const ESP_File& other) {
  close();
  _isdir = other._isdir;
  _isfakedir = other._isfakedir;
  _index = other._index;
  _filename = other._filename;
  _name = other._name;
  _size = other._size;
  _iswritemode = other._iswritemode;
  _dirlist = other._dirlist;
  _lastwrite = other._lastwrite;
  if (_index != -1) {
    tFile_handle[_index] = tFile_handle[other._index];
  }
  return *this;
}

#endif  // FILESYSTEM_FEATURE