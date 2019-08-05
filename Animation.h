#include <Arduino.h>

#ifndef ANIMATION_H
#define ANIMATION_H

#define BUTTON_DEBOUNCE 50

class Animation {
    public:
        Animation(unsigned long frameLength);
        void start(int frameCount);
        bool isRunning();
        int  getFrame();
    private:
        unsigned long _frameLength = 0, _runningSince = 0;
        int _frameCount;
};

#endif //ANIMATION_H