#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
 #include <avr/power.h> // Required for 16 MHz Adafruit Trinket
#endif

#include "Button.h"
#include "RotaryEncoder.h"
#include "Microphone.h"
#include "EEPROMUtils.h"
#include "Animation.h"

#define LED 9
#define LED_COUNT 21

#define LED_OFFSET_CENTER 0
#define LED_COUNT_CENTER 3
#define LED_OFFSET_INNER 3
#define LED_COUNT_INNER 6
#define LED_OFFSET_OUTER 9
#define LED_COUNT_OUTER 12
 
#define UPDATE_DELAY 20 //50 Updates/Second
unsigned long lastUpdate = 0;

const double LED_BLINK_BRIGHTNESS_FACTOR = 2.0;
const double LED_BLINK_RESET_FACTOR = 0.95;
const double RAINBOW_ACCELERATION_FACTOR = 4;
const double WHITE_DIM = 0.66; //Dim the LEDs in white mode, since it is the only time all Colors of one LED are lit up

Button button(10, 500, INPUT_PULLUP);
RotaryEncoder rotary(14,16);
Microphone microphone(A0);
Animation animation(UPDATE_DELAY * 10);

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

uint16_t settingDummy = 0;
#define SETTING_DUMMY_ADDRESS -1

uint16_t settingBrightness = 0;
#define SETTING_BRIGHTNESS_ADDRESS 0
uint16_t settingSaturation = 0;
#define SETTING_SATURATION_ADDRESS 4

uint16_t settingColorHue = 0;
#define SETTING_COLOR_HUE_ADDRESS 2

uint16_t settingRainbowStep = 0;
#define SETTING_RAINBOW_STEP_ADDRESS 6

uint16_t settingConcentricRange = 0;
#define SETTING_CONCENTRIC_RANGE_ADDRESS 16

uint16_t settingConcentricStep = 0;
#define SETTING_CONCENTRIC_STEP_ADDRESS 18

uint16_t settingMicrophone = 0;
#define SETTING_MICROPHONE_ADDRESS 8

#define MODE_ADDRESS 10

uint16_t settingMicrophoneThreshhold = 0;
#define SETTING_MICROPHONE_THRESHHOLD_ADDRESS 12

#define MODE_COLOR      0
#define MODE_WHITE      1
#define MODE_RAINBOW    2
#define MODE_CONCENTRIC 3
#define MODE_SETTINGS   4

#define MODE_COLOR_NUM_SETTINGS      2
#define MODE_RAINBOW_NUM_SETTINGS    1
#define MODE_WHITE_NUM_SETTINGS      1
#define MODE_CONCENTRIC_NUM_SETTINGS 2
#define MODE_SETTINGS_NUM_SETTINGS   4

Setting settings[5][4] = {
    { //MODE_COLOR
        {SETTING_COLOR_HUE_ADDRESS,             &settingColorHue,             0,   UINT16_MAX, 1024, true }, //hue, 64 steps for full rotation (little more than 3 turns)
        {SETTING_SATURATION_ADDRESS,            &settingSaturation,           65,  255,        16,   false}, //Saturation, 16 Steps
    }, { //MODE_WHITE
        {SETTING_DUMMY_ADDRESS,                 &settingDummy,                0,   0,    0, false}, //Dummy Setting because we don't want to change anything by default
    }, { //MODE_RAINBOW 
        {SETTING_RAINBOW_STEP_ADDRESS,          &settingRainbowStep,          0,   UINT16_MAX, 8, true }, //Rotation step, rolls over so it can rotate backwards
    }, { //MODE_CONCENTRIC
        {SETTING_CONCENTRIC_RANGE_ADDRESS,      &settingConcentricRange,      0,   UINT16_MAX, 1024, true }, //Concentric Range, the hue range displayed at any given time
        {SETTING_CONCENTRIC_STEP_ADDRESS,       &settingConcentricStep,       0,   UINT16_MAX, 8,    true }, //Concentric Step, how far the hue moves while smoking
    }, { //MODE_SETTINGS
        {SETTING_DUMMY_ADDRESS,                 &settingDummy,                0,   0,    0, false}, //Dummy Setting because we don't want to change anything by default
        {SETTING_BRIGHTNESS_ADDRESS,            &settingBrightness,           31,  127,  8, false}, //Brightness, 16 Steps is stored at the same address across modes
        {SETTING_MICROPHONE_ADDRESS,            &settingMicrophone,           0,   1,    1, false}, //Microphone is stored at the same address across modes
        {SETTING_MICROPHONE_THRESHHOLD_ADDRESS, &settingMicrophoneThreshhold, 0,   1023, 1, false}  //Microphone threshhold is stored at the same address across modes
    }
};

hsvColor getHsvWhite() {
    hsvColor white;
    white.hue = 0;
    white.saturation = 0;
    white.value = settingBrightness * WHITE_DIM;
    return white;
};

hsvColor getHsvBlack() {
    hsvColor black;
    black.hue = 0;
    black.saturation = 0;
    black.value = 0;
    return black;
};

int currentMode = MODE_COLOR;
int currentModeNumSettings = MODE_COLOR_NUM_SETTINGS;
int currentSetting = 0;

void setup() {
    Serial.begin(9600);

    //Setup LED Strip
#if defined(__AVR_ATtiny85__) && (F_CPU == 16000000)
    clock_prescale_set(clock_div_1);
#endif

    strip.begin();           
    strip.show();

    //Load Settings from EEPROM
    settingBrightness = EEPROMUtils.readUInt16(SETTING_BRIGHTNESS_ADDRESS);
    settingSaturation = EEPROMUtils.readUInt16(SETTING_SATURATION_ADDRESS);

    settingColorHue = EEPROMUtils.readUInt16(SETTING_COLOR_HUE_ADDRESS);

    settingRainbowStep = EEPROMUtils.readUInt16(SETTING_RAINBOW_STEP_ADDRESS);

    settingConcentricRange = EEPROMUtils.readUInt16(SETTING_CONCENTRIC_RANGE_ADDRESS);
    settingConcentricStep = EEPROMUtils.readUInt16(SETTING_CONCENTRIC_STEP_ADDRESS);

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

    Serial.print("settingConcentricRange: ");
    Serial.println(settingConcentricRange);
    Serial.print("settingConcentricStep: ");
    Serial.println(settingConcentricStep);
    
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

bool forceUpdateLEDs = false;
void loop() {
    readInputs();

    if (!animation.isRunning()) {
        if (button.isShortPress()) {
            Serial.println("Changing setting");
            cycleSettings();
            forceUpdateLEDs = true;
            redrawLEDs = true;
        }
        if (button.isLongPress()) {
            Serial.println("Changing mode");
            cycleMode();
            forceUpdateLEDs = true;
            redrawLEDs = true;
        }

        if (rotary.getRotation() != 0) {
            Serial.print("updateSetting() ");
            Serial.println(rotary.getRotation());
            updateSetting();
            redrawLEDs = true;
        }
    }

    //Update the LED Strip at a limited rate
    if(millis() - lastUpdate > UPDATE_DELAY || forceUpdateLEDs) {
        lastUpdate = millis();
        updateLEDs();
        forceUpdateLEDs = false;
    }
}

void cycleMode() {
    setMode(currentMode+1);
}

void setMode(int mode) {
    switch(mode) {
        case MODE_RAINBOW:
            currentMode = MODE_RAINBOW;
            currentModeNumSettings = MODE_RAINBOW_NUM_SETTINGS;
            break;
        case MODE_WHITE:
            currentMode = MODE_WHITE;
            currentModeNumSettings = MODE_WHITE_NUM_SETTINGS;
            break;
        case MODE_CONCENTRIC:
            currentMode = MODE_CONCENTRIC;
            currentModeNumSettings = MODE_CONCENTRIC_NUM_SETTINGS;
            break;
        case MODE_SETTINGS:
            currentMode = MODE_SETTINGS;
            currentModeNumSettings = MODE_SETTINGS_NUM_SETTINGS;
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
}


void cycleSettings() {
    ++currentSetting %= currentModeNumSettings;
    Serial.print("Current mode: ");
    Serial.println(currentMode);
    Serial.print("Switched to setting ");
    Serial.println(currentSetting);
    if(settings[currentMode][currentSetting].value == &settingMicrophoneThreshhold && settingMicrophone == 0) {
        Serial.println("Microphone deactivated. Skipping threshhold setting.");
        cycleSettings();
    }
}

Setting* getCurrentSetting() {
    return &settings[currentMode][currentSetting];
}

void updateLEDs() {
    switch(currentMode) {
        case MODE_COLOR:
            updateColor();
            break;
        case MODE_RAINBOW:
            updateRainbow();
            break;
        case MODE_WHITE:
            updateWhite();
            break;
        case MODE_CONCENTRIC:
            updateConcentric();
            break;
        case MODE_SETTINGS:
            drawSettings();
            break;
    }
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

    if(getCurrentSetting()->value == &settingSaturation) {
        if(button.isShortPress()) {
            animation.start(3);
        }

        if(animation.isRunning()) {
            //Based on calculations
            setPixelHSV(LED_OFFSET_OUTER  + 9,  settingColorHue, 255, settingBrightness);

            setPixelHSV(LED_OFFSET_OUTER + 8,   settingColorHue, 238, settingBrightness);
            setPixelHSV(LED_OFFSET_OUTER + 10,  settingColorHue, 238, settingBrightness);

            setPixelHSV(LED_OFFSET_OUTER + 7,   settingColorHue, 191, settingBrightness);
            setPixelHSV(LED_OFFSET_OUTER + 11,  settingColorHue, 191, settingBrightness);
            setPixelHSV(LED_OFFSET_INNER + 4,   settingColorHue, 191, settingBrightness);
            setPixelHSV(LED_OFFSET_INNER + 5,   settingColorHue, 191, settingBrightness);

            setPixelHSV(LED_OFFSET_CENTER + 2,  settingColorHue, 144, settingBrightness);
            
            setPixelHSV(LED_OFFSET_OUTER  + 0,  settingColorHue, 128, settingBrightness);
            setPixelHSV(LED_OFFSET_OUTER  + 6,  settingColorHue, 128, settingBrightness);
            setPixelHSV(LED_OFFSET_INNER  + 0,  settingColorHue, 128, settingBrightness);
            setPixelHSV(LED_OFFSET_INNER  + 3,  settingColorHue, 128, settingBrightness);
            setPixelHSV(LED_OFFSET_CENTER + 0,  settingColorHue, 128, settingBrightness);

            setPixelHSV(LED_OFFSET_CENTER + 1,  settingColorHue, 111, settingBrightness);

            setPixelHSV(LED_OFFSET_OUTER  + 1,  settingColorHue, 64,  settingBrightness);
            setPixelHSV(LED_OFFSET_OUTER  + 5,  settingColorHue, 64,  settingBrightness);
            setPixelHSV(LED_OFFSET_INNER  + 1,  settingColorHue, 64,  settingBrightness);
            setPixelHSV(LED_OFFSET_INNER  + 2,  settingColorHue, 64,  settingBrightness);

            setPixelHSV(LED_OFFSET_OUTER  + 2,  settingColorHue, 17,  settingBrightness);
            setPixelHSV(LED_OFFSET_OUTER  + 4,  settingColorHue, 17,  settingBrightness);

            setPixelHSV(LED_OFFSET_OUTER  + 3,  settingColorHue, 0,   settingBrightness);
            return;
        }

        if (animation.getFrame()%2 == 0) {
            fillPixelsHSV(getHsvBlack(), LED_OFFSET_CENTER, LED_COUNT_CENTER);
            redrawLEDs = true;
        }
        return;
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
void updateRainbow() {
    //increase the hue of the rainbow. When thw varable overflows it starts over, wich is actually convenient
    if(settingMicrophone == 1 && microphone.isActivated(true)) {
        //turn another stepwidth if microphon was listening in the last 500ms (turn twice as fast)
        rainbowCurrentPosition += settingRainbowStep * RAINBOW_ACCELERATION_FACTOR;
    } else {
        rainbowCurrentPosition += settingRainbowStep;
    }

    //Pixels are positioned as follows: 
    //  the first 3 are in the center
    //  the next 6 are halfway from the center
    //  the last 12 are furthest from the center
    for(int i = LED_OFFSET_CENTER; i < LED_COUNT_CENTER; i++) {
        setPixelHSV(LED_OFFSET_CENTER + i, (UINT16_MAX/LED_COUNT_CENTER) * i + rainbowCurrentPosition, 255, settingBrightness);
    }

    for(int i = 0; i < LED_COUNT_INNER; i++) {
        setPixelHSV(LED_OFFSET_INNER + i, (UINT16_MAX/LED_COUNT_INNER) * i + rainbowCurrentPosition, 255, settingBrightness);
    }
    
    for(int i = 0; i < LED_COUNT_OUTER; i++) {
        //Pixels furthest from the center are displayed at max saturation while the others are at half saturation
        setPixelHSV(LED_OFFSET_OUTER + i, (UINT16_MAX/LED_COUNT_OUTER) * i + rainbowCurrentPosition, 255, settingBrightness);
    }
}

void updateWhite() {
    //Only update whole strip when a setting is changed or if we just changed to this mode / exited the settings
    if(redrawLEDs) {
        fillPixelsHSV(getHsvWhite(), 0, 0);
        redrawLEDs = false;
    }

    if(settingMicrophone == 1) {
        //Gradually reset pixels to original color (only execute if microphone is activated)
        for(int i = 0; i < LED_COUNT; i++) {
            graduallyResetColors(getPixelHSV(i).hue, 0, settingBrightness * WHITE_DIM, i, 1);
        }

        //make a random pixel light up
        if(microphone.isActivated()) {
            setPixelHSV(random(LED_COUNT), random(UINT16_MAX), 255, settingBrightness * LED_BLINK_BRIGHTNESS_FACTOR);
        } 
    }
}

uint16_t concentricHue = 0;
void updateConcentric() {
    //hopefully convert it to signed int16
    int16_t range = settingConcentricRange;

    if (getCurrentSetting()->value == &settingConcentricStep) {
        if(button.isShortPress()) {
            animation.start(6);
        }

        if (animation.isRunning()) {
            int frame = (1 + animation.getFrame()) * (range < 0 ? -1 : 1);
            switch (frame) {
                case 3:
                case 6:
                case -1:
                case -4:
                    fillPixelsHSV(getHsvWhite(), LED_OFFSET_CENTER, LED_COUNT_CENTER);
                    fillPixelsHSV(getHsvBlack(), LED_OFFSET_INNER, LED_COUNT_INNER);
                    fillPixelsHSV(getHsvBlack(), LED_OFFSET_OUTER, LED_COUNT_OUTER);
                    break;
                
                case 2:
                case 5:
                case -2:
                case -5:
                    fillPixelsHSV(getHsvBlack(), LED_OFFSET_CENTER, LED_COUNT_CENTER);
                    fillPixelsHSV(getHsvWhite(), LED_OFFSET_INNER, LED_COUNT_INNER);
                    fillPixelsHSV(getHsvBlack(), LED_OFFSET_OUTER, LED_COUNT_OUTER);
                    break;
                
                case 1:
                case 4:
                case -3:
                case -6:
                    fillPixelsHSV(getHsvBlack(), LED_OFFSET_CENTER, LED_COUNT_CENTER);
                    fillPixelsHSV(getHsvBlack(), LED_OFFSET_INNER, LED_COUNT_INNER);
                    fillPixelsHSV(getHsvWhite(), LED_OFFSET_OUTER, LED_COUNT_OUTER);
                    break;
                
                default:
                    break;
            }
            return;
        }
    }

    if((settingMicrophone == 1 && microphone.isActivated(true)) 
        || settings[currentMode][currentSetting].value == &settingConcentricStep ) {
        concentricHue += settingConcentricStep;
    }

    fillPixelsHSV(concentricHue,             settingSaturation, settingBrightness, 0, 3);
    fillPixelsHSV(concentricHue + (range/2), settingSaturation, settingBrightness, 3, 6);
    fillPixelsHSV(concentricHue + range,     settingSaturation, settingBrightness, 9, 12);
}

void drawSettings() {
    if(getCurrentSetting()->value == &settingDummy) {
        //Draw a cogwheel to indicate settings
        fillPixelsHSV(getHsvBlack(), LED_OFFSET_CENTER, LED_COUNT_CENTER);
        fillPixelsHSV(getHsvWhite(), LED_OFFSET_INNER,  LED_COUNT_INNER);

        for(int i = LED_OFFSET_OUTER; i < LED_OFFSET_OUTER + LED_COUNT_OUTER; i++) {
            setPixelHSV(i, i%2 == 0 ? getHsvWhite() : getHsvBlack());
        }
        return;
    } else if (getCurrentSetting()->value == &settingBrightness) {
        if(button.isShortPress()) {
            animation.start(5);
        } 
        if(animation.isRunning()) {
            switch(animation.getFrame()) {
                case 2:
                    fillPixelsHSV(getHsvWhite(), LED_OFFSET_CENTER, LED_COUNT_CENTER);
                    fillPixelsHSV(getHsvBlack(), LED_OFFSET_INNER, 0);
                    break;

                case 1:
                case 3:
                    fillPixelsHSV(getHsvWhite(), LED_OFFSET_CENTER, LED_COUNT_CENTER + LED_COUNT_INNER);
                    fillPixelsHSV(getHsvBlack(), LED_OFFSET_OUTER, LED_COUNT_OUTER);
                    break;

                case 0:
                case 4:
                    fillPixelsHSV(getHsvWhite(), LED_OFFSET_CENTER, LED_COUNT_CENTER + LED_COUNT_INNER);
                    for(int i = 0; i < LED_COUNT_OUTER; i++) {
                        setPixelHSV(LED_OFFSET_OUTER + i, i%2 == 0 ? getHsvWhite() : getHsvBlack());
                    }
                    break;
            }
            return;
        } else {
            fillPixelsHSV(getHsvWhite(), 0, 0);
        }
    } else if (getCurrentSetting()->value == &settingMicrophone) {
        if (button.isShortPress()) {
            animation.start(4);
        }

        //fill everything black
        fillPixelsHSV(getHsvBlack(), 0, 0);
        if (animation.isRunning()) {
            switch (animation.getFrame()) {
                case 0:
                case 2:
                    //draw speaker cup
                    fillPixelsHSV(getHsvWhite(), LED_OFFSET_CENTER, 2);
                    fillPixelsHSV(getHsvWhite(), LED_OFFSET_INNER,  4);
                    //fall through

                case 1:
                case 3:
                    //draw noise lines
                    setPixelHSV(LED_OFFSET_OUTER + 8,  getHsvWhite());
                    setPixelHSV(LED_OFFSET_OUTER + 10, getHsvWhite());
                    break;
            }
            return;
        }

        //Draw a cup and let it blink if the setting is off
        if (settingMicrophone == 1) {
            fillPixelsHSV(getHsvWhite(), LED_OFFSET_CENTER, 2);
            fillPixelsHSV(getHsvWhite(), LED_OFFSET_INNER,  4);
        } else {
            if (animation.getFrame()%2 == 0) {
                fillPixelsHSV(getHsvWhite(), LED_OFFSET_CENTER, 2);
                fillPixelsHSV(getHsvWhite(), LED_OFFSET_INNER,  4);
            }
        }

        //Draw noise lines
        setPixelHSV(LED_OFFSET_OUTER + 8,  getHsvWhite());
        setPixelHSV(LED_OFFSET_OUTER + 10, getHsvWhite());
    } else if (getCurrentSetting()->value == &settingMicrophoneThreshhold) {
        if (button.isShortPress()) {
            animation.start(4);
        }

        //skip if microphone is off
        if (settingMicrophone == 0) {
            cycleSettings();
        }

        
        //fill everything black
        fillPixelsHSV(getHsvBlack(), 0, 0);
        if (animation.isRunning()) {
            switch (animation.getFrame()) {
                case 0:
                case 2:
                    //draw noise lines
                    setPixelHSV(LED_OFFSET_OUTER + 8,  getHsvWhite());
                    setPixelHSV(LED_OFFSET_OUTER + 10, getHsvWhite());
                    //fall through

                case 1:
                case 3:
                    //draw speaker cup
                    fillPixelsHSV(getHsvWhite(), LED_OFFSET_CENTER, 2);
                    fillPixelsHSV(getHsvWhite(), LED_OFFSET_INNER,  4);
                    break;
            }
            return;
        }

        microphone.setThreshhold(settingMicrophoneThreshhold);

        //Fill everything back to begin with
        fillPixelsHSV(getHsvBlack(), 0, 0);
        
        //Draw cup
        fillPixelsHSV(getHsvWhite(), LED_OFFSET_CENTER, 2);
        fillPixelsHSV(getHsvWhite(), LED_OFFSET_INNER,  4);

        //Draw noise lines when microphone is activated
        if (microphone.isActivated()) {
            setPixelHSV(LED_OFFSET_OUTER + 8,  getHsvWhite());
            setPixelHSV(LED_OFFSET_OUTER + 10, getHsvWhite());
        }
    }
}

void updateSetting() {
    Setting* s = getCurrentSetting();
    Serial.print("Mode: ");
    Serial.print(currentMode);
    Serial.print(" Setting: ");
    Serial.print(currentSetting);
    Serial.print(" Max: ");
    Serial.print(s->max);
    Serial.print(" Value: ");
    Serial.print(*s->value);

    uint32_t currentValue = *s->value;
    if(rotary.getRotation() > 0) {
        Serial.print(" + ");
        currentValue += s->step;
        if(currentValue > s->max) {
            if(s->rollover) {
              currentValue -= (s->max - s->min) + 1;
            } else {
              currentValue = s->max;
            }
        }
    } else if (rotary.getRotation() < 0) {
        Serial.print(" - ");
        if(currentValue < s->min + s->step) {
            if(s->rollover) {
                currentValue = s->max - (s->step - 1 - (currentValue-s->min));
            } else {
                currentValue = s->min;
            }
        } else {
            currentValue -= s->step;
        }
    }
    Serial.print(s->step);

    *s->value = currentValue;
    EEPROMUtils.writeUInt16(s->eepromAddress, currentValue);

    Serial.print(" -> ");
    Serial.println(*s->value);
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

// /**
//  * Set brigtness/value automatically
//  */
// void setPixelHSVAuto(uint16_t index, uint16_t hue, uint8_t saturation) {
    
// }

// void fillPixelsHSVAuto(uint16_t hue, uint8_t saturation, uint16_t start, uint16_t count) {
//     uint16_t c = count;
//     if(c == 0) {
//         c = LED_COUNT - start;
//     }

//     for(int i = start; i < start + c; i++) {
//         setPixelHSVAuto(i, hue, saturation);
//     }
// }

hsvColor getPixelHSV(uint16_t index) {
    return LED_HSV_COLORS[index];
}
