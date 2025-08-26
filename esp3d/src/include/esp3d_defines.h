/*
  esp3d_defines.h - ESP3D defines file

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

#ifndef _DEFINES_ESP3D_H
#define _DEFINES_ESP3D_H

// Filesystem type selection
#define ESP_FILESYSTEM_LITTLEFS 3
#define ESP_FILESYSTEM_SPIFFS 1
#define ESP_FILESYSTEM_TYPE ESP_FILESYSTEM_LITTLEFS // Default to LittleFS

// Settings
#define SETTINGS_IN_EEPROM 1
#define SETTINGS_IN_PREFERENCES 2

// Supported FW
#define UNKNOWN_FW 0
#define GRBL 10
#define MARLIN 20
#define MARLIN_EMBEDDED 30
#define SMOOTHIEWARE 40
#define REPETIER 50
#define KLIPPER 60
#define REPRAP 70
#define GRBLHAL 80
#define HP_GL 90

// Setting indices as enum
enum class ESP3DSettingIndex {
  esp3d_plugin_direct_pin_enabled = 0,
  esp3d_radio_mode = 1,
  esp3d_sta_ssid = 2,
  esp3d_sta_password = 34,
  esp3d_sta_ip_mode = 99,
  esp3d_sta_ip_value = 100,
  esp3d_sta_mask_value = 104,
  esp3d_sta_gateway_value = 108,
  esp3d_baud_rate = 112,
  esp3d_notification_type = 116,
  esp3d_calibration = 117,
  esp3d_ap_channel = 118,
  esp3d_buzzer = 119,
  esp3d_internet_time = 120,
  esp3d_http_port = 121,
  esp3d_telnet_port = 125,
  esp3d_output_client = 129,
  esp3d_hostname = 130,
  esp3d_sensor_interval = 164,
  esp3d_settings_version = 168,
  esp3d_admin_pwd = 176,
  esp3d_user_pwd = 197,
  esp3d_ap_ssid = 218,
  esp3d_ap_password = 251,
  esp3d_ap_ip_value = 316,
  esp3d_boot_delay = 320,
  esp3d_websocket_port = 324,
  esp3d_http_on = 328,
  esp3d_ftp_on = 329,
  esp3d_websocket_on = 330,
  esp3d_sd_speed_div = 331,
  esp3d_notification_token1 = 332,
  esp3d_notification_token2 = 583,
  esp3d_sensor_type = 647,
  esp3d_target_fw = 648,
  esp3d_free = 649,
  esp3d_time_server1 = 651,
  esp3d_time_server2 = 780,
  esp3d_time_server3 = 909,
  esp3d_sd_mount = 1039,
  esp3d_session_timeout = 1040,
  esp3d_sd_check_update_at_boot = 1042,
  esp3d_notification_settings = 1043,
  esp3d_calibration_1 = 1172,
  esp3d_calibration_2 = 1176,
  esp3d_calibration_3 = 1180,
  esp3d_calibration_4 = 1184,
  esp3d_calibration_5 = 1188,
  esp3d_setup = 1192,
  esp3d_eth_sta_fallback_mode = 1195,
  esp3d_ftp_ctrl_port = 1196,
  esp3d_ftp_data_active_port = 1200,
  esp3d_ftp_data_passive_port = 1204,
  esp3d_auto_notification = 1208,
  esp3d_verbose_boot = 1209,
  esp3d_webdav_on = 1210,
  esp3d_webdav_port = 1211,
  esp3d_sta_dns_value = 1212,
  esp3d_secure_serial = 1213,
  esp3d_boot_radio_state = 1214,
  esp3d_sta_fallback_mode = 1215,
  esp3d_serial_bridge_on = 1216,
  esp3d_eth_sta_ip_mode = 1217,
  esp3d_serial_bridge_baud = 1218,
  esp3d_time_zone = 1219,
  esp3d_eth_sta_ip_value = 1220,
  esp3d_eth_sta_mask_value = 1221,
  esp3d_eth_sta_gateway_value = 1222,
  esp3d_eth_sta_dns_value = 1223,
  esp3d_usb_serial_baud_rate = 1224
};

// Hidden password
#define HIDDEN_PASSWORD "********"

// Debug
#define LOG_OUTPUT_SERIAL0 1
#define LOG_OUTPUT_SERIAL1 2
#define LOG_OUTPUT_SERIAL2 3
#define LOG_OUTPUT_TELNET 4
#define LOG_OUTPUT_WEBSOCKET 5

#define LOG_LEVEL_NONE 0
#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_DEBUG 2
#define LOG_LEVEL_VERBOSE 3

// Serial
#define USE_SERIAL_0 1
#define USE_SERIAL_1 2
#define USE_SERIAL_2 3
#define USE_USB_SERIAL -1

// Serial service ID
#define MAIN_SERIAL 1
#define BRIDGE_SERIAL 2

// Communication protocols
#define RAW_SERIAL 0
#define MKS_SERIAL 1
#define SOCKET_SERIAL 2

// Display
#define OLED_I2C_SSD1306_128X64 1
#define OLED_I2C_SSDSH1106_132X64 2
#define TFT_SPI_ILI9341_320X240 3
#define TFT_SPI_ILI9488_480X320 4
#define TFT_SPI_ST7789_240X240 5
#define TFT_SPI_ST7789_135X240 6

// UI type for display
#define UI_TYPE_BASIC 1
#define UI_TYPE_ADVANCED 2
#define UI_COLORED 1
#define UI_MONOCHROME 2

// SD connection
#define ESP_NO_SD 0
#define ESP_NOT_SHARED_SD 1
#define ESP_SHARED_SD 2

// SD Device type
#define ESP_NORMAL_SDCARD 0
#define ESP_FYSETC_WIFI_PRO_SDCARD 1

// Upload type
#define ESP_UPLOAD_DIRECT_SD 1
#define ESP_UPLOAD_SHARED_SD 2
#define ESP_UPLOAD_SERIAL_SD 3
#define ESP_UPLOAD_FAST_SERIAL_SD 4
#define ESP_UPLOAD_FAST_SERIAL_USB 5
#define ESP_UPLOAD_DIRECT_USB 6

// IP mode
#define DHCP_MODE 1
#define STATIC_IP_MODE 0

// SD mount point
#define ESP_SD_ROOT 1
#define ESP_SD_SUB_SD 2
#define ESP_SD_SUB_EXT 3

// Touch
#define XPT2046_SPI 1

// Input
#define ROTARY_ENCODER 1

// File systems
#define ESP_SPIFFS_FILESYSTEM 1
#define ESP_FAT_FILESYSTEM 2
#define ESP_LITTLEFS_FILESYSTEM 3

// SD READER FS type supported
#define ESP_SD_NATIVE 1
#define ESP_SDIO 2
#define ESP_SDFAT2 3

// SDIO Mode
#define SD_ONE_BIT_MODE 1
#define SD_FOUR_BIT_MODE 0

// SD state
#define ESP_SDCARD_IDLE 0
#define ESP_SDCARD_NOT_PRESENT 1
#define ESP_SDCARD_BUSY 2

// Notifications
#define ESP_NO_NOTIFICATION 0
#define ESP_PUSHOVER_NOTIFICATION 1
#define ESP_EMAIL_NOTIFICATION 2
#define ESP_LINE_NOTIFICATION 3
#define ESP_TELEGRAM_NOTIFICATION 4
#define ESP_IFTTT_NOTIFICATION 5
#define ESP_HOMEASSISTANT_NOTIFICATION 6
#define ESP_WHATS_APP_NOTIFICATION 7

// SENSOR
#define NO_SENSOR_DEVICE 0
#define DHT11_DEVICE 1
#define DHT22_DEVICE 2
#define ANALOG_DEVICE 3
#define BMP280_DEVICE 4
#define BME280_DEVICE 5

#define USE_CELSIUS 1
#define USE_FAHRENHEIT 2

// Camera
#define CAMERA_MODEL_CUSTOM 0
#define CAMERA_MODEL_ESP_EYE 1
#define CAMERA_MODEL_M5STACK_PSRAM 2
#define CAMERA_MODEL_M5STACK_V2_PSRAM 3
#define CAMERA_MODEL_M5STACK_WIDE 4
#define CAMERA_MODEL_AI_THINKER 7
#define CAMERA_MODEL_WROVER_KIT 8
#define CAMERA_MODEL_ESP32_CAM_BOARD 10
#define CAMERA_MODEL_ESP32S2_CAM_BOARD 11
#define CAMERA_MODEL_ESP32S3_CAM_LCD 12
#define CAMERA_MODEL_ESP32S3_EYE 13
#define CAMERA_MODEL_XIAO_ESP32S3 14
#define CAMERA_MODEL_UICPAL_ESP32S3 15

// Errors code
#define ESP_ERROR_AUTHENTICATION 1
#define ESP_ERROR_FILE_CREATION 2
#define ESP_ERROR_FILE_WRITE 3
#define ESP_ERROR_UPLOAD 4
#define ESP_ERROR_NOT_ENOUGH_SPACE 5
#define ESP_ERROR_UPLOAD_CANCELLED 6
#define ESP_ERROR_FILE_CLOSE 7
#define ESP_ERROR_NO_SD 8
#define ESP_ERROR_MOUNT_SD 9
#define ESP_ERROR_RESET_NUMBERING 10
#define ESP_ERROR_BUFFER_OVERFLOW 11
#define ESP_ERROR_START_UPLOAD 12
#define ESP_ERROR_SIZE 13
#define ESP_ERROR_UPDATE 14

// File system
#define ESP_FILE_READ 0
#define ESP_FILE_WRITE 1
#define ESP_FILE_APPEND 2

#define ESP_SEEK_SET 0
#define ESP_SEEK_CUR 1
#define ESP_SEEK_END 2

#define FS_ROOT 0
#define FS_FLASH 1
#define FS_SD 2
#define FS_USBDISK 3
#define FS_UNKNOWN 254
#define MAX_FS 3

// ethernet clock modes
#define MODE_ETH_CLOCK_GPIO0_IN 0
#define MODE_ETH_CLOCK_GPIO0_OUT 1
#define MODE_ETH_CLOCK_GPIO16_OUT 2
#define MODE_ETH_CLOCK_GPIO17_OUT 3

// Ethernet type
#define TYPE_ETH_PHY_LAN8720 0
#define TYPE_ETH_PHY_TLK110 1
#define TYPE_ETH_PHY_RTL8201 2
#define TYPE_ETH_PHY_DP83848 3
#define TYPE_ETH_PHY_KSZ8041 4
#define TYPE_ETH_PHY_KSZ8081 5
#define TYPE_ETH_PHY_DM9051 6
#define TYPE_ETH_PHY_W5500 7
#define TYPE_ETH_PHY_KSZ8851 8

// Host path
#define ESP3D_HOST_PATH "/"

// FTP buffer size reduced to prevent OOM
#define FTP_BUF_SIZE 512  // Reduced from 1024 to save heap

// Minimum heap required for FTP operations
#define MIN_HEAP_FOR_FTP 20000

#endif  //_DEFINES_ESP3D_H