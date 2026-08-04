#ifndef CLOCKMODULE_H
#define CLOCKMODULE_H

#include <Arduino.h>
#include <time.h>

#include "networkmodule.h"

class ClockModule {
public:
  explicit ClockModule(
    NetworkModule& networkModule
  );

  void begin();
  void update();

  bool isSynchronized() const;
  bool isSynchronizationPending() const;

  time_t getEpoch() const;

  String getDateText() const;
  String getTimeText() const;
  String getDateTimeText() const;
  String getStatusText() const;
  String getLastSynchronizationText() const;

private:
  NetworkModule& network;

  bool ntpConfigured;
  bool synchronized;
  bool synchronizationPending;
  bool connectionWasAvailable;

  time_t lastSynchronizationEpoch;

  unsigned long lastConfigurationAttempt;
  unsigned long lastStatusCheck;
  unsigned long lastPeriodicResync;

  static constexpr unsigned long
    CONFIGURATION_RETRY_INTERVAL_MS =
      30UL * 1000UL;

  static constexpr unsigned long
    STATUS_CHECK_INTERVAL_MS =
      1000UL;

  static constexpr unsigned long
    PERIODIC_RESYNC_INTERVAL_MS =
      6UL * 60UL * 60UL * 1000UL;

  static constexpr time_t
    MINIMUM_VALID_EPOCH =
      1704067200;

  static constexpr const char*
    TIMEZONE_RULE =
      "<-03>3";

  static constexpr const char*
    NTP_SERVER_1 =
      "pool.ntp.org";

  static constexpr const char*
    NTP_SERVER_2 =
      "time.google.com";

  static constexpr const char*
    NTP_SERVER_3 =
      "time.cloudflare.com";

  void configureNtp();
  void checkSynchronization();

  String formatTime(
    const char* format
  ) const;
};

#endif
