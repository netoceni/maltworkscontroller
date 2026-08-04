#ifndef ALARMMODULE_H
#define ALARMMODULE_H

#include <Arduino.h>
#include <Preferences.h>

#include "relaymodule.h"
#include "controlmodule.h"

class AlarmModule {
public:
  enum class AlarmId : uint8_t {
    SENSOR_FAILURE = 0,
    HIGH_TEMPERATURE = 1,
    LOW_TEMPERATURE = 2,
    NO_THERMAL_RESPONSE = 3
  };

  struct Configuration {
    bool sensorAlarmEnabled;
    bool highTemperatureEnabled;
    bool lowTemperatureEnabled;
    bool responseAlarmEnabled;

    float highTemperatureLimit;
    float lowTemperatureLimit;
    float minimumExpectedChange;

    uint32_t responseTimeoutSeconds;
  };

  struct AlarmState {
    AlarmId id;
    bool active;
    bool acknowledged;
    unsigned long activeSinceMs;
  };

  AlarmModule(
    RelayModule& relayModule,
    ControlModule& controlModule
  );

  bool begin();

  void update(
    float temperature,
    bool sensorConnected
  );

  Configuration getConfiguration() const;

  bool saveConfiguration(
    const Configuration& configuration
  );

  void acknowledgeAll();

  uint8_t getActiveAlarmCount() const;
  bool hasActiveAlarm() const;
  bool hasUnacknowledgedAlarm() const;

  AlarmState getAlarmState(
    AlarmId id
  ) const;

  const char* getAlarmName(
    AlarmId id
  ) const;

  String getSummaryText() const;

private:
  RelayModule& relays;
  ControlModule& control;

  Preferences preferences;
  bool initialized;

  Configuration configuration;

  static constexpr uint8_t
    ALARM_COUNT = 4;

  AlarmState alarmStates[
    ALARM_COUNT
  ];

  enum class ResponseDirection {
    NONE,
    COOLING,
    HEATING
  };

  ResponseDirection responseDirection;

  float responseStartTemperature;
  unsigned long responseStartMs;

  static constexpr const char*
    STORAGE_NAMESPACE =
      "mwalarms";

  void loadConfiguration();
  bool validateConfiguration(
    const Configuration& candidate
  ) const;

  void setAlarm(
    AlarmId id,
    bool active
  );

  void updateResponseAlarm(
    float temperature,
    bool sensorConnected
  );

  uint8_t indexFor(
    AlarmId id
  ) const;
};

#endif
