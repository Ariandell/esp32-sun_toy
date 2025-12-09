#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "Config.h"
#include "modules/ButtonManager.h"
#include "modules/NfcManager.h"
#include "modules/AudioManager.h"
#include "modules/WebPortal.h"
#include "modules/LedController.h"

// --- ОБ'ЄКТИ ---
NfcManager nfcManager(PIN_NFC_SDA, PIN_NFC_SCL);
Button btnVol(PIN_BTN_VOL);
Button btnCtrl(PIN_BTN_CTRL);
AudioManager audioManager;
WebPortal webPortal;
LedController led;

bool isWifiMode = false;
int currentVol = 8;

void setMode(bool wifiMode) {
    if (isWifiMode == wifiMode) return;
    isWifiMode = wifiMode;
    Serial.print(">>> MODE CHANGED: "); Serial.println(isWifiMode ? "WIFI" : "PLAYER");

    if (isWifiMode) {
        led.setColor(0, 0, 255);
        webPortal.begin();
    } else {
        led.setColor(0, 255, 0);
        WiFi.softAPdisconnect(true); 
        WiFi.mode(WIFI_OFF);
    }
}

void executeCommand(String tag) {
    Serial.print("Executing: "); Serial.println(tag);

    bool played = false;
    
    if (tag.indexOf("cmd:rec") != -1 || tag == "autoplay_upload") {
        audioManager.playFile(FILE_CUSTOM_STORY);
        played = true;
    }
    else if (tag.indexOf("cmd:01") != -1) {
        audioManager.playFolder(DIR_TALE_1);
        played = true;
    }
    else if (tag.indexOf("cmd:02") != -1) {
        audioManager.playFolder(DIR_TALE_2);
        played = true;
    }
    else if (tag.indexOf("cmd:03") != -1) {
        audioManager.playFolder(DIR_TALE_3);
        played = true;
    }

    if (played) {
        led.setColor(255, 255, 0); delay(200);
        if (isWifiMode) led.setColor(0, 0, 255);
        else led.setColor(0, 255, 0);
    }
}

void onTagDetected(String uid, String content) {
    Serial.print("TAG: "); Serial.println(content);
    if (content.indexOf("cmd:") == -1 || content.indexOf("192.168") != -1) {
        if (!isWifiMode) setMode(true);
        else setMode(false);
    }
    else if (content.indexOf("cmd:") != -1) {
        executeCommand(content);
    }
}

void setup() {
    Serial.begin(115200);
    led.begin(); led.setColor(50, 20, 0);

    btnVol.begin(); btnCtrl.begin();

    btnVol.attachClick([]() {
        currentVol += 2;
        if (currentVol > 21) currentVol = 4;
        Serial.print("BTN Vol: "); Serial.println(currentVol);
        audioManager.setVolume(currentVol);
        led.setColor(255, 255, 0); delay(100);
    });

    btnCtrl.attachLongPress([]() { setMode(!isWifiMode); });
    btnCtrl.attachClick([]() { 
        Serial.println("BTN Next"); 
        audioManager.playNext(); 
    });

    SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
    delay(100);
    SD.begin(PIN_SD_CS);

    nfcManager.onTagDetected(onTagDetected);
    nfcManager.begin();

    audioManager.begin();
    audioManager.setVolume(currentVol);

    webPortal.onVolumeChange([](int vol) {
        Serial.print("WEB Vol: "); Serial.println(vol);
        currentVol = vol;
        audioManager.setVolume(currentVol);
    });

    webPortal.onLedChange([](int r, int g, int b) { led.setColor(r, g, b); });

    webPortal.onUploadComplete([]() {
        Serial.println("WEB Upload Done -> Playing");
        executeCommand("autoplay_upload");
    });

    setMode(false); 
}

void loop() {
    btnVol.loop();
    btnCtrl.loop();
    led.loop();

    if (isWifiMode) {
        webPortal.loop();
        if (webPortal.isClientConnected()) led.setLoading(true);
        else led.setColor(0, 0, 255);
        audioManager.loop(); 
    } 
    else {
        audioManager.loop();
        led.setLoading(false);
        if (audioManager.isPlaying()) led.setColor(0, 255, 0);
        else led.setColor(0, 5, 0);
    }
}