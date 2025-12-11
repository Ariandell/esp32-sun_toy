#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <SD.h>
#include "LedController.h"
#include "AudioManager.h"

struct ThemeEvent {
    String soundPath;
    int r, g, b;
    bool hasLed;
    bool hasSound;
};

class ThemeManager {
public:
    void begin();
    void apply(String eventName, AudioManager &audio, LedController &led);
    void setLed(String eventName, LedController &led);
    void playSound(String eventName, AudioManager &audio);

private:
    DynamicJsonDocument _doc{4096};
    bool _loaded = false;
};