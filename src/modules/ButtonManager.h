#pragma once
#include <Arduino.h>
#include <functional>

class Button {
public:
    Button(uint8_t pin);
    void begin();
    void loop();
    void attachClick(std::function<void()> callback);
    void attachLongPress(std::function<void()> callback);

private:
    uint8_t _pin;
    int _state;
    unsigned long _startTime;
    std::function<void()> _clickCb;
    std::function<void()> _longCb;
};