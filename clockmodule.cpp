#include "clockmodule.h"

ClockModule::ClockModule(
  NetworkModule& networkModule
) :
  network(networkModule),
  ntpConfigured(false),
  synchronized(false),
  synchronizationPending(false),
  connectionWasAvailable(false),
  lastSynchronizationEpoch(0),
  lastConfigurationAttempt(0),
  lastStatusCheck(0),
  lastPeriodicResync(0) {
}

void ClockModule::begin() {
  Serial.println(
    "Modulo de relogio inicializado."
  );

  Serial.println(
    "Fuso horario: UTC-03:00"
  );

  if (
    network.isStationConnected()
  ) {
    configureNtp();
  }
}

void ClockModule::update() {
  bool connectionAvailable =
    network.isStationConnected();

  if (
    connectionAvailable &&
    !connectionWasAvailable
  ) {
    configureNtp();
  }

  connectionWasAvailable =
    connectionAvailable;

  if (
    connectionAvailable &&
    !synchronized &&
    millis() - lastConfigurationAttempt >=
      CONFIGURATION_RETRY_INTERVAL_MS
  ) {
    configureNtp();
  }

  if (
    connectionAvailable &&
    synchronized &&
    millis() - lastPeriodicResync >=
      PERIODIC_RESYNC_INTERVAL_MS
  ) {
    configureNtp();
    lastPeriodicResync =
      millis();
  }

  if (
    millis() - lastStatusCheck >=
    STATUS_CHECK_INTERVAL_MS
  ) {
    lastStatusCheck =
      millis();

    checkSynchronization();
  }
}

bool ClockModule::
isSynchronized() const {
  return synchronized;
}

bool ClockModule::
isSynchronizationPending() const {
  return synchronizationPending;
}

time_t ClockModule::getEpoch() const {
  time_t currentTime =
    time(nullptr);

  if (
    currentTime <
    MINIMUM_VALID_EPOCH
  ) {
    return 0;
  }

  return currentTime;
}

String ClockModule::getDateText() const {
  return formatTime(
    "%d/%m/%Y"
  );
}

String ClockModule::getTimeText() const {
  return formatTime(
    "%H:%M:%S"
  );
}

String ClockModule::
getDateTimeText() const {
  return formatTime(
    "%d/%m/%Y %H:%M:%S"
  );
}

String ClockModule::
getStatusText() const {
  if (synchronized) {
    return "SINCRONIZADO";
  }

  if (
    synchronizationPending &&
    network.isStationConnected()
  ) {
    return "SINCRONIZANDO";
  }

  if (
    !network.isStationConnected()
  ) {
    return "AGUARDANDO INTERNET";
  }

  return "NAO SINCRONIZADO";
}

String ClockModule::
getLastSynchronizationText() const {
  if (
    lastSynchronizationEpoch <
    MINIMUM_VALID_EPOCH
  ) {
    return "--";
  }

  struct tm timeInfo;

  if (
    !localtime_r(
      &lastSynchronizationEpoch,
      &timeInfo
    )
  ) {
    return "--";
  }

  char buffer[24];

  if (
    strftime(
      buffer,
      sizeof(buffer),
      "%d/%m/%Y %H:%M:%S",
      &timeInfo
    ) == 0
  ) {
    return "--";
  }

  return String(buffer);
}

void ClockModule::configureNtp() {
  if (
    !network.isStationConnected()
  ) {
    return;
  }

  lastConfigurationAttempt =
    millis();

  synchronizationPending =
    true;

  Serial.println();
  Serial.println(
    "Solicitando horario aos servidores NTP..."
  );

  configTzTime(
    TIMEZONE_RULE,
    NTP_SERVER_1,
    NTP_SERVER_2,
    NTP_SERVER_3
  );

  ntpConfigured =
    true;
}

void ClockModule::
checkSynchronization() {
  time_t currentTime =
    time(nullptr);

  bool validTime =
    currentTime >=
    MINIMUM_VALID_EPOCH;

  if (
    validTime &&
    !synchronized
  ) {
    synchronized =
      true;

    synchronizationPending =
      false;

    lastSynchronizationEpoch =
      currentTime;

    lastPeriodicResync =
      millis();

    Serial.println();
    Serial.print(
      "Relogio sincronizado: "
    );

    Serial.println(
      getDateTimeText()
    );

    return;
  }

  if (
    validTime &&
    synchronized
  ) {
    return;
  }

  synchronized =
    false;

  if (
    !network.isStationConnected()
  ) {
    synchronizationPending =
      false;
  }
}

String ClockModule::formatTime(
  const char* format
) const {
  if (!synchronized) {
    return "--";
  }

  time_t currentTime =
    time(nullptr);

  if (
    currentTime <
    MINIMUM_VALID_EPOCH
  ) {
    return "--";
  }

  struct tm timeInfo;

  if (
    !localtime_r(
      &currentTime,
      &timeInfo
    )
  ) {
    return "--";
  }

  char buffer[32];

  if (
    strftime(
      buffer,
      sizeof(buffer),
      format,
      &timeInfo
    ) == 0
  ) {
    return "--";
  }

  return String(buffer);
}
