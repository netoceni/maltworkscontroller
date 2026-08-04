#include <string.h>

#include "eventlogmodule.h"

EventLogModule::EventLogModule(
  ClockModule& clockModule
) :
  clock(clockModule),
  eventCount(0),
  nextWriteIndex(0),
  initialized(false) {
  memset(
    events,
    0,
    sizeof(events)
  );
}

bool EventLogModule::begin() {
  initialized =
    preferences.begin(
      STORAGE_NAMESPACE,
      false
    );

  if (!initialized) {
    Serial.println(
      "Falha ao inicializar o historico de eventos."
    );

    return false;
  }

  /*
    A versao anterior persistia sempre o bloco de
    100 eventos. Antes de criar o formato compacto,
    remove somente esse historico descartavel para
    liberar as paginas NVS ocupadas pelo blob antigo.
  */
  size_t storedDataLength =
    preferences.getBytesLength(
      "data"
    );

  if (
    storedDataLength > 0 &&
    storedDataLength != sizeof(events)
  ) {
    Serial.println(
      "Liberando historico de eventos NVS antigo."
    );

    preferences.clear();
    preferences.end();

    initialized =
      preferences.begin(
        STORAGE_NAMESPACE,
        false
      );

    if (!initialized) {
      Serial.println(
        "Falha ao reabrir o historico de eventos."
      );

      return false;
    }
  }

  load();

  Serial.print(
    "Historico de eventos inicializado. Eventos: "
  );

  Serial.println(
    eventCount
  );

  return true;
}

bool EventLogModule::add(
  Category category,
  const String& message
) {
  if (!initialized) {
    return false;
  }

  Event& event =
    events[nextWriteIndex];

  event.epoch =
    static_cast<uint32_t>(
      clock.getEpoch()
    );

  event.uptimeSeconds =
    millis() / 1000UL;

  event.category =
    category;

  String normalized =
    message;

  normalized.replace(
    "\r",
    " "
  );

  normalized.replace(
    "\n",
    " "
  );

  normalized.trim();

  strncpy(
    event.message,
    normalized.c_str(),
    MESSAGE_LENGTH - 1
  );

  event.message[
    MESSAGE_LENGTH - 1
  ] = '\0';

  nextWriteIndex =
    (
      nextWriteIndex + 1
    ) % MAX_EVENTS;

  if (
    eventCount <
    MAX_EVENTS
  ) {
    eventCount++;
  }

  Serial.print(
    "EVENTO ["
  );

  Serial.print(
    getCategoryText(category)
  );

  Serial.print(
    "]: "
  );

  Serial.println(
    event.message
  );

  return persist();
}

void EventLogModule::clear() {
  eventCount = 0;
  nextWriteIndex = 0;

  memset(
    events,
    0,
    sizeof(events)
  );

  preferences.remove(
    "header"
  );

  preferences.remove(
    "data"
  );
}

uint16_t EventLogModule::
getCount() const {
  return eventCount;
}

bool EventLogModule::getEvent(
  uint16_t newestFirstIndex,
  Event& event
) const {
  if (
    newestFirstIndex >=
    eventCount
  ) {
    return false;
  }

  event =
    events[
      physicalIndexFor(
        newestFirstIndex
      )
    ];

  return true;
}

const char*
EventLogModule::getCategoryText(
  Category category
) const {
  switch (category) {
    case Category::SYSTEM:
      return "Sistema";

    case Category::RECIPE:
      return "Receita";

    case Category::CONTROL:
      return "Controle";

    case Category::ALARM:
      return "Alarme";

    case Category::OTA:
      return "OTA";

    case Category::NETWORK:
      return "Rede";

    case Category::CLOUD:
      return "Nuvem";
  }

  return "Outro";
}

String EventLogModule::formatDateTime(
  const Event& event
) const {
  if (
    event.epoch > 1704067200UL
  ) {
    time_t epoch =
      static_cast<time_t>(
        event.epoch
      );

    struct tm timeInfo;

    if (
      localtime_r(
        &epoch,
        &timeInfo
      )
    ) {
      char buffer[24];

      if (
        strftime(
          buffer,
          sizeof(buffer),
          "%d/%m/%Y %H:%M:%S",
          &timeInfo
        ) > 0
      ) {
        return String(buffer);
      }
    }
  }

  uint32_t seconds =
    event.uptimeSeconds;

  uint32_t hours =
    seconds / 3600UL;

  uint32_t minutes =
    (
      seconds % 3600UL
    ) / 60UL;

  uint32_t remainingSeconds =
    seconds % 60UL;

  char buffer[28];

  snprintf(
    buffer,
    sizeof(buffer),
    "Uptime %luh %02lum %02lus",
    static_cast<unsigned long>(
      hours
    ),
    static_cast<unsigned long>(
      minutes
    ),
    static_cast<unsigned long>(
      remainingSeconds
    )
  );

  return String(buffer);
}

bool EventLogModule::load() {
  size_t headerLength =
    preferences.getBytesLength(
      "header"
    );

  size_t dataLength =
    preferences.getBytesLength(
      "data"
    );

  if (
    headerLength !=
      sizeof(StorageHeader) ||
    dataLength !=
      sizeof(events)
  ) {
    eventCount = 0;
    nextWriteIndex = 0;

    return false;
  }

  StorageHeader header;

  preferences.getBytes(
    "header",
    &header,
    sizeof(header)
  );

  if (
    header.version !=
      STORAGE_VERSION ||
    header.count >
      MAX_EVENTS ||
    header.nextIndex >=
      MAX_EVENTS
  ) {
    eventCount = 0;
    nextWriteIndex = 0;

    return false;
  }

  preferences.getBytes(
    "data",
    events,
    sizeof(events)
  );

  eventCount =
    header.count;

  nextWriteIndex =
    header.nextIndex;

  return true;
}

bool EventLogModule::persist() {
  StorageHeader header;

  header.version =
    STORAGE_VERSION;

  header.count =
    eventCount;

  header.nextIndex =
    nextWriteIndex;

  size_t headerBytes =
    preferences.putBytes(
      "header",
      &header,
      sizeof(header)
    );

  size_t dataBytes =
    preferences.putBytes(
      "data",
      events,
      sizeof(events)
    );

  return
    headerBytes ==
      sizeof(header) &&
    dataBytes ==
      sizeof(events);
}

uint16_t EventLogModule::
physicalIndexFor(
  uint16_t newestFirstIndex
) const {
  int32_t index =
    static_cast<int32_t>(
      nextWriteIndex
    ) -
    1 -
    newestFirstIndex;

  while (index < 0) {
    index +=
      MAX_EVENTS;
  }

  return static_cast<uint16_t>(
    index
  );
}
