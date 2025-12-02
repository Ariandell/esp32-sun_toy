#pragma once
#include <Adafruit_NeoPixel.h>
#include "Config.h"

class LedController {
public:
    void begin();
    void setColor(uint8_t r, uint8_t g, uint8_t b);
    void setLoading(bool active);
    void loop();

private:
    Adafruit_NeoPixel _strip = Adafruit_NeoPixel(1, PIN_LED, NEO_GRB + NEO_KHZ800);
    bool _isLoading = false;
    unsigned long _lastUpdate = 0;
    int _breathVal = 0;
    int _breathDir = 1;
};