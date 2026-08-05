#include <math.h>

#include "controlmodule.h"

ControlModule::ControlModule(
  RelayModule& relayModule
) :
  relays(relayModule),
  setpoint(20.0f),
  hysteresis(0.5f),
  compressorProtectionSeconds(60UL),
  currentState(State::IDLE),
  lastCoolingOffTime(0) {
}

void ControlModule::begin() {
  relays.allOff();

  currentState =
    State::IDLE;

  lastCoolingOffTime =
    millis();

  Serial.println(
    "Modulo de controle inicializado."
  );

  Serial.print(
    "Protecao do compressor: "
  );

  Serial.print(
    compressorProtectionSeconds
  );

  Serial.println(" segundos");
}

void ControlModule::update(
  float controlTemperature,
  bool sensorConnected
) {
  if (
    !sensorConnected ||
    isnan(controlTemperature)
  ) {
    setState(
      State::SENSOR_ERROR
    );

    return;
  }

  float coolingStartTemperature =
    setpoint + hysteresis;

  float heatingStartTemperature =
    setpoint - hysteresis;

  switch (currentState) {
    case State::IDLE:
    case State::SENSOR_ERROR:

      if (
        controlTemperature >=
        coolingStartTemperature
      ) {
        if (canStartCooling()) {
          setState(
            State::COOLING
          );
        } else {
          setState(
            State::WAITING_COOLING
          );
        }
      }
      else if (
        controlTemperature <=
        heatingStartTemperature
      ) {
        setState(
          State::HEATING
        );
      }
      else {
        setState(
          State::IDLE
        );
      }

      break;

    case State::WAITING_COOLING:

      if (
        controlTemperature <=
        setpoint
      ) {
        setState(
          State::IDLE
        );
      }
      else if (canStartCooling()) {
        setState(
          State::COOLING
        );
      }

      break;

    case State::COOLING:

      if (
        controlTemperature <=
        setpoint
      ) {
        setState(
          State::IDLE
        );
      }

      break;

    case State::HEATING:

      if (
        controlTemperature >=
        setpoint
      ) {
        setState(
          State::IDLE
        );
      }

      break;
  }
}

void ControlModule::setState(
  State newState
) {
  if (
    newState ==
    currentState
  ) {
    return;
  }

  if (
    currentState ==
      State::COOLING &&
    newState !=
      State::COOLING
  ) {
    lastCoolingOffTime =
      millis();

    Serial.println(
      "Protecao do compressor iniciada."
    );
  }

  switch (newState) {
    case State::IDLE:
      relays.allOff();
      Serial.println(
        "Controle: AGUARDANDO"
      );
      break;

    case State::WAITING_COOLING:
      relays.allOff();
      Serial.println(
        "Controle: PROTECAO COMPRESSOR"
      );
      break;

    case State::COOLING:
      relays.coolingOn();
      Serial.println(
        "Controle: RESFRIANDO"
      );
      break;

    case State::HEATING:
      relays.heatingOn();
      Serial.println(
        "Controle: AQUECENDO"
      );
      break;

    case State::SENSOR_ERROR:
      relays.allOff();
      Serial.println(
        "Controle: ERRO NO SENSOR"
      );
      break;
  }

  currentState =
    newState;
}

bool ControlModule::
canStartCooling() const {
  unsigned long elapsedTime =
    millis() - lastCoolingOffTime;

  return elapsedTime >=
    compressorProtectionMilliseconds();
}

void ControlModule::setSetpoint(
  float temperature
) {
  if (temperature < -20.0f) {
    temperature = -20.0f;
  }

  if (temperature > 50.0f) {
    temperature = 50.0f;
  }

  setpoint =
    temperature;
}

float ControlModule::
getSetpoint() const {
  return setpoint;
}

void ControlModule::setHysteresis(
  float newHysteresis
) {
  if (newHysteresis < 0.1f) {
    newHysteresis = 0.1f;
  }

  if (newHysteresis > 10.0f) {
    newHysteresis = 10.0f;
  }

  hysteresis =
    newHysteresis;
}

float ControlModule::
getHysteresis() const {
  return hysteresis;
}

void ControlModule::
setCompressorProtectionSeconds(
  uint32_t seconds
) {
  if (
    seconds <
      MINIMUM_COMPRESSOR_PROTECTION_SECONDS
  ) {
    seconds =
      MINIMUM_COMPRESSOR_PROTECTION_SECONDS;
  }

  if (
    seconds >
      MAXIMUM_COMPRESSOR_PROTECTION_SECONDS
  ) {
    seconds =
      MAXIMUM_COMPRESSOR_PROTECTION_SECONDS;
  }

  compressorProtectionSeconds =
    seconds;
}

uint32_t ControlModule::
getCompressorProtectionSeconds() const {
  return compressorProtectionSeconds;
}

unsigned long ControlModule::
compressorProtectionMilliseconds() const {
  return static_cast<unsigned long>(
    compressorProtectionSeconds
  ) * 1000UL;
}

ControlModule::State
ControlModule::getState() const {
  return currentState;
}

const char*
ControlModule::getStateText() const {
  switch (currentState) {
    case State::IDLE:
      return "AGUARDANDO";

    case State::WAITING_COOLING:
      return "PROTECAO COMP.";

    case State::COOLING:
      return "RESFRIANDO";

    case State::HEATING:
      return "AQUECENDO";

    case State::SENSOR_ERROR:
      return "ERRO SENSOR";
  }

  return "DESCONHECIDO";
}

unsigned long
ControlModule::
getCoolingDelayRemainingSeconds() const {
  unsigned long elapsedTime =
    millis() - lastCoolingOffTime;

  if (
    elapsedTime >=
    compressorProtectionMilliseconds()
  ) {
    return 0;
  }

  unsigned long remainingTime =
    compressorProtectionMilliseconds() -
    elapsedTime;

  return (
    remainingTime + 999UL
  ) / 1000UL;
}
