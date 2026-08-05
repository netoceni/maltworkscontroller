#include <math.h>

#include "temperaturemodule.h"

TemperatureModule::TemperatureModule(
  uint8_t dataPin
) :
  oneWire(dataPin),
  sensors(&oneWire),
  refrigeratorRawTemperature(NAN),
  thermalWellRawTemperature(NAN),
  refrigeratorOffset(0.0f),
  thermalWellOffset(0.0f),
  refrigeratorSensorConnected(false),
  thermalWellSensorConnected(false),
  calibrationStorageAvailable(false),
  sensorCount(0) {
}

bool TemperatureModule::begin() {
  calibrationStorageAvailable =
    preferences.begin(
      STORAGE_NAMESPACE,
      false
    );

  loadCalibration();

  sensors.begin();

  sensorCount =
    sensors.getDeviceCount();

  Serial.print(
    "Sensores DS18B20 encontrados: "
  );

  Serial.println(sensorCount);

  if (sensorCount == 0) {
    refrigeratorRawTemperature = NAN;
    thermalWellRawTemperature = NAN;

    refrigeratorSensorConnected = false;
    thermalWellSensorConnected = false;

    return false;
  }

  sensors.setResolution(12);

  update();

  return refrigeratorSensorConnected;
}

void TemperatureModule::update() {
  sensorCount =
    sensors.getDeviceCount();

  if (sensorCount == 0) {
    refrigeratorRawTemperature = NAN;
    thermalWellRawTemperature = NAN;

    refrigeratorSensorConnected = false;
    thermalWellSensorConnected = false;

    return;
  }

  sensors.requestTemperatures();

  float refrigeratorReading =
    sensors.getTempCByIndex(0);

  if (
    isValidTemperature(
      refrigeratorReading
    )
  ) {
    refrigeratorRawTemperature =
      refrigeratorReading;

    refrigeratorSensorConnected =
      true;
  } else {
    refrigeratorRawTemperature = NAN;
    refrigeratorSensorConnected = false;
  }

  if (sensorCount >= 2) {
    float thermalWellReading =
      sensors.getTempCByIndex(1);

    if (
      isValidTemperature(
        thermalWellReading
      )
    ) {
      thermalWellRawTemperature =
        thermalWellReading;

      thermalWellSensorConnected =
        true;
    } else {
      thermalWellRawTemperature = NAN;
      thermalWellSensorConnected = false;
    }
  } else {
    thermalWellRawTemperature = NAN;
    thermalWellSensorConnected = false;
  }
}

float TemperatureModule::
getRefrigeratorTemperature() const {
  return applyOffset(
    refrigeratorRawTemperature,
    refrigeratorOffset,
    refrigeratorSensorConnected
  );
}

float TemperatureModule::
getRefrigeratorRawTemperature() const {
  return refrigeratorRawTemperature;
}

float TemperatureModule::
getThermalWellTemperature() const {
  return applyOffset(
    thermalWellRawTemperature,
    thermalWellOffset,
    thermalWellSensorConnected
  );
}

float TemperatureModule::
getThermalWellRawTemperature() const {
  return thermalWellRawTemperature;
}

bool TemperatureModule::
isRefrigeratorSensorConnected() const {
  return refrigeratorSensorConnected;
}

bool TemperatureModule::
isThermalWellSensorConnected() const {
  return thermalWellSensorConnected;
}

uint8_t TemperatureModule::
getSensorCount() const {
  return sensorCount;
}

float TemperatureModule::
getRefrigeratorOffset() const {
  return refrigeratorOffset;
}

float TemperatureModule::
getThermalWellOffset() const {
  return thermalWellOffset;
}

bool TemperatureModule::saveCalibration(
  float newRefrigeratorOffset,
  float newThermalWellOffset
) {
  if (
    newRefrigeratorOffset <
      MINIMUM_OFFSET ||
    newRefrigeratorOffset >
      MAXIMUM_OFFSET ||
    newThermalWellOffset <
      MINIMUM_OFFSET ||
    newThermalWellOffset >
      MAXIMUM_OFFSET
  ) {
    return false;
  }

  refrigeratorOffset =
    newRefrigeratorOffset;

  thermalWellOffset =
    newThermalWellOffset;

  if (!calibrationStorageAvailable) {
    return false;
  }

  size_t refrigeratorBytes =
    preferences.putFloat(
      "fridge",
      refrigeratorOffset
    );

  size_t thermalWellBytes =
    preferences.putFloat(
      "well",
      thermalWellOffset
    );

  return
    refrigeratorBytes > 0 &&
    thermalWellBytes > 0;
}

bool TemperatureModule::
resetCalibration() {
  return saveCalibration(
    0.0f,
    0.0f
  );
}

bool TemperatureModule::isValidTemperature(
  float temperature
) const {
  if (isnan(temperature)) {
    return false;
  }

  if (
    temperature ==
    DEVICE_DISCONNECTED_C
  ) {
    return false;
  }

  if (
    temperature < -55.0f ||
    temperature > 125.0f
  ) {
    return false;
  }

  return true;
}

float TemperatureModule::applyOffset(
  float rawTemperature,
  float offset,
  bool connected
) const {
  if (
    !connected ||
    isnan(rawTemperature)
  ) {
    return NAN;
  }

  return rawTemperature +
    offset;
}

void TemperatureModule::
loadCalibration() {
  if (!calibrationStorageAvailable) {
    refrigeratorOffset = 0.0f;
    thermalWellOffset = 0.0f;

    return;
  }

  refrigeratorOffset =
    preferences.getFloat(
      "fridge",
      0.0f
    );

  thermalWellOffset =
    preferences.getFloat(
      "well",
      0.0f
    );

  if (
    refrigeratorOffset <
      MINIMUM_OFFSET ||
    refrigeratorOffset >
      MAXIMUM_OFFSET
  ) {
    refrigeratorOffset = 0.0f;
  }

  if (
    thermalWellOffset <
      MINIMUM_OFFSET ||
    thermalWellOffset >
      MAXIMUM_OFFSET
  ) {
    thermalWellOffset = 0.0f;
  }

  Serial.print(
    "Offset geladeira: "
  );

  Serial.println(
    refrigeratorOffset,
    2
  );

  Serial.print(
    "Offset poco termico: "
  );

  Serial.println(
    thermalWellOffset,
    2
  );
}
