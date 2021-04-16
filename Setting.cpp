#include <Arduino.h>
#include "EEPROMUtils.h"
#include "Setting.h"

Setting::Setting(int eepromAddress, int max, int min, int step, bool rollover, bool binary) {
    _eepromAddress = eepromAddress;
    _max = max;
    _min = min;
    _step = step;
    _rollover = rollover;
    _binary = binary;
    _value = EEPROMUtils.readInt16(eepromAddress);
}

int32_t Setting::get() {
    if (_binary && _value == _max) {
        return _value * _step - 1;
    }
    return _value * _step;
}

int Setting::getValue() {
    return _value;
}

void Setting::setValue(int value) {
    if(value > _max) {
        _value = _max;
    } else if(value < _min) {
        _value = _min;
    } else {
        _value = value;
    }
} 

void Setting::increase() {
    // Serial.print("Increasing Setting: current: ");
    // Serial.print(_value);
    // Serial.print(" Step: ");
    // Serial.print(_step);
    // Serial.print(" (");
    // Serial.print(get());
    // Serial.print(") Max: ");
    // Serial.print(_max);
    // Serial.print(" -> ");

    _value++;
    if (_value > _max) {
        _value = _rollover ? _min : _max;
    }

    // Serial.print(_value);
    // Serial.print(" (");
    // Serial.print(get());
    // Serial.println(")");

    EEPROMUtils.writeInt16(_eepromAddress, _value);
}

void Setting::decrease() {
    Serial.print("Decreasing Setting: current: ");
    Serial.print(_value);
    Serial.print(" Step: ");
    Serial.print(_step);
    Serial.print(" (");
    Serial.print(get());
    Serial.print(") Min: ");
    Serial.print(_min);
    Serial.print(" -> ");

    _value--;
    if (_value < _min) {
        _value = _rollover ? _max : _min;
    }

    Serial.print(_value);
    Serial.print(" (");
    Serial.print(get());
    Serial.println(")");

    EEPROMUtils.writeInt16(_eepromAddress, _value);
}