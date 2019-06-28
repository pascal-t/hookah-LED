#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
 #include <avr/power.h> // Required for 16 MHz Adafruit Trinket
#endif

#define LED 6
#define LED_COUNT 13
#define UPDATE_DELAY 20 //50 Updates/Second

#define BUTTON 2
#define LONGPRESS_DURATION 250 //ms
#define ROTARY_A 3
#define ROTARY_B 4
#define MICROPHONE 5

#define DEBOUNCE 50


int buttonState = LOW;
int buttonStatePrevious = LOW;
unsigned long buttonPressedSince = 0;
unsigned long buttonDebounce = 0;
bool buttonShortPress = false;
bool buttonLongPress = false;

int rotaryAState = LOW;
int rotation = 0;

bool microphoneActivated = false;
bool microphoneListening = false;

Adafruit_NeoPixel strip(LED_COUNT, LED, NEO_GRB + NEO_KHZ800);

uint16_t settingColorHue = 0;
#define SETTING_COLOR_HUE_ADDRESS 0
uint16_t settingColorSaturation = 0;
#define SETTING_COLOR_SATURATION_ADDRESS 2
uint16_t settingColorValue = 0;
#define SETTING_COLOR_VALUE_ADDRESS 4

uint16_t settingRainbowStep = 0;
#define SETTING_RAINBOW_STEP_ADDRESS 6
uint16_t settingRainbowBrightness = 0;
#define SETTING_RAINBOW_BRIGHTNESS_ADDRESS 6

uint16_t settingWhiteBrightness = 0;
#define SETTING_WHITE_BRIGHTNESS_ADDRESS 8

uint16_t settingMicrophone = 0;
#define SETTING_MICROPHONE_ADDRESS 10


void setup() {
    //Setup LED Strip
#if defined(__AVR_ATtiny85__) && (F_CPU == 16000000)
    clock_prescale_set(clock_div_1);
#endif

    strip.begin();           
    strip.show();
    strip.setBrightness(50);

    //Setup other inputs
    pinMode(BUTTON, INPUT);
    pinMode(ROTARY_A, INPUT);
    pinMode(ROTARY_B, INPUT);
    pinMode(MICROPHONE, INPUT);

    //Load Settings from EEPROM
    settingColorHue = eepromReadUInt16(SETTING_COLOR_HUE_ADDRESS);
    settingColorSaturation = eepromReadUInt16(SETTING_COLOR_SATURATION_ADDRESS);
    settingColorValue = eepromReadUInt16(SETTING_COLOR_VALUE_ADDRESS);

    settingRainbowStep = eepromReadUInt16(SETTING_RAINBOW_STEP_ADDRESS);
    settingRainbowBrightness = eepromReadUInt16(SETTING_RAINBOW_BRIGHTNESS_ADDRESS);

    settingWhiteBrightness = eepromReadUInt16(SETTING_WHITE_BRIGHTNESS_ADDRESS);

    settingMicrophone = eepromReadUInt16(SETTING_MICROPHONE_ADDRESS);
}

void readInputs() {
    //Button state changed after debounce timer
    if(buttonState != digitalRead(BUTTON) && (millis() - buttonDebounce) > DEBOUNCE ) {

        //Start debounce timer so we won't detect a button state change until it has run out
        buttonDebounce = millis();
        buttonState = digitalRead(BUTTON);

        if(buttonState == HIGH) {
            //Button pressed
            buttonPressedSince = millis();
        } else {
            //Button released 
            if((millis() - buttonPressedSince) >= LONGPRESS_DURATION) {
                buttonLongPress = true;
            } else {
                buttonShortPress = true;
            }
        }
    } else {
        //no button press to report
        buttonShortPress = false;
        buttonLongPress = false;
    }

    /* Rotary encoder
    Explanation: 
        both ROTARY_A and ROTARY_B are at the same state when stationary.
        when rotating clockwise the contact surface touches (and looses contact with) ROTARY_A before ROTARY_B
        so if ROTARY_A changing and ROTARY_B is still in a different state we are rotating clockwise

        Reversing that logic means if we are rotating counterclockwise, 
        ROTARY_B is changing before the contact surface reaches ROTARY_A.
        So by the time we check ROTARY_A, ROTARY_B is already at the same state.
    */    
    if(rotaryAState != digitalRead(ROTARY_A)) {
        rotaryAState = digitalRead(ROTARY_A);

        if(rotaryAState != digitalRead(ROTARY_B)) {
            //Clockwise rotation
            rotation = 1;
        } else {
            //Counterclockwise rotation
            rotation = -1;
        }

    } else {
        //No change; no rotation
        rotation = 0;
    }

    //Microphone
    //Record if the microphone is switched on
    microphoneActivated = !microphoneListening && digitalRead(MICROPHONE) == HIGH;
    //Current state of the microphone
    microphoneListening = digitalRead(MICROPHONE);
}

void loop() {
    readInputs();
    if(buttonLongPress) {
        cycleMode();
    }
    if(buttonShortPress) {
        cycleSettings();
    }
    if(rotation != 0) {
        updateSetting();
    }

    //Update the LED Strip at a limited rate
    if(millis() % UPDATE_DELAY == 0) {
        updateLEDs();
    }
}


struct Setting {
    uint16_t* value;
    uint16_t max;
    uint16_t step;
    bool rollover;
};

#define MODE_COLOR 0
#define MODE_RAINBOW 1
#define MODE_WHITE 2

#define MODE_COLOR_NUM_SETTINGS 4
#define MODE_RAINBOW_NUM_SETTINGS 3
#define MODE_WHITE_NUM_SETTINGS 2

Setting settings[3][4] = {
    { //MODE_COLOR
        {&settingColorHue, UINT16_MAX, 1024, true}, //hue, 64 steps for full rotation (little more than 3 turns)
        {&settingColorSaturation, 255, 16, false}, //Saturation, 16 Steps
        {&settingColorValue, 255, 16, false}, //Value/Brightness, 16 Steps
        {&settingMicrophone, 1, 1, false} //Microphone is stored at the same address across modes
    }, { //MODE_RAINBOW 
        {&settingRainbowStep, UINT16_MAX, 8, true}, //Rotation step, rolls over so it can rotate backwards
        {&settingRainbowBrightness, 255, 16, false},
        {&settingMicrophone, 1, 1, false} //Microphone is stored at the same address across modes
    }, { //MODE_WHITE 
        {&settingWhiteBrightness, 255, 16, false}, //Brightness (yawn)
        {&settingMicrophone, 1, 1, false} //Microphone is stored at the same address across modes
    }
};

int currentMode = MODE_COLOR;
int currentModeNumSettings = MODE_COLOR_NUM_SETTINGS;
int currentSetting = 0;

void cycleMode() {
    currentSetting = 0;
    switch(currentMode) {
        case MODE_COLOR:
            currentMode = MODE_RAINBOW;
            currentModeNumSettings = MODE_RAINBOW_NUM_SETTINGS;
            break;
        case MODE_RAINBOW:
            currentMode = MODE_WHITE;
            currentModeNumSettings = MODE_WHITE_NUM_SETTINGS;
            break;
        case MODE_WHITE:
        default:
            currentMode = MODE_COLOR;
            currentMode = MODE_COLOR_NUM_SETTINGS;
            break;
    }
}


void cycleSettings() {
    currentSetting++;
    if(currentSetting >= currentModeNumSettings) {
        currentSetting = 0;
    }
}

void updateLEDs() {
    switch(currentMode) {
        case MODE_COLOR:
            break;
        case MODE_RAINBOW:
            break;
        case MODE_WHITE:
            break;
    }
    indicateSetting();
    strip.show();
}

void updateColor() {
    //Only update whole strip when a setting is changed or if we just changed to this mode / exited the settings
    if(rotation != 0 || buttonLongPress || buttonShortPress) {
        for(int i = 0; i < strip.numPixels(); i++) {
            strip.setPixelColor(i, strip.ColorHSV(settingColorHue, settingColorSaturation, settingColorValue));
        }
    }

    if(settingMicrophone == 1) {
        //Gradually reset pixels to original color (only execute if microphone is activated)
        uint32_t currentColor = strip.ColorHSV(settingColorHue, settingColorSaturation, settingColorValue);
        for(int i = 0; i < strip.numPixels(); i++) {
            uint32_t pixelColor = strip.getPixelColor(i);
            if(pixelColor != currentColor) {
                strip.setPixelColor(i, graduallyResetColor(pixelColor, currentColor, 0.8));
            }
        }

        //make a random pixel light up
        if(microphoneActivated) {
            uint8_t newBrightness = 255 - (255 - settingColorValue) * 0.5;
            strip.setPixelColor(random(13), strip.ColorHSV(settingColorHue, settingColorSaturation / 2, settingColorValue * 2));
        } 
    }
}

uint16_t rainbowCurrentPosition = 0;
unsigned long rainbowMicrophoneStart = 0;
int rainbowMicrophoneDuration = 500;
void updateRainbow() {
    if(microphoneListening) {
        rainbowMicrophoneStart = millis();
    }

    rainbowCurrentPosition += settingRainbowStep;
    if(millis() - rainbowMicrophoneStart > rainbowMicrophoneDuration) {
        //turn another stepwidth if microphon was listening in the last 500ms (turn twice as fast)
        rainbowCurrentPosition += settingRainbowStep;
    }

    //Pixels are positioned as follows: 
    //  0 is in the center
    //  1 is halfway from the center
    //  2 is furthest from the center
    //  from then on it is alternating between halfway and furthest.
    strip.setPixelColor(0, strip.ColorHSV(0,0,settingRainbowBrightness)); //the center is always white
    for(int i = 1; i < strip.numPixels(); i++) {
        //Pixels furthest from the center are displayed at max saturation while the others are at half saturation
        uint8_t saturation = (i%2 == 0) ? 255 : 127;
        strip.setPixelColor(0, strip.ColorHSV((UINT16_MAX/12) * i + rainbowCurrentPosition, saturation, settingRainbowBrightness));
    }
}

void updateWhite() {
    //Only update whole strip when a setting is changed or if we just changed to this mode / exited the settings
    if(rotation != 0 || buttonLongPress || buttonShortPress) {
        for(int i = 0; i < strip.numPixels(); i++) {
            strip.setPixelColor(i, strip.ColorHSV(0,0, settingWhiteBrightness)); 
        }
    }

    if(settingMicrophone == 1) {
        //Gradually reset pixels to original color (only execute if microphone is activated)
        uint32_t currentColor = strip.ColorHSV(settingColorHue, settingColorSaturation, settingColorValue);
        for(int i = 0; i < strip.numPixels(); i++) {
            uint32_t pixelColor = strip.getPixelColor(i);
            if(pixelColor != currentColor) {
                strip.setPixelColor(i, graduallyResetColor(pixelColor, currentColor, 0.8));
            }
        }

        //make a random pixel light up
        if(microphoneActivated) {
            strip.setPixelColor(random(13), 
                strip.Color(random(settingWhiteBrightness, 255), 
                            random(settingWhiteBrightness, 255),
                            random(settingWhiteBrightness, 255)));
        } 
    }
}

const int indicateSettingBlinkFrames = 10;
bool indicateSettingBlinkOn = true;
int indicateSettingFrameCounter = 0;
void indicateSetting() {
    //Indicate current setting by turning off one LED per settin (sans the 0th Setting (Default setting / quick setting))
    for(int i = 1; i < currentModeNumSettings; i++) {
        strip.setPixelColor(i, strip.Color(0,0,0));
        if(i == currentSetting) {
            if(indicateSettingBlinkOn) {
                strip.setPixelColor(i, strip.Color(255,255,255));
            }
            if(indicateSettingFrameCounter > indicateSettingBlinkFrames) {
                indicateSettingFrameCounter = 0;
                indicateSettingBlinkOn = !indicateSettingBlinkOn;
            }
            indicateSettingFrameCounter ++;
        }
    }
    //In order to display the microphone setting turn all LEDs off
    //If the center is green the microphone is on, if it is red the microphone is off
    if(settings[currentMode][currentSetting].value = &settingMicrophone) {
        for(int i = currentModeNumSettings; i < strip.numPixels(); i ++ ) {
            strip.setPixelColor(i, 0x00000000);
        }

        if(settingMicrophone == 1) {
            strip.setPixelColor(0, strip.Color(0,255,0));
        } else if (settingMicrophone == 0) {
            strip.setPixelColor(0, strip.Color(255,0,0));
        }
    }
}

void updateSetting() {
    Setting s = settings[currentMode][currentSetting];
    if(rotation == 1) {
        if(s.max - *s.value < s.step) {
            if(s.rollover) {
                *s.value = s.step - (s.max - *s.value);
            } else {
                *s.value = s.max;
            }
        }
    } else if (rotation == -1) {
        if(*s.value < s.step) {
            if(s.rollover) {
                *s.value = s.max - (s.step - 1 - *s.value);
            } else {
                *s.value = 0;
            }
        }
    }
}

//--HELPER-FUNCTIONS--
void eepromWriteUInt16(int address, uint16_t value) {
    uint8_t b1 = ((value >> 8) & 0xFF);
    uint8_t b2 = (value & 0xFF);

    EEPROM.update(address, b1);
    EEPROM.update(address+1, b2);
}

uint16_t eepromReadUInt16(int address) {
    uint16_t value = EEPROM.read(address) << 8;
    value += EEPROM.read(address + 1);
    
    return value;
}

uint32_t graduallyResetColor(uint32_t color, uint32_t target, double factor) {
    uint8_t colorRGBA[4] = {
            (color & 0x000000FF), 
            (color & 0x0000FF00) >> 8, 
            (color & 0x00FF0000) >> 16, 
            (color & 0xFF000000) >> 24
        };
    uint8_t targetRGBA[4] = {
            (target & 0x000000FF), 
            (target & 0x0000FF00) >> 8, 
            (target & 0x00FF0000) >> 16, 
            (target & 0xFF000000) >> 24
        };

    uint8_t newColorRGBA[4] = {};

    for(int i=0; i<4; i++) {
        int difference = colorRGBA[i] - targetRGBA[i];
        newColorRGBA[i] = targetRGBA[i] + (difference * factor);
    }

    uint32_t newColor = (newColorRGBA[3] << 24) | (newColorRGBA[2] << 16) | (newColorRGBA[1] << 8) | newColorRGBA[0];

    return newColor;
}
