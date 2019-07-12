#include <Arduino.h>
#include "Button.h"

Button::Button(int pin, int longPressDuration, int mode=INPUT) {
    pinMode(pin, mode);

    _pin = pin;
    _unpressed = _last = digitalRead(pin);
    _longPressDuration = longPressDuration;
}

void Button::read() {
    if(millis() - _debounce > BUTTON_DEBOUNCE) {
        int state = digitalRead(_pin);
        if(state != _last) { //Button state changed
            _debounce = millis(); //delay next read on state change to avoid reading the "bouncing" of the button

            if(state != _unpressed) {
                _pressedSince = millis(); //When the state changed to pressed down, start a timer
            } else {
                //since only either a short press or a long press can be detected, short press is false if a long press was sent
                _shortPress = !_longPressSent; 
            }
        } else if(state != _unpressed) { //Button is held down

            if(!_longPressSent && millis() - _pressedSince > _longPressDuration) {
                //a long press is sent when the button is still held down to improve snappiness
                _longPress = _longPressSent = true;
            } else if(_longPressSent && _longPress) { 
                //reset the variable if a long press was already sent
                _longPress = false;
            }
        } else if (state == _unpressed){ //Button is not detecting any action
            _shortPress = false;
            _longPress = false;
            _longPressSent = false;
        }

        _last = state;
    }
}

bool Button::isShortPress() {
    return _shortPress;
}
bool Button::isLongPress() {
    return _longPress;
}


