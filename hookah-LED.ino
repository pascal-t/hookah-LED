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
#include "Setting.h"

#define LED 9
#define LED_COUNT 21

#define LED_OFFSET_CENTER 0
#define LED_COUNT_CENTER 3
#define LED_OFFSET_INNER 3
#define LED_COUNT_INNER 6
#define LED_OFFSET_OUTER 9
#define LED_COUNT_OUTER 12
 
#define UPDATE_DELAY 20 //50 Updates/Second
#define SMOOTHING 300

unsigned long lastUpdate = 0;
unsigned long lastSettingUpdate = 0;

#define SETTING_DURATION 500

const double LED_BLINK_BRIGHTNESS_FACTOR = 2.0;
const double LED_BLINK_RESET_FACTOR = 0.98;
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

hsvColor LED_HSV_COLORS[LED_COUNT];
void setPixelHSVAuto(uint16_t index, uint16_t hue, uint8_t saturation=255);
void fillPixelsHSVAuto(uint16_t hue, uint8_t saturation=255, uint16_t start=0, uint16_t count=0);

#define SETTING_BRIGHTNESS_ADDRESS                 0
#define SETTING_COLOR_HUE_ADDRESS                  2
#define SETTING_COLOR_SECONDARY_SATURATION_ADDRESS 4
#define SETTING_RAINBOW_STEP_ADDRESS               6
#define SETTING_MICROPHONE_ADDRESS                 8
#define MODE_ADDRESS                              10
#define SETTING_MICROPHONE_THRESHHOLD_ADDRESS     12
#define SETTING_CONCENTRIC_RANGE_ADDRESS          16
#define SETTING_CONCENTRIC_STEP_ADDRESS           18
#define SETTING_COLOR_SECONDARY_HUE_ADDRESS       20
#define SETTING_CENTER_BRIGHTNESS                 22
#define SETTING_INNER_BRIGHTNESS                  24
#define SETTING_OUTER_BRIGHTNESS                  26

Setting settingColorHue(SETTING_COLOR_HUE_ADDRESS, 59, 0, 1092, true);
Setting settingColorSecondaryHue(SETTING_COLOR_SECONDARY_HUE_ADDRESS, 59, 0, 1092, true);
Setting settingColorSecondarySaturation(SETTING_COLOR_SECONDARY_SATURATION_ADDRESS, 16, 0, 16, false, true);

Setting settingRainbowStep(SETTING_RAINBOW_STEP_ADDRESS, 3, -3, 256);

Setting settingConcentricRange(SETTING_CONCENTRIC_RANGE_ADDRESS, 3, -3, 8192);
Setting settingConcentricStep(SETTING_CONCENTRIC_STEP_ADDRESS, 3, -3, 256);

Setting settingDummy(-1, 0);
Setting settingCenterBrightness(SETTING_CENTER_BRIGHTNESS, 16, 1, 16, false, true);
Setting settingInnerBrightness(SETTING_INNER_BRIGHTNESS, 16, 1, 16, false, true);
Setting settingOuterBrightness(SETTING_OUTER_BRIGHTNESS, 16, 1, 16, false, true);
Setting settingBrightness(SETTING_BRIGHTNESS_ADDRESS, 8, 1, 16, false, true);
Setting settingMicrophone(SETTING_MICROPHONE_ADDRESS, 1);
Setting settingMicrophoneThreshhold(SETTING_MICROPHONE_THRESHHOLD_ADDRESS, 1023);

#define MODE_COLOR      0
#define MODE_WHITE      1
#define MODE_RAINBOW    2
#define MODE_CONCENTRIC 3
#define MODE_SETTINGS   4

#define MODE_COLOR_NUM_SETTINGS      3
#define MODE_RAINBOW_NUM_SETTINGS    1
#define MODE_WHITE_NUM_SETTINGS      1
#define MODE_CONCENTRIC_NUM_SETTINGS 2
#define MODE_SETTINGS_NUM_SETTINGS   6

Setting* settings[5][4] = {
    { //MODE_COLOR
        &settingColorHue, &settingColorSecondaryHue, &settingColorSecondarySaturation
    }, { //MODE_WHITE
        &settingDummy
    }, { //MODE_RAINBOW 
        &settingRainbowStep
    }, { //MODE_CONCENTRIC
        &settingConcentricRange, &settingConcentricStep
    }, { //MODE_SETTINGS
        &settingDummy, &settingCenterBrightness, &settingInnerBrightness, &settingOuterBrightness, &settingMicrophone, &settingMicrophoneThreshhold
    }
};

hsvColor getHsvWhite() {
    hsvColor white;
    white.hue = 0;
    white.saturation = 0;
    white.value = settingBrightness.get() * WHITE_DIM;
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

    Serial.println("Loaded settings from EEPROM:");
    setMode(EEPROMUtils.readUInt16(MODE_ADDRESS));

    Serial.print("settingBrightness: ");
    Serial.println(settingBrightness.get());

    Serial.print("settingColorHue: ");
    Serial.println(settingColorHue.get());
    Serial.print("settingColorSecondaryHue: ");
    Serial.println(settingColorSecondaryHue.get());
    Serial.print("settingColorSecondarySaturation: ");
    Serial.println(settingColorSecondarySaturation.get());
    
    Serial.print("settingRainbowStep: ");
    Serial.println(settingRainbowStep.get());

    Serial.print("settingConcentricRange: ");
    Serial.println(settingConcentricRange.get());
    Serial.print("settingConcentricStep: ");
    Serial.println(settingConcentricStep.get());
    
    Serial.print("settingMicrophone: ");
    Serial.println(settingMicrophone.get());
    Serial.print("settingMicrophoneThreshhold: ");
    Serial.println(settingMicrophoneThreshhold.get());
    microphone.setThreshhold(settingMicrophoneThreshhold.get());
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
            lastSettingUpdate = millis();
            Serial.print("Mode: ");
            Serial.print(currentMode);
            Serial.print(" Setting: ");
            Serial.print(currentSetting);
            Serial.print(" ");
            if (rotary.getRotation() == 1) {
                Serial.println("increase");
                getCurrentSetting()->increase();
            } else {
                Serial.println("decrease");
                getCurrentSetting()->decrease();
            }
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

bool displaySettingUpdate() {
    return millis() - lastSettingUpdate < SETTING_DURATION;
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
    if(getCurrentSetting() == &settingMicrophoneThreshhold && settingMicrophone.get() == 0) {
        Serial.println("Microphone deactivated. Skipping threshhold setting.");
        cycleSettings();
    }
}

Setting* getCurrentSetting() {
    return settings[currentMode][currentSetting];
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
        if(pixelColor.hue != hue
        || pixelColor.saturation != saturation
        || pixelColor.value      != value) {

            uint16_t newHue = pixelColor.hue;
            if(pixelColor.hue > hue) {
                if(pixelColor.hue - hue > UINT16_MAX/2) {
                    newHue = hue - (UINT16_MAX - (pixelColor.hue - hue)) * LED_BLINK_RESET_FACTOR;
                } else {
                    newHue = hue + (pixelColor.hue - hue) * LED_BLINK_RESET_FACTOR;
                }
            } else {
                if(hue - pixelColor.hue > UINT16_MAX/2) {
                    newHue = hue + (UINT16_MAX - (hue - pixelColor.hue)) * LED_BLINK_BRIGHTNESS_FACTOR;
                } else {
                    newHue = hue - (hue - pixelColor.hue) * LED_BLINK_RESET_FACTOR;
                }
            }

            //there is no use case for resetting the hue of a color, so I will spare myself the unreadable algorithm  
            newColor.hue = newHue;
            newColor.saturation = saturation + (pixelColor.saturation - saturation) * LED_BLINK_RESET_FACTOR;
            newColor.value = value + (pixelColor.value - value) * LED_BLINK_RESET_FACTOR;

            setPixelHSV(i, newColor);
        }
    }
}

void updateColor() {
    uint16_t hue = settingColorHue.get();
    uint16_t secondaryHue = settingColorSecondaryHue.get();
    uint8_t secondarySaturation = settingColorSecondarySaturation.get();
    uint8_t value = settingBrightness.get();

    //Only update whole strip when a setting is changed or if we just changed to this mode / exited the settings
    if(redrawLEDs) {
        fillPixelsHSVAuto(hue)
        redrawLEDs = false;
    }

    if(getCurrentSetting() == &settingColorHue) {
        if(displaySettingUpdate()) {
            fillPixelsHSV(getHsvBlack(), LED_OFFSET_OUTER, LED_COUNT_OUTER);
            setPixelHSVAuto(LED_OFFSET_OUTER + settingColorHue.getValue() / 5, 0, 0);
            redrawLEDs = true;
            return;
        }
    } else if(getCurrentSetting() == &settingColorSecondaryHue) {
        fillPixelsHSV(secondaryHue, secondarySaturation, 255, LED_OFFSET_INNER, LED_COUNT_INNER);
        redrawLEDs = true;
        return;
    } else if(getCurrentSetting() == &settingColorSecondarySaturation) {
        fillPixelsHSV(secondaryHue, secondarySaturation, 255, 0, LED_COUNT_CENTER + LED_COUNT_INNER);
        for(int i = 0; i < LED_COUNT_OUTER; i++) {
            setPixelHSVAuto(LED_OFFSET_OUTER + i, secondaryHue, i * (255/(LED_COUNT_OUTER-1)));
            setPixelHSVAuto(LED_OFFSET_OUTER + LED_COUNT_OUTER - i - 1, secondaryHue, i * (255/5));
        }
        redrawLEDs = true;
        return;
    }
    
    if(settingMicrophone.get() == 1) {
        //Gradually reset pixels to original color (only execute if microphone is activated)
        graduallyResetColors(hue, 255, value);

        //make a random pixel light up
        if(microphone.isActivated(UPDATE_DELAY)) {
            setPixelHSVAuto(random(LED_COUNT), secondaryHue, secondarySaturation);
        } 
    }
}

uint16_t rainbowCurrentPosition = 0;
void updateRainbow() {
    //increase the hue of the rainbow. When thw varable overflows it starts over, wich is actually convenient
    if(settingMicrophone.get() == 1 && microphone.isActivated(SMOOTHING)) {
        //turn another stepwidth if microphon was listening in the last 500ms (turn twice as fast)
        rainbowCurrentPosition += settingRainbowStep.get() * RAINBOW_ACCELERATION_FACTOR;
    } else {
        rainbowCurrentPosition += settingRainbowStep.get();
    }

    //Pixels are positioned as follows: 
    //  the first 3 are in the center
    //  the next 6 are halfway from the center
    //  the last 12 are furthest from the center
    for(int i = LED_OFFSET_CENTER; i < LED_COUNT_CENTER; i++) {
        setPixelHSVAuto(LED_OFFSET_CENTER + i, (UINT16_MAX/LED_COUNT_CENTER) * i + rainbowCurrentPosition);
    }

    for(int i = 0; i < LED_COUNT_INNER; i++) {
        setPixelHSVAuto(LED_OFFSET_INNER + i, (UINT16_MAX/LED_COUNT_INNER) * i + rainbowCurrentPosition);
    }
    
    for(int i = 0; i < LED_COUNT_OUTER; i++) {
        //Pixels furthest from the center are displayed at max saturation while the others are at half saturation
        setPixelHSVAuto(LED_OFFSET_OUTER + i, (UINT16_MAX/LED_COUNT_OUTER) * i + rainbowCurrentPosition);
    }

    if(displaySettingUpdate()) {
        int settingValue = settingRainbowStep.getValue();
        fillPixelsHSV(getHsvBlack(), LED_OFFSET_OUTER, 4);
        fillPixelsHSV(getHsvBlack(), LED_OFFSET_OUTER + LED_COUNT_OUTER - 3, 3);
        setPixelHSVAuto(LED_OFFSET_OUTER, 0, 0);
        if(settingValue > 0) {
            setPixelHSVAuto(LED_OFFSET_OUTER + LED_COUNT_OUTER - settingValue, 0, 0);
        } else {
            setPixelHSVAuto(LED_OFFSET_OUTER - settingValue, 0, 0);
        }
    }
}

void updateWhite() {
    //Only update whole strip when a setting is changed or if we just changed to this mode / exited the settings
    if(redrawLEDs) {
        fillPixelsHSVAuto(0, 0); //hue, saturation
        redrawLEDs = false;
    }

    if(settingMicrophone.get() == 1) {
        //Gradually reset pixels to original color (only execute if microphone is activated)
        for(int i = 0; i < LED_COUNT; i++) {
            graduallyResetColors(getPixelHSV(i).hue, 0, settingBrightness.get() * WHITE_DIM, i, 1);
        }

        //make a random pixel light up
        if(microphone.isActivated(UPDATE_DELAY)) {
            setPixelHSVAuto(random(LED_COUNT), random(UINT16_MAX));
        } 
    }
}

uint16_t concentricHue = 0;
void updateConcentric() {
    //hopefully convert it to signed int16
    int16_t range = settingConcentricRange.get();

    if(getCurrentSetting() == &settingConcentricStep) {
        if(button.isShortPress()) {
            animation.start(3);
        }

        if (animation.isRunning()) {
            switch (animation.getFrame()) {
                case 0:
                    fillPixelsHSVAuto(0, 0, LED_OFFSET_CENTER, LED_COUNT_CENTER);
                    fillPixelsHSV(getHsvBlack(), LED_OFFSET_INNER, LED_COUNT_INNER);
                    fillPixelsHSV(getHsvBlack(), LED_OFFSET_OUTER, LED_COUNT_OUTER);
                    break;
                
                case 1:
                    fillPixelsHSV(getHsvBlack(), LED_OFFSET_CENTER, LED_COUNT_CENTER);
                    fillPixelsHSVAuto(0, 0, LED_OFFSET_INNER, LED_COUNT_INNER);
                    fillPixelsHSV(getHsvBlack(), LED_OFFSET_OUTER, LED_COUNT_OUTER);
                    break;
                
                case 2:
                    fillPixelsHSV(getHsvBlack(), LED_OFFSET_CENTER, LED_COUNT_CENTER);
                    fillPixelsHSV(getHsvBlack(), LED_OFFSET_INNER, LED_COUNT_INNER);
                    fillPixelsHSVAuto(0, 0, LED_OFFSET_OUTER, LED_COUNT_OUTER);
                    break;
                
                default:
                    break;
            }
            return;
        }
    }

    if((settingMicrophone.get() == 1 && microphone.isActivated(SMOOTHING)) 
        || getCurrentSetting()== &settingConcentricStep) {
        concentricHue += settingConcentricStep.get();
    }

    fillPixelsHSVAuto(concentricHue,         255, LED_OFFSET_CENTER, LED_COUNT_CENTER);
    fillPixelsHSV(concentricHue + (range/2), 255, LED_OFFSET_INNER,  LED_COUNT_INNER);
    fillPixelsHSV(concentricHue + range,     255, LED_OFFSET_OUTER,  LED_COUNT_OUTER);

    if(getCurrentSetting() == &settingConcentricRange) {
        if(displaySettingUpdate()) {
            if(displaySettingUpdate()) {
                int settingValue = settingConcentricRange.getValue();
                fillPixelsHSV(getHsvBlack(), LED_OFFSET_OUTER, 4);
                fillPixelsHSV(getHsvBlack(), LED_OFFSET_OUTER + LED_COUNT_OUTER - 3, 3);
                setPixelHSVAuto(LED_OFFSET_OUTER, 0, 0);
                if(settingValue > 0) {
                    setPixelHSVAuto(LED_OFFSET_OUTER + LED_COUNT_OUTER - settingValue, 0, 0);
                } else {
                    setPixelHSVAuto(LED_OFFSET_OUTER - settingValue, 0, 0);
                }
            }
        }
    } else if(getCurrentSetting() == &settingConcentricStep) {
        if(displaySettingUpdate()) {
            if(displaySettingUpdate()) {
                int settingValue = settingConcentricStep.getValue();
                fillPixelsHSV(getHsvBlack(), LED_OFFSET_OUTER, 4);
                fillPixelsHSV(getHsvBlack(), LED_OFFSET_OUTER + LED_COUNT_OUTER - 3, 3);
                setPixelHSVAuto(LED_OFFSET_OUTER, 0, 0);
                if(settingValue > 0) {
                    setPixelHSVAuto(LED_OFFSET_OUTER + LED_COUNT_OUTER - settingValue, 0, 0);
                } else {
                    setPixelHSVAuto(LED_OFFSET_OUTER - settingValue, 0, 0);
                }
            }
        }
    }
}

void drawSettings() {
    if(getCurrentSetting() == &settingDummy) {
        //Draw a cogwheel to indicate settings
        fillPixelsHSV(getHsvBlack(), LED_OFFSET_CENTER, LED_COUNT_CENTER);
        fillPixelsHSV(getHsvWhite(), LED_OFFSET_INNER,  LED_COUNT_INNER);

        for(int i = LED_OFFSET_OUTER; i < LED_OFFSET_OUTER + LED_COUNT_OUTER; i++) {
            if(i%2 == 0) {
                setPixelHSVAuto(i, 0, 0);
            } else {
                setPixelHSV(i, getHsvBlack());
            }
        }
        return;
    } else if (getCurrentSetting() == &settingBrightness) {
        if(button.isShortPress()) {
            animation.start(3);
        } 
        if(animation.isRunning()) {
            switch(animation.getFrame()) {
                case 0:
                    fillPixelsHSV(getHsvWhite(), LED_OFFSET_CENTER, LED_COUNT_CENTER);
                    fillPixelsHSV(getHsvBlack(), LED_OFFSET_INNER, 0);
                    break;

                case 1:
                    fillPixelsHSV(getHsvWhite(), LED_OFFSET_CENTER, LED_COUNT_CENTER + LED_COUNT_INNER);
                    fillPixelsHSV(getHsvBlack(), LED_OFFSET_OUTER, LED_COUNT_OUTER);
                    break;

                case 2:
                    fillPixelsHSV(getHsvWhite(), LED_OFFSET_CENTER, LED_COUNT_CENTER + LED_COUNT_INNER);
                    for(int i = 0; i < LED_COUNT_OUTER; i++) {
                        if(i%2 == 0) {
                            setPixelHSVAuto(LED_OFFSET_OUTER + i, 0, 0);
                        } else {
                            setPixelHSV(LED_OFFSET_OUTER + i, getHsvBlack());
                        }
                    }
                    break;
            }
            return;
        } else {
            fillPixelsHSV(getHsvWhite(), 0, 0);
        }
    } else if (getCurrentSetting() == &settingMicrophone) {
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
                    setPixelHSVAuto(LED_OFFSET_OUTER + 8,  0, 0);
                    setPixelHSVAuto(LED_OFFSET_OUTER + 10, 0, 0);
                    break;
            }
            return;
        }

        //Draw a cup and let it blink if the setting is off
        if (settingMicrophone.get() == 1) {
            fillPixelsHSV(getHsvWhite(), LED_OFFSET_CENTER, 2);
            fillPixelsHSV(getHsvWhite(), LED_OFFSET_INNER,  4);
        } else {
            if (animation.getFrame()%2 == 0) {
                fillPixelsHSV(getHsvWhite(), LED_OFFSET_CENTER, 2);
                fillPixelsHSV(getHsvWhite(), LED_OFFSET_INNER,  4);
            }
        }

        //Draw noise lines
        setPixelHSVAuto(LED_OFFSET_OUTER + 8, 0, 0);
        setPixelHSVAuto(LED_OFFSET_OUTER + 10, 0, 0);
    } else if (getCurrentSetting() == &settingMicrophoneThreshhold) {
        if (button.isShortPress()) {
            animation.start(4);
        }

        //skip if microphone is off
        if (settingMicrophone.get() == 0) {
            cycleSettings();
        }

        
        //fill everything black
        fillPixelsHSV(getHsvBlack(), 0, 0);
        if (animation.isRunning()) {
            switch (animation.getFrame()) {
                case 0:
                case 2:
                    //draw noise lines
                    setPixelHSVAuto(LED_OFFSET_OUTER + 8,  0, 0);
                    setPixelHSVAuto(LED_OFFSET_OUTER + 10, 0, 0);
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

        //If the setting has changed, update it in the rotary instance
        if (rotary.getRotation() != 0) {
            microphone.setThreshhold(settingMicrophoneThreshhold.get());
        }

        //Fill everything back to begin with
        fillPixelsHSV(getHsvBlack(), 0, 0);
        
        //Draw cup
        fillPixelsHSV(getHsvWhite(), LED_OFFSET_CENTER, 2);
        fillPixelsHSV(getHsvWhite(), LED_OFFSET_INNER,  4);

        //Draw noise lines when microphone is activated
        if (microphone.isActivated(UPDATE_DELAY)) {
            setPixelHSVAuto(LED_OFFSET_OUTER + 8,  0, 0);
            setPixelHSVAuto(LED_OFFSET_OUTER + 10, 0, 0);
        }
    }
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

void setBlack(uint16_t index) {
    setPixelHSV(index, 0, 0, 0);
}

void setWhite(uint16_t index) {
    if(index >= LED_OFFSET_OUTER) {
        setPixelHSV(index, 0, 0, settingOuterBrightness.get() * WHITE_DIM);
    } else if(index >= LED_OFFSET_INNER) {
        setPixelHSV(index, 0, 0, settingInnerBrightness.get() * WHITE_DIM);
    } else if(index >= LED_OFFSET_OUTER) {
        setPixelHSV(index, 0, 0, settingCenterBrightness.get() * WHITE_DIM);
    }
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

/**
 * Set brigtness/value automatically
 */
void setPixelHSVAuto(uint16_t index, uint16_t hue, uint8_t saturation) {
    if(index >= LED_OFFSET_OUTER) {
        setPixelHSV(index, hue, saturation, settingOuterBrightness.get());
    } else if(index >= LED_OFFSET_INNER) {
        setPixelHSV(index, hue, saturation, settingInnerBrightness.get());
    } else if(index >= LED_OFFSET_OUTER) {
        setPixelHSV(index, hue, saturation, settingCenterBrightness.get());
    }
}

void fillPixelsHSVAuto(uint16_t hue, uint8_t saturation, uint16_t start, uint16_t count) {
    uint16_t c = count;
    if(c == 0) {
        c = LED_COUNT - start;
    }

    for(int i = start; i < start + c; i++) {
        setPixelHSVAuto(i, hue, saturation);
    }
}

hsvColor getPixelHSV(uint16_t index) {
    return LED_HSV_COLORS[index];
}
