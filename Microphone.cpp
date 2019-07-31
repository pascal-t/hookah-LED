#include <Arduino.h>
#include "Microphone.h"

Microphone::Microphone(int pin, int threshhold=511, unsigned long smoothing_duration = 500) {
    pinMode(pin, INPUT);

    _pin = pin;
    _threshhold = threshhold;
    _smoothing_duration = smoothing_duration;
}
void Microphone::read() {
    _level = analogRead(_pin);
    if(isActivated()) {
        _last_activation = millis();
    }
}

int Microphone::getLevel() {
    return _level;
}

bool Microphone::isActivated(bool smooth=false) {
    if(smooth) {
        return millis() - _last_activation < _smoothing_duration;
    } else {
        return _level > _threshhold;
    }
}

void Microphone::setThreshhold(int threshhold) {
    _threshhold = threshhold;
}