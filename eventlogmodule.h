#ifndef EVENTLOGMODULE_H
#define EVENTLOGMODULE_H

#include <Arduino.h>
#include <Preferences.h>
#include <time.h>

#include "clockmodule.h"

class EventLogModule {
public:
  static constexpr uint16_t
    MAX_EVENTS = 20;

  static constexpr uint8_t
    MESSAGE_LENGTH = 72;

  enum class Category : uint8_t {
    SYSTEM = 0,
    RECIPE = 1,
    CONTROL = 2,
    ALARM = 3,
    OTA = 4,
    NETWORK = 5,
    CLOUD = 6
  };

  struct Event {
    uint32_t epoch;
    uint32_t uptimeSeconds;
    Category category;
    char message[
      MESSAGE_LENGTH
    ];
  };

  explicit EventLogModule(
    ClockModule& clockModule
  );

  bool begin();

  bool add(
    Category category,
    const String& message
  );

  void clear();

  uint16_t getCount() const;

  bool getEvent(
    uint16_t newestFirstIndex,
    Event& event
  ) const;

  const char* getCategoryText(
    Category category
  ) const;

  String formatDateTime(
    const Event& event
  ) const;

private:
  ClockModule& clock;
  Preferences preferences;

  Event events[MAX_EVENTS];

  uint16_t eventCount;
  uint16_t nextWriteIndex;
  bool initialized;

  static constexpr const char*
    STORAGE_NAMESPACE =
      "mwevents";

  static constexpr uint32_t
    STORAGE_VERSION = 2;

  struct StorageHeader {
    uint32_t version;
    uint16_t count;
    uint16_t nextIndex;
  };

  bool load();
  bool persist();

  uint16_t physicalIndexFor(
    uint16_t newestFirstIndex
  ) const;
};

#endif
