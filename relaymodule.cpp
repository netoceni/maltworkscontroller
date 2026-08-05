#include "relaymodule.h"

RelayModule::RelayModule(
  uint8_t coolingPin,
  uint8_t heatingPin,
  bool activeLow
) :
  coolingPin(coolingPin),
  heatingPin(heatingPin),
  activeLow(activeLow),
  coolingState(false),
  heatingState(false) {
}

void RelayModule::begin() {
  digitalWrite(
    coolingPin,
    activeLow ? HIGH : LOW
  );

  digitalWrite(
    heatingPin,
    activeLow ? HIGH : LOW
  );

  pinMode(
    coolingPin,
    OUTPUT
  );

  pinMode(
    heatingPin,
    OUTPUT
  );

  allOff();

  Serial.println(
    "Modulo de reles inicializado."
  );
}

void RelayModule::writeRelay(
  uint8_t pin,
  bool turnOn
) {
  if (activeLow) {
    digitalWrite(
      pin,
      turnOn ? LOW : HIGH
    );
  } else {
    digitalWrite(
      pin,
      turnOn ? HIGH : LOW
    );
  }
}

void RelayModule::coolingOn() {
  heatingOff();

  writeRelay(
    coolingPin,
    true
  );

  if (!coolingState) {
    Serial.println(
      "Rele de resfriamento LIGADO."
    );
  }

  coolingState = true;
}

void RelayModule::coolingOff() {
  writeRelay(
    coolingPin,
    false
  );

  if (coolingState) {
    Serial.println(
      "Rele de resfriamento DESLIGADO."
    );
  }

  coolingState = false;
}

void RelayModule::heatingOn() {
  coolingOff();

  writeRelay(
    heatingPin,
    true
  );

  if (!heatingState) {
    Serial.println(
      "Rele de aquecimento LIGADO."
    );
  }

  heatingState = true;
}

void RelayModule::heatingOff() {
  writeRelay(
    heatingPin,
    false
  );

  if (heatingState) {
    Serial.println(
      "Rele de aquecimento DESLIGADO."
    );
  }

  heatingState = false;
}

void RelayModule::allOff() {
  writeRelay(
    coolingPin,
    false
  );

  writeRelay(
    heatingPin,
    false
  );

  coolingState = false;
  heatingState = false;
}

bool RelayModule::isCoolingOn() const {
  return coolingState;
}

bool RelayModule::isHeatingOn() const {
  return heatingState;
}
