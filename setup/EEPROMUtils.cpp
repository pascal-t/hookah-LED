#include <Arduino.h>
#include <EEPROM.h>
#include "EEPROMUtils.h"


static void EEPROMUtilsClass::writeUInt16(int address, uint16_t value) {
    uint8_t b1 = ((value >> 8) & 0xFF);
    uint8_t b2 = (value & 0xFF);

    EEPROM.update(address, b1);
    EEPROM.update(address+1, b2);
}

static uint16_t EEPROMUtilsClass::readUInt16(int address) {
    uint16_t value = EEPROM.read(address) << 8;
    value += EEPROM.read(address + 1);
    
    return value;
}