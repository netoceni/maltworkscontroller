#include <math.h>

#include "alarmmodule.h"

AlarmModule::AlarmModule(
  RelayModule& relayModule,
  ControlModule& controlModule
) :
  relays(relayModule),
  control(controlModule),
  initialized(false),
  responseDirection(
    ResponseDirection::NONE
  ),
  responseStartTemperature(NAN),
  responseStartMs(0) {
  for (
    uint8_t index = 0;
    index < ALARM_COUNT;
    index++
  ) {
    alarmStates[index].id =
      static_cast<AlarmId>(
        index
      );

    alarmStates[index].active =
      false;

    alarmStates[index].acknowledged =
      false;

    alarmStates[index].activeSinceMs =
      0;
  }
}

bool AlarmModule::begin() {
  initialized =
    preferences.begin(
      STORAGE_NAMESPACE,
      false
    );

  loadConfiguration();

  Serial.println(
    "Modulo de alarmes inicializado."
  );

  return initialized;
}

void AlarmModule::update(
  float temperature,
  bool sensorConnected
) {
  bool validTemperature =
    sensorConnected &&
    !isnan(temperature);

  setAlarm(
    AlarmId::SENSOR_FAILURE,
    configuration.sensorAlarmEnabled &&
    !validTemperature
  );

  setAlarm(
    AlarmId::HIGH_TEMPERATURE,
    configuration.highTemperatureEnabled &&
    validTemperature &&
    temperature >
      configuration.highTemperatureLimit
  );

  setAlarm(
    AlarmId::LOW_TEMPERATURE,
    configuration.lowTemperatureEnabled &&
    validTemperature &&
    temperature <
      configuration.lowTemperatureLimit
  );

  updateResponseAlarm(
    temperature,
    validTemperature
  );
}

AlarmModule::Configuration
AlarmModule::getConfiguration() const {
  return configuration;
}

bool AlarmModule::saveConfiguration(
  const Configuration& candidate
) {
  if (
    !initialized ||
    !validateConfiguration(
      candidate
    )
  ) {
    return false;
  }

  configuration =
    candidate;

  preferences.putBool(
    "sensor_en",
    configuration.sensorAlarmEnabled
  );

  preferences.putBool(
    "high_en",
    configuration.highTemperatureEnabled
  );

  preferences.putBool(
    "low_en",
    configuration.lowTemperatureEnabled
  );

  preferences.putBool(
    "resp_en",
    configuration.responseAlarmEnabled
  );

  preferences.putFloat(
    "high_lim",
    configuration.highTemperatureLimit
  );

  preferences.putFloat(
    "low_lim",
    configuration.lowTemperatureLimit
  );

  preferences.putFloat(
    "min_change",
    configuration.minimumExpectedChange
  );

  preferences.putULong(
    "resp_time",
    configuration.responseTimeoutSeconds
  );

  responseDirection =
    ResponseDirection::NONE;

  responseStartMs = 0;
  responseStartTemperature = NAN;

  setAlarm(
    AlarmId::NO_THERMAL_RESPONSE,
    false
  );

  return true;
}

void AlarmModule::acknowledgeAll() {
  for (
    uint8_t index = 0;
    index < ALARM_COUNT;
    index++
  ) {
    if (
      alarmStates[index].active
    ) {
      alarmStates[index].acknowledged =
        true;
    }
  }
}

uint8_t AlarmModule::
getActiveAlarmCount() const {
  uint8_t count = 0;

  for (
    uint8_t index = 0;
    index < ALARM_COUNT;
    index++
  ) {
    if (
      alarmStates[index].active
    ) {
      count++;
    }
  }

  return count;
}

bool AlarmModule::hasActiveAlarm() const {
  return getActiveAlarmCount() > 0;
}

bool AlarmModule::
hasUnacknowledgedAlarm() const {
  for (
    uint8_t index = 0;
    index < ALARM_COUNT;
    index++
  ) {
    if (
      alarmStates[index].active &&
      !alarmStates[index].acknowledged
    ) {
      return true;
    }
  }

  return false;
}

AlarmModule::AlarmState
AlarmModule::getAlarmState(
  AlarmId id
) const {
  return alarmStates[
    indexFor(id)
  ];
}

const char* AlarmModule::getAlarmName(
  AlarmId id
) const {
  switch (id) {
    case AlarmId::SENSOR_FAILURE:
      return "Falha no sensor";

    case AlarmId::HIGH_TEMPERATURE:
      return "Temperatura acima do limite";

    case AlarmId::LOW_TEMPERATURE:
      return "Temperatura abaixo do limite";

    case AlarmId::NO_THERMAL_RESPONSE:
      return "Sem resposta termica";
  }

  return "Alarme desconhecido";
}

String AlarmModule::getSummaryText() const {
  for (
    uint8_t index = 0;
    index < ALARM_COUNT;
    index++
  ) {
    if (
      alarmStates[index].active
    ) {
      return String(
        getAlarmName(
          alarmStates[index].id
        )
      );
    }
  }

  return "Nenhum alarme";
}

void AlarmModule::loadConfiguration() {
  configuration.sensorAlarmEnabled =
    initialized
      ? preferences.getBool(
          "sensor_en",
          true
        )
      : true;

  configuration.highTemperatureEnabled =
    initialized
      ? preferences.getBool(
          "high_en",
          true
        )
      : true;

  configuration.lowTemperatureEnabled =
    initialized
      ? preferences.getBool(
          "low_en",
          true
        )
      : true;

  configuration.responseAlarmEnabled =
    initialized
      ? preferences.getBool(
          "resp_en",
          true
        )
      : true;

  configuration.highTemperatureLimit =
    initialized
      ? preferences.getFloat(
          "high_lim",
          35.0f
        )
      : 35.0f;

  configuration.lowTemperatureLimit =
    initialized
      ? preferences.getFloat(
          "low_lim",
          -5.0f
        )
      : -5.0f;

  configuration.minimumExpectedChange =
    initialized
      ? preferences.getFloat(
          "min_change",
          0.5f
        )
      : 0.5f;

  configuration.responseTimeoutSeconds =
    initialized
      ? preferences.getULong(
          "resp_time",
          90UL * 60UL
        )
      : 90UL * 60UL;

  if (
    !validateConfiguration(
      configuration
    )
  ) {
    configuration.sensorAlarmEnabled =
      true;

    configuration.highTemperatureEnabled =
      true;

    configuration.lowTemperatureEnabled =
      true;

    configuration.responseAlarmEnabled =
      true;

    configuration.highTemperatureLimit =
      35.0f;

    configuration.lowTemperatureLimit =
      -5.0f;

    configuration.minimumExpectedChange =
      0.5f;

    configuration.responseTimeoutSeconds =
      90UL * 60UL;
  }
}

bool AlarmModule::validateConfiguration(
  const Configuration& candidate
) const {
  return
    candidate.highTemperatureLimit >
      candidate.lowTemperatureLimit &&
    candidate.highTemperatureLimit <=
      60.0f &&
    candidate.lowTemperatureLimit >=
      -30.0f &&
    candidate.minimumExpectedChange >=
      0.1f &&
    candidate.minimumExpectedChange <=
      10.0f &&
    candidate.responseTimeoutSeconds >=
      60UL &&
    candidate.responseTimeoutSeconds <=
      24UL * 60UL * 60UL;
}

void AlarmModule::setAlarm(
  AlarmId id,
  bool active
) {
  uint8_t index =
    indexFor(id);

  AlarmState& state =
    alarmStates[index];

  if (
    active &&
    !state.active
  ) {
    state.active = true;
    state.acknowledged = false;
    state.activeSinceMs = millis();

    Serial.print(
      "ALARME ATIVO: "
    );

    Serial.println(
      getAlarmName(id)
    );

    return;
  }

  if (
    !active &&
    state.active
  ) {
    state.active = false;
    state.acknowledged = false;
    state.activeSinceMs = 0;

    Serial.print(
      "ALARME NORMALIZADO: "
    );

    Serial.println(
      getAlarmName(id)
    );
  }
}

void AlarmModule::updateResponseAlarm(
  float temperature,
  bool sensorConnected
) {
  if (
    !configuration.responseAlarmEnabled ||
    !sensorConnected
  ) {
    responseDirection =
      ResponseDirection::NONE;

    responseStartMs = 0;
    responseStartTemperature = NAN;

    setAlarm(
      AlarmId::NO_THERMAL_RESPONSE,
      false
    );

    return;
  }

  ResponseDirection currentDirection =
    ResponseDirection::NONE;

  if (relays.isCoolingOn()) {
    currentDirection =
      ResponseDirection::COOLING;
  } else if (relays.isHeatingOn()) {
    currentDirection =
      ResponseDirection::HEATING;
  }

  if (
    currentDirection ==
    ResponseDirection::NONE
  ) {
    responseDirection =
      ResponseDirection::NONE;

    responseStartMs = 0;
    responseStartTemperature = NAN;

    setAlarm(
      AlarmId::NO_THERMAL_RESPONSE,
      false
    );

    return;
  }

  if (
    currentDirection !=
      responseDirection ||
    isnan(responseStartTemperature)
  ) {
    responseDirection =
      currentDirection;

    responseStartTemperature =
      temperature;

    responseStartMs =
      millis();

    setAlarm(
      AlarmId::NO_THERMAL_RESPONSE,
      false
    );

    return;
  }

  unsigned long elapsedSeconds =
    (
      millis() -
      responseStartMs
    ) / 1000UL;

  float measuredChange = 0.0f;

  if (
    responseDirection ==
    ResponseDirection::COOLING
  ) {
    measuredChange =
      responseStartTemperature -
      temperature;
  } else {
    measuredChange =
      temperature -
      responseStartTemperature;
  }

  if (
    measuredChange >=
      configuration.minimumExpectedChange
  ) {
    /*
      Houve resposta. Reinicia a janela para
      continuar supervisionando a saída.
    */
    responseStartTemperature =
      temperature;

    responseStartMs =
      millis();

    setAlarm(
      AlarmId::NO_THERMAL_RESPONSE,
      false
    );

    return;
  }

  setAlarm(
    AlarmId::NO_THERMAL_RESPONSE,
    elapsedSeconds >=
      configuration.responseTimeoutSeconds
  );
}

uint8_t AlarmModule::indexFor(
  AlarmId id
) const {
  return static_cast<uint8_t>(
    id
  );
}
