#include <EEPROM.h>
#include "EEPROMUtils.h"

uint16_t settingBrightness = 0;
#define SETTING_BRIGHTNESS_ADDRESS 0

uint16_t settingColorHue = 0;
#define SETTING_COLOR_HUE_ADDRESS 2
uint16_t settingColorSaturation = 255;
#define SETTING_COLOR_SATURATION_ADDRESS 4

uint16_t settingRainbowStep = 280;
#define SETTING_RAINBOW_STEP_ADDRESS 6

uint16_t settingMicrophone = 0;
#define SETTING_MICROPHONE_ADDRESS 8

#define MODE_ADDRESS 10

uint16_t settingMicrophoneThreshhold = 525;
#define SETTING_MICROPHONE_THRESHHOLD_ADDRESS 12

void setup() {
  pinMode(13, OUTPUT);

  for (int i = 0 ; i < EEPROM.length() ; i++) {
    EEPROM.write(i, 0);
  }

  EEPROMUtils.writeUInt16(SETTING_BRIGHTNESS_ADDRESS, settingBrightness);
  
  EEPROMUtils.writeUInt16(SETTING_COLOR_HUE_ADDRESS, settingColorHue);
  EEPROMUtils.writeUInt16(SETTING_COLOR_SATURATION_ADDRESS, settingColorSaturation);
  
  EEPROMUtils.writeUInt16(SETTING_RAINBOW_STEP_ADDRESS, settingRainbowStep);

  EEPROMUtils.writeUInt16(SETTING_MICROPHONE_ADDRESS, settingMicrophone);
  EEPROMUtils.writeUInt16(SETTING_MICROPHONE_THRESHHOLD_ADDRESS, settingMicrophoneThreshhold);
  EEPROMUtils.writeUInt16(SETTING_BRIGHTNESS_ADDRESS, settingBrightness);
  EEPROMUtils.writeUInt16(SETTING_BRIGHTNESS_ADDRESS, settingBrightness);

  // turn the LED on when we're done
  digitalWrite(13, HIGH); 
} 

void loop() {
  
}
