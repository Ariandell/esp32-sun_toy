#include "NfcManager.h"

#define DEBUG_NFC 1

#if DEBUG_NFC
    #define LOG_NFC(msg) Serial.println("[NFC] " msg)
    #define LOG_NFC_F(fmt, ...) Serial.printf("[NFC] " fmt "\n", ##__VA_ARGS__)
#else
    #define LOG_NFC(msg)
    #define LOG_NFC_F(fmt, ...)
#endif

NfcManager* _nfcInstance = nullptr;

NfcManager::NfcManager(uint8_t pinSda, uint8_t pinScl) 
    : _pinSda(pinSda), _pinScl(pinScl), _nfc(pinSda, pinScl), _taskHandle(NULL) {
    _nfcInstance = this;
    LOG_NFC("Constructor called");
}

void NfcManager::begin() {
    LOG_NFC("begin() - Initializing NFC");
    LOG_NFC_F("  I2C Pins: SDA=%d, SCL=%d", _pinSda, _pinScl);
    
    Wire.begin(_pinSda, _pinScl);
    Wire.setClock(50000); 
    Wire.setTimeOut(10000); 

    _nfc.begin();
    
    uint32_t versiondata = _nfc.getFirmwareVersion();
    if (!versiondata) {
        LOG_NFC("ERROR: NFC Module not found!");
        Serial.println("NFC Module not found!");
        return;
    }
    
    LOG_NFC_F("Found chip PN5%02X", (versiondata>>24) & 0xFF);
    Serial.print("Found chip PN5"); Serial.println((versiondata>>24) & 0xFF, HEX); 
    
    _nfc.SAMConfig();
    LOG_NFC("SAMConfig complete");
    
    LOG_NFC("Starting NFC task on Core 0 with priority 3...");
    xTaskCreatePinnedToCore(
        NfcManager::taskEntry, "NFC_Task", 8192, this, 3, &_taskHandle, 0 
    );
    LOG_NFC("NFC initialization complete");
}

void NfcManager::onTagDetected(std::function<void(String, String)> callback) {
    _callback = callback;
    LOG_NFC("Callback registered");
}

Adafruit_PN532* NfcManager::getDriver() {
    return &_nfc;
}

void NfcManager::taskEntry(void* parameter) {
    NfcManager* instance = (NfcManager*)parameter;
    instance->loopTask();
}

void NfcManager::loopTask() {
    LOG_NFC("Task started - entering main loop");
    
    while(true) {
        uint8_t uid[7]; 
        uint8_t uidLength;
        if (_nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 100)) {
            vTaskDelay(30 / portTICK_PERIOD_MS);
            
            String uidStr = "";
            for (uint8_t i = 0; i < uidLength; i++) {
                if (uid[i] < 0x10) uidStr += "0";
                uidStr += String(uid[i], HEX);
            }
            uidStr.toUpperCase();
            uint8_t dataBuffer[16]; 
            memset(dataBuffer, 0, sizeof(dataBuffer));
            
            String content = "";
            bool error = false;
            int errorCount = 0;
            
            for (uint8_t page = 4; page < 10; page++) {
                bool pageRead = false;
                for(int r = 0; r < 5; r++) {
                    memset(dataBuffer, 0, sizeof(dataBuffer));
                    
                    if (_nfc.ntag2xx_ReadPage(page, dataBuffer)) {
                        pageRead = true;
                        break;
                    }
                    vTaskDelay(30 / portTICK_PERIOD_MS);
                }
                
                if (pageRead) {
                    for (int i = 0; i < 4; i++) {
                        char c = (char)dataBuffer[i];
                        if (c >= 0x20 && c <= 0x7E) { 
                            if (isalnum(c) || c == ':' || c == '/' || c == '.' || c == '-' || c == '_') {
                                content += c;
                            }
                        }
                    }
                } else {
                    errorCount++;
                    if (errorCount >= 2) {
                        error = true;
                        LOG_NFC_F("Page %d read failed after 5 retries", page);
                        break;
                    }
                }
                vTaskDelay(20 / portTICK_PERIOD_MS);
            }
            if (!error && content.length() > 3 && _callback) {
                if (content.indexOf("cmd:") != -1 || content.indexOf("192.168") != -1) {
                    LOG_NFC_F("Valid tag content: %s", content.c_str());
                    _callback(uidStr, content);
                    vTaskDelay(2000 / portTICK_PERIOD_MS); 
                } else {
                    LOG_NFC_F("Invalid content (no cmd: or IP): %s", content.c_str());
                }
            }
        }
        vTaskDelay(300 / portTICK_PERIOD_MS);
    }
}