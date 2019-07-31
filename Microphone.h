#include <Arduino.h>

#ifndef MICROPHONE_H
#define MICROPHONE_H

class Microphone {
    public:
        Microphone(int pin, int threshhold=511, unsigned long smoothing_duration=500);
        void read();
        int getLevel();
        bool isActivated(bool smooth=false);
        void setThreshhold(int threshhold);
    private:
        int _pin, _level, _threshhold;
        unsigned long _smoothing_duration, _last_activation;
};

#endif //MICROPHONE_H
