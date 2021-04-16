#include <Arduino.h>

#ifndef MICROPHONE_H
#define MICROPHONE_H

class Microphone {
    public:
        Microphone(int pin, int threshhold=511);
        void read();
        int getLevel();
        bool isActivated(int smoothing=20);
        void setThreshhold(int threshhold);
    private:
        int _pin, _level, _threshhold;
        unsigned long _last_activation;
};

#endif //MICROPHONE_H
