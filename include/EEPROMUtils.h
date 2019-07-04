#include <Arduino.h>
#include <EEPROM.h>

#ifndef EEPROM_UTILS_H
#define EEPROM_UTILS_H

class EEPROMUtils {
    public:
        static void writeUInt16(int address, uint16_t value);
        static uint16_t readUInt16(int address);
};

#endif //EEPROM_UTILS_H