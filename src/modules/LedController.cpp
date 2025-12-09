#include "LedController.h"

void LedController::begin() {
    _strip.begin();
    _strip.setBrightness(50);
    setColor(255, 0, 0);
}

void LedController::setColor(uint8_t r, uint8_t g, uint8_t b) {
    _isLoading = false;
    _strip.setPixelColor(0, _strip.Color(r, g, b));
    _strip.show();
}

void LedController::setLoading(bool active) {
    _isLoading = active;
}

void LedController::loop() {
    if (_isLoading) {
        if (millis() - _lastUpdate > 10) {
            _lastUpdate = millis();
            _breathVal += _breathDir;
            if (_breathVal >= 255 || _breathVal <= 0) _breathDir *= -1;
            _strip.setPixelColor(0, _strip.Color(0, 0, _breathVal));
            _strip.show();
        }
    }
}