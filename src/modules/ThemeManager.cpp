#include "ThemeManager.h"

void ThemeManager::begin() {
    if (!SD.exists("/system/config.json")) {
        Serial.println("Config file not found! Using defaults.");
        return;
    }

    File file = SD.open("/system/config.json");
    DeserializationError error = deserializeJson(_doc, file);
    file.close();

    if (error) {
        Serial.print("JSON Error: "); Serial.println(error.c_str());
        return;
    }

    _loaded = true;
    Serial.println("Theme Config Loaded!");
}

void ThemeManager::apply(String eventName, AudioManager &audio, LedController &led) {
    if (!_loaded) return;
    if (_doc.containsKey(eventName)) {
        JsonObject event = _doc[eventName];
        if (event.containsKey("led")) {
            JsonArray color = event["led"];
            led.setColor(color[0], color[1], color[2]);
        }
        if (event.containsKey("sound")) {
            const char* path = event["sound"];
            audio.playFile(String(path));
        }
    } else {
        Serial.print("Event not found: "); Serial.println(eventName);
    }
}

void ThemeManager::setLed(String eventName, LedController &led) {
    if (!_loaded || !_doc.containsKey(eventName)) return;
    JsonObject event = _doc[eventName];
    if (event.containsKey("led")) {
        JsonArray color = event["led"];
        led.setColor(color[0], color[1], color[2]);
    }
}

void ThemeManager::playSound(String eventName, AudioManager &audio) {
    if (!_loaded || !_doc.containsKey(eventName)) return;
    JsonObject event = _doc[eventName];
    if (event.containsKey("sound")) {
        const char* path = event["sound"];
        audio.playFile(String(path));
    }
}