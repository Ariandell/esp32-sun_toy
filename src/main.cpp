#include <Arduino.h>
#include <WiFi.h>
#include "Config.h"
#include "modules/AudioManager.h"
#include "modules/NfcManager.h"
#include "modules/ThemeManager.h"
#include "modules/WebPortal.h"
#include "modules/ButtonManager.h"

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

enum AppState {
    STATE_IDLE,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_WIFI_MODE,
    STATE_BT_MODE,
    STATE_WAITING_FOR_PLAY
};

AppState currentState = STATE_IDLE;
String lastScannedTag = "";
unsigned long stateEnterTime = 0;
bool isCustomFigurineActive = false;

volatile bool nfcEventPending = false;
String pendingUid = "";
String pendingContent = "";

bool isComboMode = false; 
void changeState(AppState newState);
const char* stateToString(AppState state);
void processNfcTag();

const char* stateToString(AppState state) {
    switch(state) {
        case STATE_IDLE: return "IDLE";
        case STATE_PLAYING: return "PLAYING";
        case STATE_PAUSED: return "PAUSED";
        case STATE_WIFI_MODE: return "WIFI_MODE";
        case STATE_BT_MODE: return "BT_MODE";
        case STATE_WAITING_FOR_PLAY: return "WAITING_FOR_PLAY";
        default: return "UNKNOWN";
    }
}

void changeState(AppState newState) {
    LOG_MAIN_F("changeState: %s -> %s", stateToString(currentState), stateToString(newState));
    
    if (currentState == newState) {
        LOG_MAIN("  SKIPPED: Same state");
        return; 
    }

    if (currentState == STATE_BT_MODE) {
        LOG_MAIN("  Exit BT_MODE: Stopping Bluetooth...");
        audioManager.stopBluetooth();
        lastScannedTag = "";
    }

    if (currentState == STATE_WIFI_MODE) {
        LOG_MAIN("  Exit WIFI_MODE: Stopping WebPortal...");
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        lastScannedTag = "";
        isCustomFigurineActive = false;
    }
    
    currentState = newState;
    stateEnterTime = millis();
    LOG_MAIN_F("  State changed at: %lu ms", stateEnterTime);

    switch (currentState) {
        case STATE_IDLE:
            LOG_MAIN("  Enter IDLE: Green LED");
            theme.setLed("idle", led);
            break;
        case STATE_PLAYING:
            LOG_MAIN("  Enter PLAYING: Setting LED");
            theme.setLed("playing", led);
            break;
        case STATE_PAUSED:
            LOG_MAIN("  Enter PAUSED: Orange LED");
            theme.setLed("paused", led);
            break;
        case STATE_BT_MODE:
            LOG_MAIN("  Enter BT_MODE: Starting Bluetooth");
            theme.setLed("bt_mode", led);
            audioManager.startBluetooth();
            break;
        case STATE_WIFI_MODE:
            LOG_MAIN("  Enter WIFI_MODE: Starting Access Point");
            theme.setLed("wifi_start", led); 
            webPortal.begin(); 
            break;
        case STATE_WAITING_FOR_PLAY:
            LOG_MAIN("  Enter WAITING_FOR_PLAY: Playing prompt");
            theme.setLed("idle", led);
            theme.apply("play_prompt", audioManager, led);
            break;
    }
    LOG_MAIN("changeState COMPLETE");
}

void handleCustomFigurine() {
    LOG_MAIN("handleCustomFigurine()");
    if (currentState == STATE_BT_MODE) {
        LOG_MAIN("  BLOCKED: Cannot switch to WIFI from BT. Flash error.");
        for(int i=0; i<3; i++) {
            led.setColor(255, 0, 0);
            delay(200);
            theme.setLed("bt_mode", led);
            delay(200);
        }
        return;
    }

    if (SD.exists(FILE_CUSTOM_STORY)) {
        LOG_MAIN("  Custom story EXISTS - switching to WAITING_FOR_PLAY");
        changeState(STATE_WAITING_FOR_PLAY);
    } 
    else {
        LOG_MAIN("  Custom story NOT FOUND - switching to WiFi mode");
        theme.apply("wifi_prompt", audioManager, led);
        delay(4000); 
        changeState(STATE_WIFI_MODE);
    }
}

void handleNormalTag(String folder) {
    LOG_MAIN_F("handleNormalTag('%s')", folder.c_str());
    audioManager.playFolder(folder);
    
    if (currentState == STATE_PLAYING) {
        LOG_MAIN("  Already in PLAYING state, refreshing LED");
        theme.setLed("playing", led);
    } else {
        changeState(STATE_PLAYING);
    }
}

void onTagDetected(String uid, String content) {
    if (!nfcEventPending) {
        pendingUid = uid;
        pendingContent = content;
        nfcEventPending = true;
    }
}

void processNfcTag() {
    if (!nfcEventPending) return;
    
    String uid = pendingUid;
    String content = pendingContent;
    nfcEventPending = false; 
    
    LOG_MAIN_F("========== NFC TAG DETECTED (Processed in Loop) ==========");
    LOG_MAIN_F("  UID: %s", uid.c_str());
    LOG_MAIN_F("  Content: %s", content.c_str());
    
    LOG_MAIN_F("  Content: %s", content.c_str());
    
    if (currentState == STATE_BT_MODE) {
        LOG_MAIN("  IGNORED: In BT mode");
        return;
    }

    if (content == lastScannedTag) {
        LOG_MAIN("  IGNORED: Same tag as before");
        return; 
    }

    lastScannedTag = content;
    LOG_MAIN_F("  Updated lastScannedTag to: %s", content.c_str());

    if (content.indexOf("192.168") != -1 || content.indexOf("cmd:rec") != -1) {
        LOG_MAIN("  TAG TYPE: Custom Figurine");
        isCustomFigurineActive = true;
        handleCustomFigurine();
    } 
    else if (content.indexOf("cmd:") != -1) {
        LOG_MAIN("  TAG TYPE: Normal Tale");
        isCustomFigurineActive = false;
        String folderCode = content.substring(content.indexOf("cmd:") + 4); 
        String folderPath = "/tales/" + folderCode;
        LOG_MAIN_F("  Folder path: %s", folderPath.c_str());
        handleNormalTag(folderPath);
    } else {
        LOG_MAIN("  TAG TYPE: Unknown format, ignoring");
    }
}

void setup() {
    Serial.begin(115200);
    delay(100);
    LOG_MAIN("========================================");
    LOG_MAIN("         SunToy Starting Up");
    LOG_MAIN("========================================");
    
    LOG_MAIN("Initializing LED...");
    led.begin(); 
    led.setColor(255, 100, 0);

    LOG_MAIN("Initializing buttons...");
    btnVol.begin(); 
    btnCtrl.begin();

    btnVol.attachClick([]() {
        if (isComboMode) return;
        int oldVol = audioManager.getVolume();
        int v = oldVol + 2;
        if (v > 21) v = 4;
        LOG_MAIN_F("BTN_VOL Click: Volume %d -> %d", oldVol, v);
        audioManager.setVolume(v);
    });

    btnVol.attachLongPress([]() {
        if (isComboMode) {
            LOG_MAIN("BTN_VOL LongPress IGNORED (Combo Active)");
            return;
        }
        LOG_MAIN_F("BTN_VOL LongPress: CurrentState=%s", stateToString(currentState));
        if (currentState != STATE_WIFI_MODE) {
            if (currentState == STATE_BT_MODE) {
                LOG_MAIN("  Action: Exit BT mode -> IDLE");
                changeState(STATE_IDLE);
            }
            else {
                LOG_MAIN("  Action: Enter BT mode");
                changeState(STATE_BT_MODE);
            }
        } else {
            LOG_MAIN("  IGNORED: In WIFI mode");
        }
    });

    btnCtrl.attachClick([]() {
        if (isComboMode) return;
        LOG_MAIN_F("BTN_CTRL Click: CurrentState=%s", stateToString(currentState));
        if (currentState == STATE_PLAYING || currentState == STATE_PAUSED) {
            LOG_MAIN("  Action: Toggle pause");
            audioManager.togglePause();
            currentState = (currentState == STATE_PLAYING) ? STATE_PAUSED : STATE_PLAYING;
            LOG_MAIN_F("  New state: %s", stateToString(currentState));
            if(currentState == STATE_PAUSED) theme.setLed("paused", led);
            else theme.setLed("playing", led);
        }
        else if (currentState == STATE_BT_MODE) {
            LOG_MAIN("  Action: Toggle BT pause");
            audioManager.togglePause();
        }
        else if (currentState == STATE_WAITING_FOR_PLAY) {
            LOG_MAIN("  Action: Play custom story");
            audioManager.playFile(FILE_CUSTOM_STORY);
            changeState(STATE_PLAYING);
        } else {
            LOG_MAIN("  No action for this state");
        }
    });

    btnCtrl.attachLongPress([]() {
        if (isComboMode) {
            LOG_MAIN("BTN_CTRL LongPress IGNORED (Combo Active)");
            return;
        }
        LOG_MAIN_F("BTN_CTRL LongPress: CurrentState=%s, isCustom=%d", stateToString(currentState), isCustomFigurineActive);
         if (currentState == STATE_PLAYING && !isCustomFigurineActive) {
             LOG_MAIN("  Action: Play next track");
             audioManager.playNext();
         }
         else if (currentState == STATE_BT_MODE) {
             LOG_MAIN("  Action: BT next track");
             BluetoothA2DPSink* sink = audioManager.getBtSink();
             if (sink != nullptr) {
                 sink->next();
             } else {
                 LOG_MAIN("  WARNING: BT sink is null!");
             }
         } else {
             LOG_MAIN("  No action for this state");
         }
    });

    LOG_MAIN("Initializing SPI...");
    SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
    
    LOG_MAIN("Initializing SD card...");
    if(!SD.begin(PIN_SD_CS)) { 
        LOG_MAIN("ERROR: SD card init failed!");
        led.setColor(255,0,0); 
        return; 
    }
    LOG_MAIN("SD card OK!");

    LOG_MAIN("Initializing ThemeManager...");
    theme.begin();
    
    LOG_MAIN("Initializing NFC...");
    nfcManager.onTagDetected(onTagDetected);
    nfcManager.begin();
    
    LOG_MAIN("Initializing AudioManager...");
    audioManager.begin();
    
    LOG_MAIN("Setting up WebPortal callback...");
    webPortal.onUploadComplete([]() {
        LOG_MAIN("WebPortal: Upload complete callback!");
        changeState(STATE_PLAYING);
        // Set flag AFTER 'changeState', because changeState(WIFI->PLAYING) resets it
        isCustomFigurineActive = true; 
        audioManager.playFile(FILE_CUSTOM_STORY);
    });
    
    LOG_MAIN("Playing boot theme...");
    theme.apply("boot", audioManager, led);
    
    LOG_MAIN("Changing to IDLE state...");
    changeState(STATE_IDLE);
    
    LOG_MAIN("========================================");
    LOG_MAIN("         Setup Complete!");
    LOG_MAIN("========================================");
}

void loop() {
    processNfcTag();
    btnVol.loop();
    btnCtrl.loop();
    led.loop();
    
    static unsigned long comboStart = 0;
    static unsigned long lastComboActiveTime = 0;
    static unsigned long comboReleaseStart = 0;
    
    bool bothPressed = (digitalRead(PIN_BTN_VOL) == HIGH && digitalRead(PIN_BTN_CTRL) == HIGH);

    if (bothPressed) {
        lastComboActiveTime = millis();
        comboReleaseStart = 0; 
        if (!isComboMode) {
            LOG_MAIN("Combo buttons detected - entering Combo Mode");
            isComboMode = true; 
        }
        
        if (comboStart == 0) {
            comboStart = millis();
            LOG_MAIN("Combo timer started");
        }
        
        if (millis() - comboStart > 3000) { 
            // Allow WiFi mode entry regardless of active tag
            if (currentState == STATE_BT_MODE) {
                LOG_MAIN("  Combo BLOCKED in BT mode. Flash error.");
                 for(int i=0; i<3; i++) {
                    led.setColor(255, 0, 0);
                    delay(200);
                    theme.setLed("bt_mode", led);
                    delay(200);
                }
                comboStart = 0; 
                while(digitalRead(PIN_BTN_VOL) == HIGH || digitalRead(PIN_BTN_CTRL) == HIGH) delay(50);
            } else {
                LOG_MAIN("Combo: 3 seconds - switching to WIFI mode");
                 changeState(STATE_WIFI_MODE);
                 comboStart = 0; 
                 while(digitalRead(PIN_BTN_VOL) == HIGH || digitalRead(PIN_BTN_CTRL) == HIGH) delay(50);
            }
        }
    } else {
        // Debounce: Only clear combo timer if released for > 100ms
        if (comboStart > 0) {
            if (comboReleaseStart == 0) comboReleaseStart = millis();
            
            if (millis() - comboReleaseStart > 100) {
                comboStart = 0; // Confirmed release
            }
        } else {
            comboStart = 0;
        }

        if (isComboMode && (millis() - lastComboActiveTime > 1000)) {
             LOG_MAIN("Combo buttons released (cooldown passed) - exiting Combo Mode");
             isComboMode = false;
        }
    }

    if (currentState == STATE_WIFI_MODE) {
        webPortal.loop();
        if (webPortal.isClientConnected()) led.setLoading(true);
        else theme.setLed("wifi_start", led);
    } 
    else {
        audioManager.loop();

        if (currentState == STATE_WAITING_FOR_PLAY) {
            if (millis() - stateEnterTime > 10000) {
                LOG_MAIN("WAITING_FOR_PLAY timeout - auto-starting custom story");
                audioManager.playFile(FILE_CUSTOM_STORY);
                changeState(STATE_PLAYING);
            }
        }

        if (currentState == STATE_PLAYING && !audioManager.isPlaying()) {
            if (!isCustomFigurineActive) {
                LOG_MAIN("Track ended - playing next");
                audioManager.playNext(); 
            } else {
                LOG_MAIN("Custom story ended - returning to IDLE");
                changeState(STATE_IDLE); 
            }
        }
    }
}