#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_NeoPixel.h>
#include <Audio.h>
#include <Wire.h> 
#include <Adafruit_PN532.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "Config.h"
#include "modules/WebPortal.h"
#include "modules/ButtonManager.h"
#include "modules/AudioManager.h"
#include "modules/LedController.h"

WebPortal webPortal;
LedController led;
AudioManager audioManager;
Adafruit_PN532 nfc(PIN_NFC_SDA, PIN_NFC_SCL);
Button btnVol(PIN_BTN_VOL);    
Button btnCtrl(PIN_BTN_CTRL); 

bool isWifiMode = false;
int currentVol = 8; 
unsigned long bothPressedTime = 0;
bool wifiTriggered = false;
bool hasPlaylist = false; 

volatile bool newTagDetected = false;
String pendingTagData = ""; 
TaskHandle_t nfcTaskHandle = NULL;

void toggleWifi(bool enable) {
    if (enable && !isWifiMode) {
        audioManager.playFile(SND_WIFI_ON);
        unsigned long startWait = millis();
        while(millis() - startWait < 2500) { 
            audioManager.loop(); 
            led.loop(); 
        }
        webPortal.begin();
        isWifiMode = true;
        led.setColor(0, 0, 255);
    } 
    else if (!enable && isWifiMode) {
        audioManager.stopSong(); 
        delay(50);
        
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_OFF);
        isWifiMode = false;
        delay(100);
        
        audioManager.playFile(SND_WIFI_OFF);
        led.setColor(0, 255, 0);
    }
}

void nfcTask(void * parameter) {
    while(true) {
        if (newTagDetected) {
            vTaskDelay(100 / portTICK_PERIOD_MS);
            continue;
        }

        uint8_t uid[7]; uint8_t uidLength;
        if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 50)) {
            uint8_t dataBuffer[32]; String result = "";
            for (uint8_t page = 4; page < 10; page++) {
                if (nfc.ntag2xx_ReadPage(page, dataBuffer)) {
                    for (int i = 0; i < 4; i++) {
                        char c = (char)dataBuffer[i];
                        if (isalnum(c) || c == ':' || c == '/' || c == '.' || c == '-' || c == '_') 
                            result += c;
                    }
                }
            }

            if (result.length() > 0) {
                pendingTagData = result;
                newTagDetected = true;
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

void setup() {
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    
    led.begin(); 
    led.setColor(50, 20, 0);
    
    btnVol.begin(); 
    btnCtrl.begin();

    btnVol.attachClick([](){
        currentVol += 2;
        if (currentVol > 21) currentVol = 4;
        audioManager.setVolume(currentVol);
        led.setColor(255, 255, 0); delay(100);
    });

    btnCtrl.attachClick([](){
        if (hasPlaylist) {
            audioManager.playNext();
            led.setColor(0, 255, 255); delay(100);
        }
    });

    SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
    if (!SD.begin(PIN_SD_CS)) {
        led.setColor(255, 0, 0);
    }

    Wire.begin(PIN_NFC_SDA, PIN_NFC_SCL);
    nfc.begin();
    if (nfc.getFirmwareVersion()) {
        nfc.SAMConfig();
        xTaskCreatePinnedToCore(
            nfcTask, "NFC_Task", 4096, NULL, 1, &nfcTaskHandle, 0
        );
    }

    audioManager.begin();
    audioManager.setVolume(currentVol);
    
    led.setColor(0, 255, 0); 
    audioManager.playFile(SND_BOOT);
}

void loop() {
    audioManager.loop();
    led.loop();
    btnVol.loop();
    btnCtrl.loop();

    if (digitalRead(PIN_BTN_VOL) == HIGH && digitalRead(PIN_BTN_CTRL) == HIGH) {
        if (bothPressedTime == 0) {
            bothPressedTime = millis();
        }
        
        if (millis() - bothPressedTime > 3000 && !wifiTriggered) {
            wifiTriggered = true;
            led.setColor(255, 255, 255); delay(200);
            toggleWifi(!isWifiMode);
        }
    } else {
        bothPressedTime = 0; 
        wifiTriggered = false;
    }

    if (isWifiMode) {
        webPortal.loop();
        if (webPortal.isClientConnected()) led.setLoading(true);
        else led.setColor(0, 0, 255);
    } else {
        led.setLoading(false);
        if (audioManager.isPlaying()) led.setColor(0, 255, 0);
        else led.setColor(0, 5, 0);
    }

    if (newTagDetected) {
        String tag = pendingTagData;
        bool commandExecuted = false;

        if (tag.indexOf("192.168.4.1") != -1 || tag.indexOf("cmd:magic") != -1) {
            toggleWifi(!isWifiMode);
            commandExecuted = true;
        }
        else if (tag.indexOf("cmd:rec") != -1) {
            if (isWifiMode) toggleWifi(false);
            audioManager.playFile(FILE_CUSTOM_STORY);
            hasPlaylist = true;
            commandExecuted = true;
        }
        else if (tag.indexOf("cmd:01") != -1) {
            if (isWifiMode) toggleWifi(false);
            audioManager.playFolder("/tales/01");
            hasPlaylist = true;
            commandExecuted = true;
        }
        else if (tag.indexOf("cmd:02") != -1) {
            if (isWifiMode) toggleWifi(false);
            audioManager.playFolder("/tales/02");
            hasPlaylist = true;
            commandExecuted = true;
        }
        else if (tag.indexOf("cmd:03") != -1) {
            if (isWifiMode) toggleWifi(false);
            audioManager.playFolder("/tales/03");
            hasPlaylist = true;
            commandExecuted = true;
        }

        newTagDetected = false; 
        
        if (commandExecuted) {
             led.setColor(255, 255, 0);
             delay(200);
        }
    }
}