#ifndef CONTROLMODULE_H
#define CONTROLMODULE_H

#include <Arduino.h>

#include "relaymodule.h"

class ControlModule {
public:
  enum class State {
    IDLE,
    WAITING_COOLING,
    COOLING,
    HEATING,
    SENSOR_ERROR
  };

  explicit ControlModule(
    RelayModule& relayModule
  );

  void begin();

  void update(
    float controlTemperature,
    bool sensorConnected
  );

  void setSetpoint(
    float temperature
  );

  float getSetpoint() const;

  void setHysteresis(
    float hysteresis
  );

  float getHysteresis() const;

  void setCompressorProtectionSeconds(
    uint32_t seconds
  );

  uint32_t
  getCompressorProtectionSeconds() const;

  State getState() const;

  const char* getStateText() const;

  unsigned long
  getCoolingDelayRemainingSeconds() const;

private:
  RelayModule& relays;

  float setpoint;
  float hysteresis;
  uint32_t compressorProtectionSeconds;

  State currentState;

  unsigned long lastCoolingOffTime;

  static constexpr uint32_t
    MINIMUM_COMPRESSOR_PROTECTION_SECONDS =
      60UL;

  static constexpr uint32_t
    MAXIMUM_COMPRESSOR_PROTECTION_SECONDS =
      900UL;

  unsigned long
  compressorProtectionMilliseconds() const;

  bool canStartCooling() const;

  void setState(
    State newState
  );
};

#endif
