#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
 #include <avr/power.h> // Required for 16 MHz Adafruit Trinket
#endif

#define LED 6
#define LED_COUNT 13
#define UPDATE_DELAY 20 //50 Updates/Second
unsigned long lastUpdate = 0;

const double LED_BLINK_BRIGHTNESS_FACTOR = 2.0;
const double LED_BLINK_RESET_FACTOR = 0.8;
const double RAINBOW_ACCELERATION_FACTOR = 4;

#define ROTARY_A 7
#define ROTARY_B 8
#define BUTTON 9
#define LONGPRESS_DURATION 250 //ms
#define MICROPHONE 10

#define DEBOUNCE 50


int buttonState = HIGH;
unsigned long buttonPressedSince = 0;
unsigned long buttonDebounce = 0;
bool buttonShortPress = false;
bool buttonLongPress = false;

int rotaryAState = HIGH;
int rotation = 0;

bool microphoneActivated = false;
bool microphoneListening = false;

Adafruit_NeoPixel strip(LED_COUNT, LED, NEO_GRB + NEO_KHZ800);
bool redrawLEDs = true;

uint16_t settingBrightness = 0;
#define SETTING_BRIGHTNESS_ADDRESS 0

uint16_t settingColorHue = 0;
#define SETTING_COLOR_HUE_ADDRESS 2
uint16_t settingColorSaturation = 0;
#define SETTING_COLOR_SATURATION_ADDRESS 4

uint16_t settingRainbowStep = 0;
#define SETTING_RAINBOW_STEP_ADDRESS 6

uint16_t settingMicrophone = 0;
#define SETTING_MICROPHONE_ADDRESS 8


void setup() {
    //Setup LED Strip
#if defined(__AVR_ATtiny85__) && (F_CPU == 16000000)
    clock_prescale_set(clock_div_1);
#endif

    strip.begin();           
    strip.show();
    strip.setBrightness(100);

    //Setup other inputs
    pinMode(BUTTON, INPUT_PULLUP);
    pinMode(ROTARY_A, INPUT);
    pinMode(ROTARY_B, INPUT);
    pinMode(MICROPHONE, INPUT);

//    //Load Settings from EEPROM
//    settingBrightness = eepromReadUInt16(SETTING_BRIGHTNESS_ADDRESS);
//
//    settingColorHue = eepromReadUInt16(SETTING_COLOR_HUE_ADDRESS);
//    settingColorSaturation = eepromReadUInt16(SETTING_COLOR_SATURATION_ADDRESS);
//
//    settingRainbowStep = eepromReadUInt16(SETTING_RAINBOW_STEP_ADDRESS);
//
//    settingMicrophone = eepromReadUInt16(SETTING_MICROPHONE_ADDRESS);

    
    settingBrightness = 127;

    settingColorHue = 0;
    settingColorSaturation = 255;

    settingRainbowStep = 512;

    settingMicrophone = 1;

    Serial.begin(9600);
}

void readInputs() {
    //Button state changed after debounce timer
    if((buttonState != digitalRead(BUTTON)) && millis() - buttonDebounce > DEBOUNCE) {

        //Start debounce timer so we won't detect a button state change until it has run out
        buttonDebounce = millis();
        buttonState = digitalRead(BUTTON);

        if(buttonState == LOW) {
            //Button pressed because INPUT_PULLUP reverses HIGH and LOW
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
    
    /*  Rotary encoder
        States during a clockwise rotation:
         A   |  B
        HIGH | HIGH <- Stationary
        LOW  | HIGH
        LOW  | LOW 
        HIGH | LOW 
        HIGH | HIGH <- Stationary

        States during a counterclockwise roation:
         A   |  B
        HIGH | HIGH <- Stationary
        HIGH | LOW 
        LOW  | LOW 
        LOW  | HIGH
        HIGH | HIGH <- Stationary

        Observation: when A changes to LOW, B reads HIGH for a clockwise rotation and LOW for a counterclockwise rotation
    */
    int stateA = digitalRead(ROTARY_A);
    if(rotaryAState != stateA && stateA == LOW) { //A changes to LOW
        if(stateA != digitalRead(ROTARY_B)) { //B reads HIGH
            //Clockwise rotation
            rotation = 1;
        } else { //B reads LOW
            //Counterclockwise rotation
            rotation = -1;
        }
    } else {
        //No change; no rotation
        rotation = 0;
    }
    rotaryAState = stateA;
    
    
    //Microphone
    //Record if the microphone is switched on. The microphone is HIGH when no sound is detected
    microphoneActivated = !microphoneListening && digitalRead(MICROPHONE) == LOW;
    //Current state of the microphone
    microphoneListening = digitalRead(MICROPHONE) == LOW;
}

void loop() {
    readInputs();
    if(buttonLongPress) {
        Serial.println("cycleMode()");
        cycleMode();
    }
    if(buttonShortPress) {
        Serial.println("cycleSettings()");
        cycleSettings();
    }
    if(rotation != 0) {
        Serial.print("updateSetting() ");
        Serial.println(rotation);
        updateSetting();
    }

    //Update the LED Strip at a limited rate
    if(millis() - lastUpdate > UPDATE_DELAY) {
        lastUpdate = millis();
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
        {&settingBrightness, 127, 8, false}, //Value/Brightness, 16 Steps is stored at the same address across modes
        {&settingMicrophone, 1, 1, false} //Microphone is stored at the same address across modes
    }, { //MODE_RAINBOW 
        {&settingRainbowStep, UINT16_MAX, 8, true}, //Rotation step, rolls over so it can rotate backwards
        {&settingBrightness, 127, 8, false},//Brightness, 16 Steps is stored at the same address across modes
        {&settingMicrophone, 1, 1, false} //Microphone is stored at the same address across modes
    }, { //MODE_WHITE 
        {&settingBrightness, 127, 8, false}, //Brightness, 16 Steps is stored at the same address across modes
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
            currentModeNumSettings = MODE_COLOR_NUM_SETTINGS;
            break;
    }
    redrawLEDs = true;
    Serial.print("Switched to mode ");
    Serial.println(currentMode);
}


void cycleSettings() {
    ++currentSetting %= currentModeNumSettings;
    redrawLEDs = true;
    Serial.print("Current mode: ");
    Serial.println(currentMode);
    Serial.print("Switched to setting ");
    Serial.println(currentSetting);
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
    }
    indicateSetting();
    strip.show();
}

void updateColor() {
    //Only update whole strip when a setting is changed or if we just changed to this mode / exited the settings
    if(redrawLEDs) {
        strip.fill(strip.ColorHSV(settingColorHue, settingColorSaturation, settingBrightness));
        redrawLEDs = false;
    }

    if(settingMicrophone == 1) {
        //Gradually reset pixels to original color (only execute if microphone is activated)
        uint32_t currentColor = strip.ColorHSV(settingColorHue, settingColorSaturation, settingBrightness);
        graduallyResetColors(currentColor);

        //make a random pixel light up
        if(microphoneListening) {
            strip.setPixelColor(random(LED_COUNT), strip.ColorHSV(settingColorHue, settingColorSaturation / 2, settingBrightness * LED_BLINK_BRIGHTNESS_FACTOR));
        } 
    }
}

uint16_t rainbowCurrentPosition = 0;
unsigned long rainbowMicrophoneStart = 0;
int rainbowMicrophoneDuration = 500;
void updateRainbow() {
    if(settingMicrophone == 1 && microphoneListening) {
        rainbowMicrophoneStart = millis();
    }

    //increase the hue of the rainbow. When thw varable overflows it starts over, wich is actually convenient
    if(millis() - rainbowMicrophoneStart < rainbowMicrophoneDuration) {
        //turn another stepwidth if microphon was listening in the last 500ms (turn twice as fast)
        rainbowCurrentPosition += settingRainbowStep * RAINBOW_ACCELERATION_FACTOR;
    } else {
        rainbowCurrentPosition += settingRainbowStep;
    }

    //Pixels are positioned as follows: 
    //  0 is in the center
    //  1 is halfway from the center
    //  2 is furthest from the center
    //  from then on it is alternating between halfway and furthest.
    strip.setPixelColor(0, strip.ColorHSV(0,0,settingBrightness)); //the center is always white
    for(int i = 1; i < strip.numPixels(); i++) {
        //Pixels furthest from the center are displayed at max saturation while the others are at half saturation
        uint8_t saturation = (i%2 == 0) ? 255 : 127;
        strip.setPixelColor(i, strip.ColorHSV((UINT16_MAX/12) * i + rainbowCurrentPosition, saturation, settingBrightness));
    }
}

void updateWhite() {
    //Only update whole strip when a setting is changed or if we just changed to this mode / exited the settings
    if(redrawLEDs) {
        strip.fill(strip.Color(settingBrightness,settingBrightness, settingBrightness), 0, LED_COUNT);
        redrawLEDs = false;
    }

    if(settingMicrophone == 1) {
        //Gradually reset pixels to original color (only execute if microphone is activated)
        graduallyResetColors(strip.ColorHSV(settingBrightness, settingBrightness, settingBrightness));

        //make a random pixel light up
        if(microphoneListening) {
            strip.setPixelColor(random(13), strip.ColorHSV(random(UINT16_MAX), 255, settingBrightness * LED_BLINK_BRIGHTNESS_FACTOR));
        } 
    }
}


void graduallyResetColors(uint32_t targetColor) {
    uint8_t targetRGB[3];
    targetRGB[0] = (targetColor & 0x00FF0000) >> 16;
    targetRGB[1] = (targetColor & 0x0000FF00) >> 8;
    targetRGB[2] = (targetColor & 0x000000FF);
    
    uint8_t pixelRGB[3];
    uint8_t newColorRGB[3];
    
    for(int i=0; i<LED_COUNT; i++) {
        uint32_t pixelColor = strip.getPixelColor(i);
        if(pixelColor != targetColor) {
            pixelRGB[0] = (pixelColor & 0x00FF0000) >> 16;
            pixelRGB[1] = (pixelColor & 0x0000FF00) >> 8;
            pixelRGB[2] = (pixelColor & 0x000000FF);
        
            for(int j=0; j<3; j++) {
                int difference = pixelRGB[j] - targetRGB[j];
                newColorRGB[j] = targetRGB[j] + (difference * LED_BLINK_RESET_FACTOR);
            }
            strip.setPixelColor(i, strip.Color(newColorRGB[0], newColorRGB[1], newColorRGB[2]));
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
            strip.fill(strip.Color(0,0,0));
            if(settingMicrophone == 1) {
                strip.setPixelColor(0, strip.Color(0,settingBrightness,0));
            } else if (settingMicrophone == 0) {
                strip.setPixelColor(0, strip.Color(settingBrightness,0,0));
            }
        } else {
            //for all the other settings just turn off enough LEDs to indicate the current setting
            strip.fill(strip.Color(0,0,0), 1, currentModeNumSettings - 1);
        }
        
        ++indicateSettingFrameCounter %= indicateSettingBlinkFrames * 2;
        if(indicateSettingFrameCounter < indicateSettingBlinkFrames) {
            strip.setPixelColor(currentSetting, strip.ColorHSV(0,0,settingBrightness));
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
    Serial.print(" + ");
    Serial.print(s.step);
    

    uint32_t currentValue = *s.value;
    if(rotation == 1) {
        currentValue += s.step;
        if(currentValue > s.max) {
            if(s.rollover) {
              currentValue -= s.max + 1;
            } else {
              currentValue = s.max;
            }
        }
    } else if (rotation == -1) {
        if(currentValue < s.step) {
            if(s.rollover) {
                currentValue = s.max - (s.step - 1 - currentValue);
            } else {
                currentValue = 0;
            }
        } else {
            currentValue -= s.step;
        }
    }

    *s.value = currentValue;
    redrawLEDs = true;

    Serial.print(" -> ");
    Serial.println(*s.value);
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
