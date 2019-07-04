#include <Arduino.h>

#ifndef BUTTON_H
#define BUTTON_H

#define BUTTON_DEBOUNCE 50

class Button {
    public:
        Button(int pin, int longPressDuration=300, int mode=INPUT);
        void read();
        bool isShortPress();
        bool isLongPress();
    private:
        int _pin, _last, _unpressed, _longPressDuration;
        unsigned long _pressedSince = 0, _debounce = 0;
        bool _shortPress = false, _longPress = false, _longPressSent = false;
};

#endif //BUTTON_H
