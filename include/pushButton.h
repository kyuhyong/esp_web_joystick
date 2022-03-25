#ifndef _PUSH_BUTTON_H_
#define _PUSH_BUTTON_H_

#include <Arduino.h>

enum BUTTON_PRESS {
    SHORT_PRESS = 0,
    LONG_PRESS = 1
};

class PUSH_BUTTON {
public:
    typedef void (*PushButton_PressEvent)(BUTTON_PRESS);
    void onNewPushButtonEvent(PushButton_PressEvent cbEvent);
    PUSH_BUTTON(uint8_t pin, uint8_t pushpull);
    void update();
private:
    bool _state;
    unsigned long _last_pressed_millis;
    uint8_t _pin;
    uint8_t _pushpull;
    int                     _buttonPressedState;
    PushButton_PressEvent   _cbEvent;
    uint16_t                _longPressDelay_ms;
};

#endif