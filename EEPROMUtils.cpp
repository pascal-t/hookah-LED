#include <Arduino.h>
#include <EEPROM.h>
#include "EEPROMUtils.h"


void EEPROMUtilsClass::writeUInt16(int address, uint16_t value) {
    if(address == -1) {
        return;
    }
    uint8_t b1 = ((value >> 8) & 0xFF);
    uint8_t b2 = (value & 0xFF);

    EEPROM.update(address, b1);
    EEPROM.update(address+1, b2);
}

uint16_t EEPROMUtilsClass::readUInt16(int address) {
    if(address == -1) {
        return 0;
    }
    uint16_t value = EEPROM.read(address) << 8;
    value += EEPROM.read(address + 1);
    
    return value;
}

void EEPROMUtilsClass::writeInt16(int address, int16_t value) {
    if(address == -1) {
        return;
    }
    uint8_t b1 = ((value >> 8) & 0xFF);
    uint8_t b2 = (value & 0xFF);

    EEPROM.update(address, b1);
    EEPROM.update(address+1, b2);
}

int16_t EEPROMUtilsClass::readInt16(int address) {
    if(address == -1) {
        return 0;
    }
    int16_t value = EEPROM.read(address) << 8;
    value |= EEPROM.read(address + 1);
    
    return value;
}
