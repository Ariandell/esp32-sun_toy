#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PN532.h>
#include <functional>

class NfcManager {
public:
    NfcManager(uint8_t pinSda, uint8_t pinScl);
    void begin();
    void onTagDetected(std::function<void(String, String)> callback); 
    Adafruit_PN532* getDriver();

private:
    uint8_t _pinSda;
    uint8_t _pinScl;
    Adafruit_PN532 _nfc;
    TaskHandle_t _taskHandle;
    std::function<void(String, String)> _callback;
    static void taskEntry(void* parameter);
    void loopTask();
};