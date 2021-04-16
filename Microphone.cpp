#include <Arduino.h>
#include "Microphone.h"

Microphone::Microphone(int pin, int threshhold) {
    pinMode(pin, INPUT);

    _pin = pin;
    _threshhold = threshhold;
}
void Microphone::read() {
    _level = analogRead(_pin);
    if(isActivated(0)) {
        _last_activation = millis();
    }
}

int Microphone::getLevel() {
    return _level;
}

bool Microphone::isActivated(int smoothing) {
    if(smoothing > 0) {
        return millis() - _last_activation < smoothing;
    } else {
        return _level > _threshhold;
    }
}

void Microphone::setThreshhold(int threshhold) {
    _threshhold = threshhold;
}