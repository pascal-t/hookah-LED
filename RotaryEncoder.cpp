#include <Arduino.h>
#include "RotaryEncoder.h"

/**
 * Creates a Rotary Encoder with pins A and B / Clk and Dt
 */
RotaryEncoder::RotaryEncoder(int pinA, int pinB) {
    pinMode(pinA, INPUT);
    pinMode(pinB, INPUT);

    _pinA = pinA;
    _pinB = pinB;
    _aLast = digitalRead(pinA);
}

/**
 * Reads the input of the rotary encoder and returns 0 for no rotation, -1 for counterclockwise and 1 for clockwise.
 * Call this method at the beginning of the loop function to receive meaningful reads;
 * 
 * States during a clockwise rotation:
 *  A   |  B
 * HIGH | HIGH <- Stationary
 * LOW  | HIGH
 * LOW  | LOW 
 * HIGH | LOW 
 * HIGH | HIGH <- Stationary
 *
 * States during a counterclockwise roation:
 *  A   |  B
 * HIGH | HIGH <- Stationary
 * HIGH | LOW 
 * LOW  | LOW 
 * LOW  | HIGH
 * HIGH | HIGH <- Stationary
 * 
 * Observation: when A changes to LOW, B reads HIGH for a clockwise rotation and LOW for a counterclockwise rotation
 */
void RotaryEncoder::read() {
    int aState = digitalRead(_pinA);
    if(_aLast != aState && aState == LOW) { //A changes to LOW
        if(aState != digitalRead(_pinB)) { //B reads HIGH
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
    _aLast = aState;
}

int RotaryEncoder::getRotation() {
    return rotation;
}