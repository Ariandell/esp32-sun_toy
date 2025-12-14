#include "LedController.h"

void LedController::begin() {
    _strip.begin();
    _strip.setBrightness(50);
    setColor(255, 0, 0);
}

void LedController::setColor(uint8_t r, uint8_t g, uint8_t b) {
    _isLoading = false;
    _blinkCount = 0;
    _savedR = r; _savedG = g; _savedB = b;
    _strip.setPixelColor(0, _strip.Color(r, g, b));
    _strip.show();
}

void LedController::setLoading(bool active) {
    _isLoading = active;
    _blinkCount = 0;
}

void LedController::blinkError(int count) {
    _blinkCount = count * 2;
    _blinkPhase = 0;
    _blinkLastUpdate = millis();
}

void LedController::loop() {
    if (_blinkCount > 0) {
        if (millis() - _blinkLastUpdate > 200) {
            _blinkLastUpdate = millis();
            if (_blinkPhase % 2 == 0) {
                _strip.setPixelColor(0, _strip.Color(255, 0, 0));
            } else {
                _strip.setPixelColor(0, _strip.Color(_savedR, _savedG, _savedB)); 
            }
            _strip.show();
            _blinkPhase++;
            _blinkCount--;
        }
        return;
    }
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