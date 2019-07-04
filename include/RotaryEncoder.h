#include <Arduino.h>

#ifndef ROTARY_ENCODER_H
#define ROTARY_ENCODER_H

class RotaryEncoder {
    public:
        RotaryEncoder(int pinA, int pinB);
        void read();
        int getRotation();
    private:
        int _pinA, _pinB, _aLast, rotation = 0;
};

#endif //ROTARY_ENCODER_H
