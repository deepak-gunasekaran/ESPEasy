/*
   ShiftRegister74HC595.h - Library for simplified control of 74HC595 shift registers.
   Developed and maintained by Timo Denk and contributers, since Nov 2014.
   Additional information is available at https://timodenk.com/blog/shift-register-arduino-library/
   Released into the public domain.
 */

#pragma once

#include <Arduino.h>
#include <vector>

class ShiftRegister74HC595_NonTemplate {
public:

  ShiftRegister74HC595_NonTemplate(const uint8_t size,
                                   const uint8_t serialDataPin,
                                   const uint8_t clockPin,
                                   const uint8_t latchPin);

  void     setAll(const uint8_t *digitalValues);
  uint8_t* getAll();
  void     set(const uint8_t pin,
               const uint8_t value);
  void     setNoUpdate(const uint8_t pin,
                       uint8_t       value);
  void     updateRegisters();
  void     setAllLow();
  void     setAllHigh();
  uint8_t  get(const uint8_t pin);

private:

  uint8_t _size;
  uint8_t _clockPin;
  uint8_t _serialDataPin;
  uint8_t _latchPin;

  std::vector<uint8_t>_digitalValues;
};
