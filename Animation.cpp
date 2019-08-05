#include <Arduino.h>
#include "Animation.h"

Animation::Animation(unsigned long frameLength) {
    _frameLength = frameLength;
}

void Animation::start(int frameCount) {
    _runningSince = millis();
    _frameCount = frameCount;
}

bool Animation::isRunning() {
    return  getFrame() < _frameCount;
}

int Animation::getFrame() {
    return (millis() - _runningSince) / _frameLength;
}