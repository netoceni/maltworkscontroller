#ifndef TEMPERATUREMODULE_H
#define TEMPERATUREMODULE_H

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Preferences.h>

class TemperatureModule {
public:
  explicit TemperatureModule(
    uint8_t dataPin
  );

  bool begin();
  void update();

  float getRefrigeratorTemperature() const;
  float getRefrigeratorRawTemperature() const;

  float getThermalWellTemperature() const;
  float getThermalWellRawTemperature() const;

  bool isRefrigeratorSensorConnected() const;
  bool isThermalWellSensorConnected() const;

  uint8_t getSensorCount() const;

  float getRefrigeratorOffset() const;
  float getThermalWellOffset() const;

  bool saveCalibration(
    float refrigeratorOffset,
    float thermalWellOffset
  );

  bool resetCalibration();

private:
  OneWire oneWire;
  DallasTemperature sensors;
  Preferences preferences;

  float refrigeratorRawTemperature;
  float thermalWellRawTemperature;

  float refrigeratorOffset;
  float thermalWellOffset;

  bool refrigeratorSensorConnected;
  bool thermalWellSensorConnected;
  bool calibrationStorageAvailable;

  uint8_t sensorCount;

  static constexpr const char*
    STORAGE_NAMESPACE =
      "mwcalibration";

  static constexpr float
    MINIMUM_OFFSET = -10.0f;

  static constexpr float
    MAXIMUM_OFFSET = 10.0f;

  bool isValidTemperature(
    float temperature
  ) const;

  float applyOffset(
    float rawTemperature,
    float offset,
    bool connected
  ) const;

  void loadCalibration();
};

#endif
