#include "NfcManager.h"
NfcManager* _nfcInstance = nullptr;

NfcManager::NfcManager(uint8_t pinSda, uint8_t pinScl) 
    : _pinSda(pinSda), _pinScl(pinScl), _nfc(pinSda, pinScl), _taskHandle(NULL) {
    _nfcInstance = this;
}

void NfcManager::begin() {
    Wire.begin(_pinSda, _pinScl);
    Wire.setClock(100000); 
    Wire.setTimeOut(3000); 

    _nfc.begin();
    
    uint32_t versiondata = _nfc.getFirmwareVersion();
    if (!versiondata) {
        Serial.println("NFC Module not found!");
        return;
    }
    
    Serial.print("Found chip PN5"); Serial.println((versiondata>>24) & 0xFF, HEX); 
    _nfc.SAMConfig();
    xTaskCreatePinnedToCore(
        NfcManager::taskEntry, "NFC_Task", 4096, this, 1, &_taskHandle, 0
    );
}

void NfcManager::onTagDetected(std::function<void(String, String)> callback) {
    _callback = callback;
}

Adafruit_PN532* NfcManager::getDriver() {
    return &_nfc;
}

void NfcManager::taskEntry(void* parameter) {
    NfcManager* instance = (NfcManager*)parameter;
    instance->loopTask();
}

void NfcManager::loopTask() {
    while(true) {
        uint8_t uid[7]; uint8_t uidLength;
        if (_nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 100)) {
            String uidStr = "";
            for (uint8_t i = 0; i < uidLength; i++) {
                if (uid[i] < 0x10) uidStr += "0";
                uidStr += String(uid[i], HEX);
            }
            uidStr.toUpperCase();
            uint8_t dataBuffer[32]; 
            String content = "";
            bool error = false;
            for (uint8_t page = 4; page < 10; page++) {
                vTaskDelay(15 / portTICK_PERIOD_MS);
                
                if (_nfc.ntag2xx_ReadPage(page, dataBuffer)) {
                    for (int i = 0; i < 4; i++) {
                        char c = (char)dataBuffer[i];
                        if (c != 0 && c != 0xFE && (isalnum(c) || c == ':' || c == '/' || c == '.' || c == '-' || c == '_')) 
                            content += c;
                    }
                } else {
                    error = true;
                    break;
                }
            }
            if (!error && _callback) {
                Serial.print("NFC Found: "); Serial.println(content);
                _callback(uidStr, content);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}