#include <Arduino.h>

#ifndef MICROPHONE_H
#define MICROPHONE_H

class Microphone {
    public:
        Microphone(int pin, int threshhold=511);
        void read();
        int getLevel();
        bool isActivated();
        void setThreshhold(int threshhold);
    private:
        int _pin, _level, _threshhold;
};

#endif //MICROPHONE_H
