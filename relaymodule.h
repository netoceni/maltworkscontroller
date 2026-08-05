#ifndef RELAYMODULE_H
#define RELAYMODULE_H

#include <Arduino.h>

class RelayModule {
public:
  RelayModule(
    uint8_t coolingPin,
    uint8_t heatingPin,
    bool activeLow = true
  );

  void begin();

  void coolingOn();
  void coolingOff();

  void heatingOn();
  void heatingOff();

  void allOff();

  bool isCoolingOn() const;
  bool isHeatingOn() const;

private:
  uint8_t coolingPin;
  uint8_t heatingPin;

  bool activeLow;
  bool coolingState;
  bool heatingState;

  void writeRelay(
    uint8_t pin,
    bool turnOn
  );
};

#endif
