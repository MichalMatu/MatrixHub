#ifdef NATIVE_BUILD

#include <Arduino.h>

extern "C" {

__attribute__((weak)) void pinMode(uint8_t pin, uint8_t mode) {
    (void)pin;
    (void)mode;
}

__attribute__((weak)) void digitalWrite(uint8_t pin, uint8_t val) {
    (void)pin;
    (void)val;
}

__attribute__((weak)) int digitalRead(uint8_t pin) {
    (void)pin;
    return HIGH;
}

__attribute__((weak)) void delayMicroseconds(uint32_t us) {
    (void)us;
}

}  // extern "C"

#endif
