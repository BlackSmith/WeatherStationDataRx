#pragma once

#include <cstdint>

#define IRAM_ATTR
#define INPUT 0
#define CHANGE 1

extern unsigned long mock_time_us;
extern bool mock_pin_high;
extern void (*mock_interrupt)();

inline unsigned long micros() { return mock_time_us; }
inline unsigned long millis() { return mock_time_us / 1000; }
inline void delay(unsigned long ms) { mock_time_us += ms * 1000; }
inline void pinMode(uint8_t, int) {}
inline bool digitalRead(uint8_t) { return mock_pin_high; }
inline uint8_t digitalPinToInterrupt(uint8_t pin) { return pin; }
inline void attachInterrupt(uint8_t, void (*callback)(), int) { mock_interrupt = callback; }
inline void detachInterrupt(uint8_t) { mock_interrupt = nullptr; }
