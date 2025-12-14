#pragma once

// FIRMWARE VERSION
#define FIRMWARE_VERSION "1.0.0"

// WIFI
#define WIFI_SSID "SunToy"
#define WIFI_PASS "" 

// SD 
#define PIN_SD_CS   5
#define PIN_SD_SCK  18
#define PIN_SD_MISO 19
#define PIN_SD_MOSI 23

// AUDIO 
#define PIN_I2S_BCLK 26
#define PIN_I2S_LRC  25 
#define PIN_I2S_DOUT 22

// NFC
#define PIN_NFC_SDA 21
#define PIN_NFC_SCL 14

// PERIPHERALS
#define PIN_LED 2
#define PIN_BTN_VOL 4
#define PIN_BTN_CTRL 13

#define DIR_TALE_1    "/tales/01"
#define DIR_TALE_2    "/tales/02"
#define DIR_TALE_3    "/tales/03"
#define FILE_CUSTOM_STORY      "/records/custom_story.wav"

#define TAG_CMD_CUSTOM "cmd:rec" 