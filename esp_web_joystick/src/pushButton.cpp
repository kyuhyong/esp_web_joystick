#include "pushButton.h"

PUSH_BUTTON::PUSH_BUTTON(uint8_t pin, uint8_t pushpull)
{
   this->_pin = pin;
   this->_pushpull = pin;
   this->_state = false;
   this->_longPressDelay_ms = 1000;
   if(pushpull == INPUT_PULLUP) {
      this->_buttonPressedState = 0;
   } else {
      this->_buttonPressedState = 1;
   }
   pinMode(pin, pushpull);
}

void PUSH_BUTTON::onNewPushButtonEvent(PushButton_PressEvent cvEvent)
{
   this->_cbEvent = cvEvent;
}

void PUSH_BUTTON::update()
{
   if(!this->_state) {
      if(digitalRead(this->_pin) == this->_buttonPressedState) {
         this->_state = true;
         this->_last_pressed_millis = millis();
      }
   } else {
      if(digitalRead(this->_pin) != this->_buttonPressedState) {
         this->_state = false;
         unsigned long btn_delay = millis() - this->_last_pressed_millis;
         if(btn_delay > this->_longPressDelay_ms) {
            this->_cbEvent(LONG_PRESS);
         } else {
            this->_cbEvent(SHORT_PRESS);
         }
      }
   }
}