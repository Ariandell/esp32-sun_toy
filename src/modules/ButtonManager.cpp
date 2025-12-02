#include "ButtonManager.h"

Button::Button(uint8_t pin) : _pin(pin) {}

void Button::begin() { 
    pinMode(_pin, INPUT_PULLDOWN); 
    _state = LOW; 
}

void Button::attachClick(std::function<void()> callback) { _clickCb = callback; }
void Button::attachLongPress(std::function<void()> callback) { _longCb = callback; }

void Button::loop() {
    int read = digitalRead(_pin);
    if (read == HIGH && _state == LOW) { 
        _startTime = millis();
        _state = HIGH; 
    } 
    else if (read == LOW && _state == HIGH) { 
        unsigned long duration = millis() - _startTime;
        _state = LOW; 
        
        if (duration > 50 && duration < 800) {
            if (_clickCb) _clickCb();
        } else if (duration >= 800) {
            if (_longCb) _longCb();
        }
    }
}