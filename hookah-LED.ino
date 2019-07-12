#include <Arduino.h>
#include "Button.h"
#include "RotaryEncoder.h"
#include "Microphone.h"
#include "EEPROMUtils.h"
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
 #include <avr/power.h> // Required for 16 MHz Adafruit Trinket
#endif

#define LED 9
#define LED_COUNT 13
#define UPDATE_DELAY 20 //50 Updates/Second
unsigned long lastUpdate = 0;

const double LED_BLINK_BRIGHTNESS_FACTOR = 2.0;
const double LED_BLINK_RESET_FACTOR = 0.95;
const double RAINBOW_ACCELERATION_FACTOR = 4;
const double WHITE_DIM = 0.66; //Dim the LEDs in white mode, since it is the only time all Colors of one LED are lit up

Button button(10, 500, INPUT_PULLUP);
RotaryEncoder rotary(14,16);
Microphone microphone(A0);

Adafruit_NeoPixel strip(LED_COUNT, LED, NEO_GRB + NEO_KHZ800);
bool redrawLEDs = true;

struct hsvColor {
    uint16_t hue = 0;
    uint8_t saturation = 0;
    uint8_t value = 0;
};

struct Setting {
    int eepromAddress;
    uint16_t* value;
    uint16_t min;
    uint16_t max;
    uint16_t step;
    bool rollover;
};

hsvColor LED_HSV_COLORS[LED_COUNT];

#define MODE_COLOR 0
#define MODE_RAINBOW 1
#define MODE_FADE 2
#define MODE_WHITE 3

#define MODE_COLOR_NUM_SETTINGS 5
#define MODE_RAINBOW_NUM_SETTINGS 4
#define MODE_FADE_NUM_SETTINGS 5
#define MODE_WHITE_NUM_SETTINGS 3

int currentMode = MODE_COLOR;
int currentModeNumSettings = MODE_COLOR_NUM_SETTINGS;
int currentSetting = 0;

uint16_t settingBrightness = 0;
#define SETTING_BRIGHTNESS_ADDRESS 0
uint16_t settingSaturation = 0;
#define SETTING_SATURATION_ADDRESS 4

uint16_t settingColorHue = 0;
#define SETTING_COLOR_HUE_ADDRESS 2

uint16_t settingRainbowStep = 0;
#define SETTING_RAINBOW_STEP_ADDRESS 6

uint16_t settingFadeStep = 0;
#define SETTING_FADE_STEP_ADDRESS 14

uint16_t settingMicrophone = 0;
#define SETTING_MICROPHONE_ADDRESS 8

#define MODE_ADDRESS 10

uint16_t settingMicrophoneThreshhold = 0;
#define SETTING_MICROPHONE_THRESHHOLD_ADDRESS 12


void setup() {
    Serial.begin(9600);

    //Setup LED Strip
#if defined(__AVR_ATtiny85__) && (F_CPU == 16000000)
    clock_prescale_set(clock_div_1);
#endif

    strip.begin();           
    strip.show();
    strip.setBrightness(150);

    //Load Settings from EEPROM
    settingBrightness = EEPROMUtils.readUInt16(SETTING_BRIGHTNESS_ADDRESS);
    settingSaturation = EEPROMUtils.readUInt16(SETTING_SATURATION_ADDRESS);

    settingColorHue = EEPROMUtils.readUInt16(SETTING_COLOR_HUE_ADDRESS);

    settingRainbowStep = EEPROMUtils.readUInt16(SETTING_RAINBOW_STEP_ADDRESS);

    settingFadeStep = EEPROMUtils.readUInt16(SETTING_FADE_STEP_ADDRESS);

    settingMicrophone = EEPROMUtils.readUInt16(SETTING_MICROPHONE_ADDRESS);

    settingMicrophoneThreshhold = EEPROMUtils.readUInt16(SETTING_MICROPHONE_THRESHHOLD_ADDRESS);

    Serial.println("Loaded settings from EEPROM:");
    setMode(EEPROMUtils.readUInt16(MODE_ADDRESS));

    Serial.print("settingBrightness: ");
    Serial.println(settingBrightness);
    Serial.print("settingSaturation: ");
    Serial.println(settingSaturation);

    Serial.print("settingColorHue: ");
    Serial.println(settingColorHue);
    
    Serial.print("settingRainbowStep: ");
    Serial.println(settingRainbowStep);

    Serial.print("settingFadeStep: ");
    Serial.println(settingFadeStep);
    
    Serial.print("settingMicrophone: ");
    Serial.println(settingMicrophone);

    Serial.print("settingMicrophoneThreshhold: ");
    Serial.println(settingMicrophoneThreshhold);
    microphone.setThreshhold(settingMicrophoneThreshhold);
}

void readInputs() {
    button.read();
    rotary.read();
    microphone.read();
}

void loop() {
    readInputs();

    if(button.isShortPress()) {
      Serial.println("Changing setting");
      cycleSettings();
    }
    if(button.isLongPress()) {
      Serial.println("Changing mode");
      cycleMode();
    }

    if(rotary.getRotation() != 0) {
        Serial.print("updateSetting() ");
        Serial.println(rotary.getRotation());
        updateSetting();
    }

    //Update the LED Strip at a limited rate
    if(millis() - lastUpdate > UPDATE_DELAY) {
        lastUpdate = millis();
        updateLEDs();
    }
}

Setting settings[4][5] = {
    { //MODE_COLOR
        {SETTING_COLOR_HUE_ADDRESS,             &settingColorHue,             0,   UINT16_MAX, 1024, true }, //hue, 64 steps for full rotation (little more than 3 turns)
        {SETTING_SATURATION_ADDRESS,            &settingSaturation,           65,  255,        16,   false}, //Saturation, 16 Steps
        {SETTING_BRIGHTNESS_ADDRESS,            &settingBrightness,           31,  127,        8,    false}, //Value/Brightness, 16 Steps is stored at the same address across modes
        {SETTING_MICROPHONE_ADDRESS,            &settingMicrophone,           0,   1,          1,    false}, //Microphone is stored at the same address across modes
        {SETTING_MICROPHONE_THRESHHOLD_ADDRESS, &settingMicrophoneThreshhold, 400, 1023,       1,    false}  //Microphone threshhold is stored at the same address across modes
    }, { //MODE_RAINBOW 
        {SETTING_RAINBOW_STEP_ADDRESS,          &settingRainbowStep,          0,   UINT16_MAX, 8, true }, //Rotation step, rolls over so it can rotate backwards
        {SETTING_BRIGHTNESS_ADDRESS,            &settingBrightness,           31,  127,        8, false}, //Brightness, 16 Steps is stored at the same address across modes
        {SETTING_MICROPHONE_ADDRESS,            &settingMicrophone,           0,   1,          1, false}, //Microphone is stored at the same address across modes
        {SETTING_MICROPHONE_THRESHHOLD_ADDRESS, &settingMicrophoneThreshhold, 400, 1023,       1, false}  //Microphone threshhold is stored at the same address across modes
    }, { //MODE_FADE
        {SETTING_FADE_STEP_ADDRESS,             &settingFadeStep,             0,   UINT16_MAX, 16, true }, //FadeStep, how far the Hue changes when smoking
        {SETTING_SATURATION_ADDRESS,            &settingSaturation,           65,  255,        16, false}, //Saturation, 16 Steps is stored at the same address across modes
        {SETTING_BRIGHTNESS_ADDRESS,            &settingBrightness,           31,  127,        8,  false}, //Brightness, 16 Steps is stored at the same address across modes
        {SETTING_MICROPHONE_ADDRESS,            &settingMicrophone,           0,   1,          1,  false}, //Microphone is stored at the same address across modes
        {SETTING_MICROPHONE_THRESHHOLD_ADDRESS, &settingMicrophoneThreshhold, 400, 1023,       1,  false}  //Microphone threshhold is stored at the same address across modes
    }, { //MODE_WHITE 
        {SETTING_BRIGHTNESS_ADDRESS,            &settingBrightness,           31,  127,  8, false}, //Brightness, 16 Steps is stored at the same address across modes
        {SETTING_MICROPHONE_ADDRESS,            &settingMicrophone,           0,   1,    1, false}, //Microphone is stored at the same address across modes
        {SETTING_MICROPHONE_THRESHHOLD_ADDRESS, &settingMicrophoneThreshhold, 400, 1023, 1, false}  //Microphone threshhold is stored at the same address across modes
    }
};

void cycleMode() {
    setMode(currentMode+1);
}

void setMode(int mode) {
    switch(mode) {
        case MODE_RAINBOW:
            currentMode = MODE_RAINBOW;
            currentModeNumSettings = MODE_RAINBOW_NUM_SETTINGS;
            break;
        case MODE_FADE:
            currentMode = MODE_FADE;
            currentModeNumSettings = MODE_FADE_NUM_SETTINGS;
            break;
        case MODE_WHITE:
            currentMode = MODE_WHITE;
            currentModeNumSettings = MODE_WHITE_NUM_SETTINGS;
            break;
        case MODE_COLOR:
        default:
            currentMode = MODE_COLOR;
            currentModeNumSettings = MODE_COLOR_NUM_SETTINGS;
            break;
    }
    Serial.print("Switched to mode ");
    Serial.println(currentMode);
    EEPROMUtils.writeUInt16(MODE_ADDRESS, currentMode);
    currentSetting = 0;
    redrawLEDs = true;
}


void cycleSettings() {
    ++currentSetting %= currentModeNumSettings;
    redrawLEDs = true;
    Serial.print("Current mode: ");
    Serial.println(currentMode);
    Serial.print("Switched to setting ");
    Serial.println(currentSetting);
    if(settings[currentMode][currentSetting].value == &settingMicrophoneThreshhold && settingMicrophone == 0) {
        Serial.println("Microphone deactivated. Skipping threshhold setting.");
        cycleSettings();
    }
}

void updateLEDs() {
    switch(currentMode) {
        case MODE_COLOR:
            updateColor();
            break;
        case MODE_RAINBOW:
            updateRainbow();
            break;
        case MODE_FADE:
            updateFade();
            break;
        case MODE_WHITE:
            updateWhite();
            break;
    }
    indicateSetting();
    strip.show();
}

void graduallyResetColors(uint16_t hue, uint8_t saturation, uint8_t value, uint16_t start=0, uint16_t count=0) {
    hsvColor newColor;

    uint16_t c = count == 0 ? LED_COUNT : count;
    
    for(int i=start; i<start+c; i++) {
        hsvColor pixelColor = getPixelHSV(i);
        if(pixelColor.saturation != saturation
        || pixelColor.value      != value) {
        
            //there is no use case for resetting the hue of a color, so I will spare myself the unreadable algorithm  
            newColor.hue = hue;
            newColor.saturation = saturation + (pixelColor.saturation - saturation) * LED_BLINK_RESET_FACTOR;
            newColor.value = value + (pixelColor.value - value) * LED_BLINK_RESET_FACTOR;

            setPixelHSV(i, newColor);
        }
    }
}

void updateColor() {
    //Only update whole strip when a setting is changed or if we just changed to this mode / exited the settings
    if(redrawLEDs) {
        fillPixelsHSV(settingColorHue, settingSaturation, settingBrightness, 0, 0);
        redrawLEDs = false;
    }

    if(settingMicrophone == 1) {
        //Gradually reset pixels to original color (only execute if microphone is activated)
        graduallyResetColors(settingColorHue, settingSaturation, settingBrightness);

        //make a random pixel light up
        if(microphone.isActivated()) {
            setPixelHSV(random(LED_COUNT), settingColorHue, settingSaturation / 2, settingBrightness * LED_BLINK_BRIGHTNESS_FACTOR);
        } 
    }
}

uint16_t rainbowCurrentPosition = 0;
unsigned long rainbowMicrophoneStart = 0;
int rainbowMicrophoneDuration = 500;
void updateRainbow() {
    if(settingMicrophone == 1) {
        if(microphone.isActivated()) {
            rainbowMicrophoneStart = millis();
        }

        //increase the hue of the rainbow. When thw varable overflows it starts over, wich is actually convenient
        if(millis() - rainbowMicrophoneStart < rainbowMicrophoneDuration) {
            //turn another stepwidth if microphon was listening in the last 500ms (turn twice as fast)
            rainbowCurrentPosition += settingRainbowStep * RAINBOW_ACCELERATION_FACTOR;
        } else {
            rainbowCurrentPosition += settingRainbowStep;
        }
    }

    //Pixels are positioned as follows: 
    //  0 is in the center
    //  1 is halfway from the center
    //  2 is furthest from the center
    //  from then on it is alternating between halfway and furthest.
    setPixelHSV(0, 0,0,settingBrightness); //the center is always white
    for(int i = 1; i < strip.numPixels(); i++) {
        //Pixels furthest from the center are displayed at max saturation while the others are at half saturation
        setPixelHSV(i, (UINT16_MAX/12) * i + rainbowCurrentPosition, 255, settingBrightness);
    }
}

uint16_t fadeHue = 0;
void updateFade() {
    if(settingMicrophone == 1) {
        //make all pixels change color around the color wheel
        if(microphone.isActivated() ) {
            fadeHue += settingFadeStep;
        } 
    }
    
    fillPixelsHSV(fadeHue, settingSaturation, settingBrightness, 0, 0);
}

void updateWhite() {
    //Only update whole strip when a setting is changed or if we just changed to this mode / exited the settings
    if(redrawLEDs) {
        fillPixelsHSV(0, 0, settingBrightness * WHITE_DIM, 0, 0);
        redrawLEDs = false;
    }

    if(settingMicrophone == 1) {
        //Gradually reset pixels to original color (only execute if microphone is activated)
        for(int i = 0; i < LED_COUNT; i++) {
            graduallyResetColors(getPixelHSV(i).hue, 0, settingBrightness * WHITE_DIM, i, 1);
        }

        //make a random pixel light up
        if(microphone.isActivated()) {
            setPixelHSV(random(13), random(UINT16_MAX), 255, settingBrightness * LED_BLINK_BRIGHTNESS_FACTOR);
        } 
    }
}

const int indicateSettingBlinkFrames = 10;
int indicateSettingFrameCounter = 0;
void indicateSetting() {
    if(currentSetting != 0) {
        //Indicate current setting by turning off one LED per setting (sans the 0th Setting (Default setting / quick setting))
        if(settings[currentMode][currentSetting].value == &settingMicrophone) {
            //In order to display the microphone setting turn all LEDs off
            //If the center is green the microphone is on, if it is red the microphone is off
            fillPixelsHSV(0,0,0,0,0);
            if(settingMicrophone == 1) {
                strip.setPixelColor(0, strip.Color(0,settingBrightness,0));
            } else if (settingMicrophone == 0) {
                strip.setPixelColor(0, strip.Color(settingBrightness,0,0));
            }
        } else if(settings[currentMode][currentSetting].value == &settingMicrophoneThreshhold) {
            microphone.setThreshhold(settingMicrophoneThreshhold);
            //In order to display the microphone threshhold setting turn all LEDs off
            //If the center LED lights up, the microphone detects input
            fillPixelsHSV(0,0,0,0,0);
            if(microphone.isActivated()) {
                setPixelHSV(0, 0, 0,settingBrightness);
            }
        } else {
            //for all the other settings just turn off enough LEDs to indicate the current setting
            fillPixelsHSV(0,0,0, 1, currentModeNumSettings - 1);
        }
        
        ++indicateSettingFrameCounter %= indicateSettingBlinkFrames * 2;
        if(indicateSettingFrameCounter < indicateSettingBlinkFrames) {
            setPixelHSV(currentSetting, 0,0,settingBrightness);
        }
    }
}

void updateSetting() {
    Setting s = settings[currentMode][currentSetting];
    Serial.print("Mode: ");
    Serial.print(currentMode);
    Serial.print(" Setting: ");
    Serial.print(currentSetting);
    Serial.print(" Max: ");
    Serial.print(s.max);
    Serial.print(" Value: ");
    Serial.print(*s.value);

    uint32_t currentValue = *s.value;
    if(rotary.getRotation() > 0) {
        Serial.print(" + ");
        currentValue += s.step;
        if(currentValue > s.max) {
            if(s.rollover) {
              currentValue -= (s.max - s.min) + 1;
            } else {
              currentValue = s.max;
            }
        }
    } else if (rotary.getRotation() < 0) {
        Serial.print(" - ");
        if(currentValue < s.min + s.step) {
            if(s.rollover) {
                currentValue = s.max - (s.step - 1 - (currentValue-s.min));
            } else {
                currentValue = s.min;
            }
        } else {
            currentValue -= s.step;
        }
    }

    *s.value = currentValue;
    EEPROMUtils.writeUInt16(s.eepromAddress, currentValue);
    redrawLEDs = true;

    Serial.print(" -> ");
    Serial.println(*s.value);
}

//--HELPER-FUNCTIONS--
void setPixelHSV(uint16_t index, uint16_t hue, uint16_t saturation, uint16_t value) {
    LED_HSV_COLORS[index].hue = hue;
    LED_HSV_COLORS[index].saturation = saturation;
    LED_HSV_COLORS[index].value = value;
    
    strip.setPixelColor(index, strip.ColorHSV(hue, saturation, value));
}

void setPixelHSV(uint16_t index, hsvColor color) {
    setPixelHSV(index, color.hue, color.saturation, color.value);
}

void fillPixelsHSV(uint16_t hue, uint16_t saturation, uint16_t value, uint16_t start, uint16_t count) {
    uint16_t c = count;
    if(c == 0) {
        c = LED_COUNT - start;
    }

    for(int i = start; i < start + c; i++) {
        setPixelHSV(i, hue, saturation, value);
    }
}

void fillPixelsHSV(hsvColor color, uint16_t start, uint16_t count) {
    fillPixelsHSV(color.hue, color.saturation, color.value, start, count);
}

hsvColor getPixelHSV(uint16_t index) {
    return LED_HSV_COLORS[index];
}
