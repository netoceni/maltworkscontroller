#ifndef HISTORYMODULE_H
#define HISTORYMODULE_H

#include <Arduino.h>

class HistoryModule {
public:
  static constexpr uint16_t MAX_POINTS =
    1440;

  HistoryModule();

  void begin();

  void addPoint(
    float temperature,
    float setpoint,
    bool temperatureValid
  );

  uint16_t getPointCount() const;

  bool getPoint(
    uint16_t chronologicalIndex,
    float& temperature,
    float& setpoint,
    bool& temperatureValid,
    unsigned long& ageSeconds
  ) const;

  void clear();

private:
  struct HistoryPoint {
    float temperature;
    float setpoint;
    unsigned long timestamp;
    bool temperatureValid;
  };

  HistoryPoint points[MAX_POINTS];

  uint16_t pointCount;
  uint16_t nextWriteIndex;

  uint16_t getPhysicalIndex(
    uint16_t chronologicalIndex
  ) const;
};

#endif
