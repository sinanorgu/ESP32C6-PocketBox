#include "definitions.hpp"

void setBacklightBrightness(uint8_t brightness) {
    // Set the backlight brightness using PWM
    analogWrite(TFT_BL, brightness);
} 