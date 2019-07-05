#include <Arduino.h>
#include <EEPROM.h>

#ifndef EEPROM_UTILS_H
#define EEPROM_UTILS_H

class EEPROMUtilsClass {
    public:
        static void writeUInt16(int address, uint16_t value);
        static uint16_t readUInt16(int address);
};

extern EEPROMUtilsClass EEPROMUtils;

#endif //EEPROM_UTILS_H