#include <Arduino.h>
#include <Microphone.h>

Microphone::Microphone(int pin, int threshhold=511) {
    pinMode(pin, INPUT);

    _pin = pin;
    _threshhold = threshhold;
}
void Microphone::read() {
    _level = analogRead(_pin);
}

int Microphone::getLevel() {
    return _level;
}

bool Microphone::isActivated() {
    return _level > _threshhold;
}

void Microphone::setThreshhold(int threshhold) {
    _threshhold = threshhold;
}