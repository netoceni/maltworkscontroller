#include <math.h>

#include "historymodule.h"

HistoryModule::HistoryModule() :
  pointCount(0),
  nextWriteIndex(0) {
}

void HistoryModule::begin() {
  clear();

  Serial.println(
    "Modulo de historico inicializado."
  );

  Serial.print(
    "Capacidade do historico: "
  );

  Serial.print(MAX_POINTS);
  Serial.println(" pontos.");
}

void HistoryModule::addPoint(
  float temperature,
  float setpoint,
  bool temperatureValid
) {
  HistoryPoint& point =
    points[nextWriteIndex];

  point.temperature =
    temperatureValid
      ? temperature
      : NAN;

  point.setpoint =
    setpoint;

  point.timestamp =
    millis();

  point.temperatureValid =
    temperatureValid;

  nextWriteIndex++;

  if (
    nextWriteIndex >=
    MAX_POINTS
  ) {
    nextWriteIndex = 0;
  }

  if (
    pointCount <
    MAX_POINTS
  ) {
    pointCount++;
  }
}

uint16_t HistoryModule::
getPointCount() const {
  return pointCount;
}

uint16_t HistoryModule::getPhysicalIndex(
  uint16_t chronologicalIndex
) const {
  if (
    pointCount <
    MAX_POINTS
  ) {
    return chronologicalIndex;
  }

  return (
    nextWriteIndex +
    chronologicalIndex
  ) % MAX_POINTS;
}

bool HistoryModule::getPoint(
  uint16_t chronologicalIndex,
  float& temperature,
  float& setpoint,
  bool& temperatureValid,
  unsigned long& ageSeconds
) const {
  if (
    chronologicalIndex >=
    pointCount
  ) {
    return false;
  }

  uint16_t physicalIndex =
    getPhysicalIndex(
      chronologicalIndex
    );

  const HistoryPoint& point =
    points[physicalIndex];

  temperature =
    point.temperature;

  setpoint =
    point.setpoint;

  temperatureValid =
    point.temperatureValid;

  ageSeconds =
    (
      millis() -
      point.timestamp
    ) / 1000UL;

  return true;
}

void HistoryModule::clear() {
  pointCount = 0;
  nextWriteIndex = 0;

  for (
    uint16_t index = 0;
    index < MAX_POINTS;
    index++
  ) {
    points[index].temperature =
      NAN;

    points[index].setpoint =
      NAN;

    points[index].timestamp =
      0;

    points[index].temperatureValid =
      false;
  }
}
