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
const double LED_BLINK_RESET_FACTOR = 0.95;
const double RAINBOW_ACCELERATION_FACTOR = 4;

#define ROTARY_A 7
#define ROTARY_B 8
#define BUTTON 9
#define LONGPRESS_DURATION 500 //ms
const int MICROPHONE = A0;

#define DEBOUNCE 50


int buttonState = HIGH;
unsigned long buttonPressedSince = 0;
unsigned long buttonDebounce = 0;
bool buttonShortPress = false;
bool buttonLongPress = false;
bool buttonSentLongPress = false;

int rotaryAState = HIGH;
int rotation = 0;

bool microphoneActivated = false;
bool microphoneListening = false;

Adafruit_NeoPixel strip(LED_COUNT, LED, NEO_GRB + NEO_KHZ800);
bool redrawLEDs = true;

struct hsvColor {
    uint16_t hue = 0;
    uint8_t saturation = 0;
    uint8_t value = 0;
};

hsvColor LED_HSV_COLORS[LED_COUNT];

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
    strip.setBrightness(100);

    //Setup other inputs
    pinMode(BUTTON, INPUT_PULLUP);
    pinMode(ROTARY_A, INPUT);
    pinMode(ROTARY_B, INPUT);
    pinMode(MICROPHONE, INPUT);

    //Load Settings from EEPROM
    settingBrightness = eepromReadUInt16(SETTING_BRIGHTNESS_ADDRESS);

    settingColorHue = eepromReadUInt16(SETTING_COLOR_HUE_ADDRESS);
    settingColorSaturation = eepromReadUInt16(SETTING_COLOR_SATURATION_ADDRESS);

    settingRainbowStep = eepromReadUInt16(SETTING_RAINBOW_STEP_ADDRESS);

    settingMicrophone = eepromReadUInt16(SETTING_MICROPHONE_ADDRESS);

    settingMicrophoneThreshhold = eepromReadUInt16(SETTING_MICROPHONE_THRESHHOLD_ADDRESS);

    Serial.println("Loaded settings from EEPROM:");
    setMode(eepromReadUInt16(MODE_ADDRESS));

    Serial.print("settingBrightness: ");
    Serial.println(settingBrightness);

    Serial.print("settingColorHue: ");
    Serial.println(settingColorHue);
    Serial.print("settingColorSaturation: ");
    Serial.println(settingColorSaturation);
    
    Serial.print("settingRainbowStep: ");
    Serial.println(settingRainbowStep);
    
    Serial.print("settingMicrophone: ");
    Serial.println(settingMicrophone);

    Serial.print("settingMicrophoneThreshhold: ");
    Serial.println(settingMicrophoneThreshhold);
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
    microphoneActivated = !microphoneListening && analogRead(MICROPHONE) > settingMicrophoneThreshhold;
    //Current state of the microphone
    microphoneListening = analogRead(MICROPHONE) > settingMicrophoneThreshhold;
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



#define MODE_COLOR 0
#define MODE_RAINBOW 1
#define MODE_WHITE 2

#define MODE_COLOR_NUM_SETTINGS 5
#define MODE_RAINBOW_NUM_SETTINGS 4
#define MODE_WHITE_NUM_SETTINGS 3

struct Setting {
    int eepromAddress;
    uint16_t* value;
    uint16_t max;
    uint16_t step;
    bool rollover;
};

Setting settings[3][5] = {
    { //MODE_COLOR
        {SETTING_COLOR_HUE_ADDRESS,             &settingColorHue,             UINT16_MAX, 1024, true }, //hue, 64 steps for full rotation (little more than 3 turns)
        {SETTING_COLOR_SATURATION_ADDRESS,      &settingColorSaturation,      255,        16,   false}, //Saturation, 16 Steps
        {SETTING_BRIGHTNESS_ADDRESS,            &settingBrightness,           127,        8,    false}, //Value/Brightness, 16 Steps is stored at the same address across modes
        {SETTING_MICROPHONE_ADDRESS,            &settingMicrophone,           1,          1,    false}, //Microphone is stored at the same address across modes
        {SETTING_MICROPHONE_THRESHHOLD_ADDRESS, &settingMicrophoneThreshhold, 1023,       1,    false}  //Microphone threshhold is stored at the same address across modes
    }, { //MODE_RAINBOW 
        {SETTING_RAINBOW_STEP_ADDRESS,          &settingRainbowStep,          UINT16_MAX, 8, true }, //Rotation step, rolls over so it can rotate backwards
        {SETTING_BRIGHTNESS_ADDRESS,            &settingBrightness,           127,        8, false}, //Brightness, 16 Steps is stored at the same address across modes
        {SETTING_MICROPHONE_ADDRESS,            &settingMicrophone,           1,          1, false}, //Microphone is stored at the same address across modes
        {SETTING_MICROPHONE_THRESHHOLD_ADDRESS, &settingMicrophoneThreshhold, 1023,       1, false}  //Microphone threshhold is stored at the same address across modes
    }, { //MODE_WHITE 
        {SETTING_BRIGHTNESS_ADDRESS,            &settingBrightness,           127,  8, false}, //Brightness, 16 Steps is stored at the same address across modes
        {SETTING_MICROPHONE_ADDRESS,            &settingMicrophone,           1,    1, false}, //Microphone is stored at the same address across modes
        {SETTING_MICROPHONE_THRESHHOLD_ADDRESS, &settingMicrophoneThreshhold, 1023, 1, false}  //Microphone threshhold is stored at the same address across modes
    }
};

int currentMode = MODE_COLOR;
int currentModeNumSettings = MODE_COLOR_NUM_SETTINGS;
int currentSetting = 0;

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
        case MODE_COLOR:
        default:
            currentMode = MODE_COLOR;
            currentModeNumSettings = MODE_COLOR_NUM_SETTINGS;
            break;
    }
    Serial.print("Switched to mode ");
    Serial.println(currentMode);
    eepromWriteUInt16(MODE_ADDRESS, currentMode);
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
        if(pixelColor.hue        != hue 
        || pixelColor.saturation != saturation
        || pixelColor.value      != value) {
        
            //the hue overflows from 
            if(hue > pixelColor.hue) {
                uint16_t difference = hue - pixelColor.hue;
                if(difference < 0x8000) {
                    newColor.hue = hue - difference * LED_BLINK_RESET_FACTOR;
                } else {
                    newColor.hue = hue + (difference % 0x8000) * LED_BLINK_RESET_FACTOR;  
                }
            } else {
                uint16_t difference = pixelColor.hue - hue;
                if(difference < 0x8000) {
                    newColor.hue = hue + difference * LED_BLINK_RESET_FACTOR;
                } else {
                    newColor.hue = hue - (difference % 0x8000) * LED_BLINK_RESET_FACTOR;  
                }
            }

            newColor.saturation = saturation + (pixelColor.saturation - saturation) * LED_BLINK_RESET_FACTOR;
            newColor.value = value + (pixelColor.value - value) * LED_BLINK_RESET_FACTOR;

            setPixelHSV(i, newColor);
        }
    }
}

void updateColor() {
    //Only update whole strip when a setting is changed or if we just changed to this mode / exited the settings
    if(redrawLEDs) {
        fillPixelsHSV(settingColorHue, settingColorSaturation, settingBrightness, 0, 0);
        redrawLEDs = false;
    }

    if(settingMicrophone == 1) {
        //Gradually reset pixels to original color (only execute if microphone is activated)
        uint32_t currentColor = strip.ColorHSV(settingColorHue, settingColorSaturation, settingBrightness);
        graduallyResetColors(settingColorHue, settingColorSaturation, settingBrightness);

        //make a random pixel light up
        if(microphoneListening) {
            setPixelHSV(random(LED_COUNT), settingColorHue, settingColorSaturation / 2, settingBrightness * LED_BLINK_BRIGHTNESS_FACTOR);
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
    setPixelHSV(0, 0,0,settingBrightness); //the center is always white
    for(int i = 1; i < strip.numPixels(); i++) {
        //Pixels furthest from the center are displayed at max saturation while the others are at half saturation
        uint8_t saturation = (i%2 == 0) ? 255 : 127;
        setPixelHSV(i, (UINT16_MAX/12) * i + rainbowCurrentPosition, saturation, settingBrightness);
    }
}

void updateWhite() {
    //Only update whole strip when a setting is changed or if we just changed to this mode / exited the settings
    if(redrawLEDs) {
        fillPixelsHSV(0, 0, settingBrightness, 0, 0);
        redrawLEDs = false;
    }

    if(settingMicrophone == 1) {
        //Gradually reset pixels to original color (only execute if microphone is activated)
        for(int i = 0; i < LED_COUNT; i++) {
            graduallyResetColors(getPixelHSV(i).hue, 0, settingBrightness, i, 1);
        }

        //make a random pixel light up
        if(microphoneListening) {
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
            //In order to display the microphone threshhold setting turn all LEDs off
            //If the center LED lights up, the microphone detects input
            fillPixelsHSV(0,0,0,0,0);
            if(microphoneListening) {
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
    }

    *s.value = currentValue;
    eepromWriteUInt16(s.eepromAddress, currentValue);
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
