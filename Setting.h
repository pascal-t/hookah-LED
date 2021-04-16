#include <Arduino.h>

#ifndef SETTING_H
#define SETTING_H

class Setting {
    public:
        Setting(int eepromAddress, int max, int min=0, int step=1, bool rollover=false, bool binary=false);
        int32_t get();
        int getValue();
        void setValue(int value);
        void increase();
        void decrease();
    private:
        int _eepromAddress, _max, _min, _step;
        int16_t _value;
        bool _rollover, _binary;
};

#endif //SETTING_H