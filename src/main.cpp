#include <Arduino.h>
#include <WiFi.h>
#include <freertos/semphr.h> 
#include "Config.h"
#include "modules/AudioManager.h"
#include "modules/NfcManager.h"
#include "modules/ThemeManager.h"
#include "modules/WebPortal.h"
#include "modules/ButtonManager.h"
#include <Preferences.h>

#define DEBUG_MAIN 1
#if DEBUG_MAIN
    #define LOG_MAIN(msg) Serial.println("[MAIN] " msg)
    #define LOG_MAIN_F(fmt, ...) Serial.printf("[MAIN] " fmt "\n", ##__VA_ARGS__)
#else
    #define LOG_MAIN(msg)
    #define LOG_MAIN_F(fmt, ...)
#endif

AudioManager audioManager;
NfcManager nfcManager(PIN_NFC_SDA, PIN_NFC_SCL);
ThemeManager theme;
WebPortal webPortal;
LedController led; 

Button btnVol(PIN_BTN_VOL);
Button btnCtrl(PIN_BTN_CTRL);
SemaphoreHandle_t nfcMutex = NULL;

enum AppState {
    STATE_IDLE,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_WIFI_MODE,       
    STATE_BT_MODE,
    STATE_WAITING_FOR_PLAY,
    STATE_WIFI_TRANSITION 
};

AppState currentState = STATE_IDLE;
String lastScannedTag = "";
unsigned long stateEnterTime = 0;
unsigned long lastNfcTime = 0; 
bool isCustomFigurineActive = false;
volatile bool nfcEventPending = false;
volatile bool pendingCustomPlayback = false;
String pendingUid = "";
String pendingContent = "";
bool isComboMode = false; 
bool ignoreButtonsUntilRelease = false; 
void changeState(AppState newState);
const char* stateToString(AppState state);
void processNfcTag();
void handleCustomFigurine();

const char* stateToString(AppState state) {
    switch(state) {
        case STATE_IDLE: return "IDLE";
        case STATE_PLAYING: return "PLAYING";
        case STATE_PAUSED: return "PAUSED";
        case STATE_WIFI_MODE: return "WIFI_MODE";
        case STATE_BT_MODE: return "BT_MODE";
        case STATE_WAITING_FOR_PLAY: return "WAITING_FOR_PLAY";
        case STATE_WIFI_TRANSITION: return "WIFI_TRANSITION";
        default: return "UNKNOWN";
    }
}

void changeState(AppState newState) {
    LOG_MAIN_F("changeState: %s -> %s", stateToString(currentState), stateToString(newState));
    
    if (currentState == newState) return;
    if (currentState == STATE_BT_MODE) {
        audioManager.stopBluetooth();
        lastScannedTag = "";
    }

    if (currentState == STATE_WIFI_MODE) {
        webPortal.stop();
        delay(100);
        
        // Play WiFi off sound before disconnecting
        if (SD.exists("/system/wifi_off.wav")) {
            audioManager.playFile("/system/wifi_off.wav");
            unsigned long wifiSoundStart = millis();
            while (audioManager.isPlaying() && millis() - wifiSoundStart < 3000) {
                audioManager.loop();
                delay(10);
            }
        }
        
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        delay(100);
        lastScannedTag = "";
        isCustomFigurineActive = false;
        pendingCustomPlayback = false;
    }
    
    currentState = newState;
    stateEnterTime = millis();

    switch (currentState) {
        case STATE_IDLE:
            theme.setLed("idle", led);
            break;
        case STATE_PLAYING:
            theme.setLed("playing", led);
            break;
        case STATE_PAUSED:
            theme.setLed("paused", led);
            break;
        case STATE_BT_MODE:
            theme.setLed("bt_mode", led);
            // Play BT on sound before starting bluetooth
            if (SD.exists("/system/bt_on.wav")) {
                audioManager.playFile("/system/bt_on.wav");
                // Wait for sound to finish before BT takes over I2S
                unsigned long btSoundStart = millis();
                while (audioManager.isPlaying() && millis() - btSoundStart < 3000) {
                    audioManager.loop();
                    delay(10);
                }
            }
            audioManager.startBluetooth();
            break;
            
        case STATE_WIFI_TRANSITION:
            led.setColor(255, 200, 0); 
            break;

        case STATE_WIFI_MODE:
            audioManager.stop();  // Stop any playing audio first
            delay(100);
            theme.setLed("wifi_start", led); 
            webPortal.begin(); 
            audioManager.playFile("/system/connect.mp3"); 
            break;
            
        case STATE_WAITING_FOR_PLAY:
            theme.setLed("idle", led);
            theme.apply("play_prompt", audioManager, led);
            break;
    }
}

void onAudioStateChanged(bool isPlaying) {
    LOG_MAIN_F("Audio State Callback: isPlaying=%d", isPlaying);
    
    if (currentState == STATE_BT_MODE) return; 
    if (currentState == STATE_WIFI_TRANSITION || currentState == STATE_WIFI_MODE) {
        return;
    }

    if (isPlaying && currentState != STATE_PLAYING) {
        changeState(STATE_PLAYING);
    } else if (!isPlaying && currentState == STATE_PLAYING) {
        changeState(STATE_PAUSED);
    }
}

void handleCustomFigurine() {
    if (currentState == STATE_BT_MODE) {
        led.blinkError(3);
        return;
    }

    LOG_MAIN_F("Checking custom file: %s", FILE_CUSTOM_STORY);

    if (SD.exists(FILE_CUSTOM_STORY)) {
        LOG_MAIN(">>> FILE FOUND. Playing custom story.");
        changeState(STATE_PLAYING); 
        audioManager.playFile(FILE_CUSTOM_STORY);
        
    } else {
        LOG_MAIN(">>> FILE MISSING. Starting WiFi Sequence.");
        if (SD.exists("/system/need_rec.mp3")) {
            audioManager.playFile("/system/need_rec.mp3");
        } else {
            LOG_MAIN("WARNING: /system/need_rec.mp3 not found!");
        }
        changeState(STATE_WIFI_TRANSITION);
    }
}

void onTagDetected(String uid, String content) {
    if (xSemaphoreTake(nfcMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (!nfcEventPending) {
            pendingUid = uid;
            pendingContent = content;
            nfcEventPending = true;
        }
        xSemaphoreGive(nfcMutex);
    }
}

void processNfcTag() {
    if (millis() - lastNfcTime < 500) return;

    String uid = "";
    String content = "";
    bool hasEvent = false;
    
    if (xSemaphoreTake(nfcMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        if (nfcEventPending) {
            uid = pendingUid;
            content = pendingContent;
            nfcEventPending = false;
            hasEvent = true;
        }
        xSemaphoreGive(nfcMutex);
    }

    if (!hasEvent) return;
    
    lastNfcTime = millis(); 
    
    LOG_MAIN_F("NFC Tag: %s", content.c_str());
    
    if (currentState == STATE_BT_MODE || currentState == STATE_WIFI_MODE || currentState == STATE_WIFI_TRANSITION) {
        LOG_MAIN("Ignored: System busy in WiFi/BT mode");
        return;
    }

    if (content == lastScannedTag) {
        return; 
    }

    lastScannedTag = content;
    if (content.indexOf("192.168") != -1 || content.indexOf("cmd:rec") != -1) {
        isCustomFigurineActive = true;
        handleCustomFigurine();
    } 
    else if (content.indexOf("cmd:") != -1) {
        isCustomFigurineActive = false;
        String folderCode = content.substring(content.indexOf("cmd:") + 4); 
        String folderPath = "/tales/" + folderCode;
        audioManager.playFolder(folderPath);
    }
}

void setup() {
    Serial.begin(115200);
    nfcMutex = xSemaphoreCreateMutex();

    led.begin(); 
    led.setColor(255, 100, 0); 
    btnVol.begin(); 
    btnCtrl.begin();
    
    btnVol.attachClick([]() {
        if (isComboMode || ignoreButtonsUntilRelease) return;
        int oldVol = audioManager.getVolume();
        int v = oldVol + 2;
        if (v > 21) v = 4;
        audioManager.setVolume(v);
    });

    btnVol.attachLongPress([]() {
        if (isComboMode || ignoreButtonsUntilRelease) return;
        if (currentState != STATE_WIFI_MODE && currentState != STATE_WIFI_TRANSITION) {
            if (currentState == STATE_BT_MODE) changeState(STATE_IDLE);
            else changeState(STATE_BT_MODE);
        }
    });

    btnCtrl.attachClick([]() {
        if (isComboMode || ignoreButtonsUntilRelease) return;
        
        if (currentState == STATE_PLAYING || currentState == STATE_PAUSED || currentState == STATE_BT_MODE) {
            audioManager.togglePause();
        }
        else if (currentState == STATE_WAITING_FOR_PLAY) {
            audioManager.playFile(FILE_CUSTOM_STORY);
        }
    });

    btnCtrl.attachLongPress([]() {
        if (isComboMode || ignoreButtonsUntilRelease) return;
         if (currentState == STATE_PLAYING && !isCustomFigurineActive) {
             audioManager.playNext();
         }
         else if (currentState == STATE_BT_MODE) {
             BluetoothA2DPSink* sink = audioManager.getBtSink();
             if (sink) sink->next();
         }
    });

    SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
    if(!SD.begin(PIN_SD_CS)) { 
        Serial.println("SD Card Mount Failed");
        led.setColor(255,0,0); 
        return; 
    }

    theme.begin();
    nfcManager.onTagDetected(onTagDetected);
    nfcManager.begin();
    
    audioManager.begin();
    audioManager.onStateChange(onAudioStateChanged);
    
    webPortal.onUploadComplete([]() {
        LOG_MAIN("Upload Complete. Pending playback.");
        pendingCustomPlayback = true;
    });
    Preferences bootPrefs;
    bootPrefs.begin("audio", false);
    bool btExitPending = bootPrefs.getBool("bt_exit", false);
    
    if (btExitPending) {
        bootPrefs.putBool("bt_exit", false);
        LOG_MAIN("Boot after BT exit - playing bt_off sound");
        led.setColor(128, 0, 128); 
        if (SD.exists("/system/bt_off.wav")) {
            audioManager.playFile("/system/bt_off.wav");
            unsigned long btOffStart = millis();
            while (audioManager.isPlaying() && millis() - btOffStart < 3000) {
                audioManager.loop();
                delay(10);
            }
        }
        theme.setLed("idle", led);
    } else {
        theme.apply("boot", audioManager, led);
    }
    bootPrefs.end();
    
    changeState(STATE_IDLE);
}

void loop() {
    processNfcTag();
    btnVol.loop();
    btnCtrl.loop();
    led.loop();
    
    static unsigned long comboStart = 0;
    bool volPressed = (digitalRead(PIN_BTN_VOL) == HIGH);
    bool ctrlPressed = (digitalRead(PIN_BTN_CTRL) == HIGH);
    bool bothPressed = volPressed && ctrlPressed;
    
    if (ignoreButtonsUntilRelease) {
        if (!volPressed && !ctrlPressed) {
            ignoreButtonsUntilRelease = false; 
            isComboMode = false;
        }
        return; 
    }

    if (bothPressed) {
        if (!isComboMode) {
            isComboMode = true;
            comboStart = millis();
        }
        
        if (millis() - comboStart > 3000) { 
            if (currentState == STATE_BT_MODE) {
                led.setColor(255, 0, 0); 
            } else {
                changeState(STATE_WIFI_MODE);
            }
            ignoreButtonsUntilRelease = true; 
            comboStart = 0;
        }
    } else {
        if (isComboMode) {
            isComboMode = false;
            comboStart = 0;
        }
    }
    if (currentState == STATE_WIFI_MODE) {
        webPortal.loop();
        if (webPortal.isClientConnected()) led.setLoading(true);
        else theme.setLed("wifi_start", led);
        if (audioManager.isPlaying()) {
            audioManager.loop();
        }
        if (pendingCustomPlayback) {
            pendingCustomPlayback = false;
            LOG_MAIN("Processing pending playback...");
            delay(300); 
            audioManager.stop();
            delay(200);
            isCustomFigurineActive = true;
            changeState(STATE_PLAYING);
            delay(100);
            audioManager.playFile(FILE_CUSTOM_STORY);
        }
    } 
    else if (currentState == STATE_WIFI_TRANSITION) {
        audioManager.loop();
        if (!audioManager.isPlaying()) {
            LOG_MAIN("Prompt finished. Switching to WiFi Mode.");
            changeState(STATE_WIFI_MODE);
        }
    }
    else {
        audioManager.loop();

        if (currentState == STATE_WAITING_FOR_PLAY) {
            if (millis() - stateEnterTime > 10000) {
                audioManager.playFile(FILE_CUSTOM_STORY);
            }
        }

        static unsigned long lastTrackEndTime = 0;
        if (currentState == STATE_PLAYING && !audioManager.isPlaying()) {
            if (lastTrackEndTime == 0) {
                lastTrackEndTime = millis();
            }

            if (millis() - lastTrackEndTime > 500) {
                lastTrackEndTime = 0; 
                if (!isCustomFigurineActive) {
                    audioManager.playNext(); 
                } else {
                    changeState(STATE_IDLE); 
                }
            }
        } else {
            lastTrackEndTime = 0;
        }
    }
}