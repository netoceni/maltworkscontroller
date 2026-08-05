#include "networkmodule.h"

NetworkModule::NetworkModule() :
  stationSsid(""),
  stationPassword(""),
  accessPointStarted(false),
  connectionPreviouslyReported(false),
  lastConnectionAttempt(0) {
}

bool NetworkModule::begin(
  const char* accessPointName,
  const char* accessPointPassword,
  const String& savedStationSsid,
  const String& savedStationPassword
) {
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.mode(WIFI_AP_STA);

  accessPointStarted =
    WiFi.softAP(
      accessPointName,
      accessPointPassword
    );

  if (accessPointStarted) {
    Serial.println(
      "Rede de contingencia inicializada."
    );

    Serial.print(
      "IP da rede propria: "
    );

    Serial.println(
      WiFi.softAPIP()
    );
  } else {
    Serial.println(
      "Falha ao criar a rede de contingencia."
    );
  }

  stationSsid =
    savedStationSsid;

  stationPassword =
    savedStationPassword;

  stationSsid.trim();

  if (hasStationCredentials()) {
    startStationConnection();
  } else {
    Serial.println(
      "Wi-Fi domestico ainda nao configurado."
    );
  }

  return accessPointStarted;
}

void NetworkModule::update() {
  if (!hasStationCredentials()) {
    return;
  }

  bool connected =
    isStationConnected();

  if (
    connected &&
    !connectionPreviouslyReported
  ) {
    connectionPreviouslyReported =
      true;

    Serial.println();
    Serial.println(
      "Wi-Fi domestico conectado."
    );

    Serial.print(
      "Rede: "
    );

    Serial.println(
      WiFi.SSID()
    );

    Serial.print(
      "IP local: "
    );

    Serial.println(
      WiFi.localIP()
    );
  }

  if (
    !connected &&
    connectionPreviouslyReported
  ) {
    connectionPreviouslyReported =
      false;

    Serial.println();
    Serial.println(
      "Wi-Fi domestico desconectado."
    );
  }

  if (
    connected ||
    millis() - lastConnectionAttempt <
      RECONNECT_INTERVAL_MS
  ) {
    return;
  }

  startStationConnection();
}

bool NetworkModule::connectStation(
  const String& ssid,
  const String& password
) {
  String normalizedSsid =
    ssid;

  normalizedSsid.trim();

  if (
    normalizedSsid.length() == 0 ||
    normalizedSsid.length() > 32
  ) {
    return false;
  }

  stationSsid =
    normalizedSsid;

  stationPassword =
    password;

  connectionPreviouslyReported =
    false;

  startStationConnection();

  return true;
}

void NetworkModule::disconnectStation() {
  stationSsid = "";
  stationPassword = "";

  WiFi.disconnect(
    false,
    true
  );

  connectionPreviouslyReported =
    false;

  Serial.println(
    "Credenciais do Wi-Fi domestico removidas."
  );
}

bool NetworkModule::
isAccessPointStarted() const {
  return accessPointStarted;
}

bool NetworkModule::
hasStationCredentials() const {
  return stationSsid.length() > 0;
}

bool NetworkModule::
isStationConnected() const {
  return WiFi.status() ==
    WL_CONNECTED;
}

String NetworkModule::
getStationStatusText() const {
  if (!hasStationCredentials()) {
    return "NAO CONFIGURADO";
  }

  switch (WiFi.status()) {
    case WL_CONNECTED:
      return "CONECTADO";

    case WL_NO_SSID_AVAIL:
      return "REDE NAO ENCONTRADA";

    case WL_CONNECT_FAILED:
      return "FALHA DE AUTENTICACAO";

    case WL_CONNECTION_LOST:
      return "CONEXAO PERDIDA";

    case WL_DISCONNECTED:
      return "CONECTANDO";

    case WL_IDLE_STATUS:
      return "CONECTANDO";

    default:
      return "CONECTANDO";
  }
}

String NetworkModule::
getStationSsid() const {
  return stationSsid;
}

IPAddress NetworkModule::
getAccessPointIp() const {
  return WiFi.softAPIP();
}

IPAddress NetworkModule::
getStationIp() const {
  if (!isStationConnected()) {
    return IPAddress(
      0,
      0,
      0,
      0
    );
  }

  return WiFi.localIP();
}

int32_t NetworkModule::
getStationRssi() const {
  if (!isStationConnected()) {
    return 0;
  }

  return WiFi.RSSI();
}

uint8_t NetworkModule::
getConnectedAccessPointClients() const {
  return WiFi.softAPgetStationNum();
}

void NetworkModule::
startStationConnection() {
  if (!hasStationCredentials()) {
    return;
  }

  lastConnectionAttempt =
    millis();

  Serial.print(
    "Tentando conectar ao Wi-Fi: "
  );

  Serial.println(
    stationSsid
  );

  WiFi.disconnect(
    false,
    false
  );

  if (
    stationPassword.length() == 0
  ) {
    WiFi.begin(
      stationSsid.c_str()
    );
  } else {
    WiFi.begin(
      stationSsid.c_str(),
      stationPassword.c_str()
    );
  }
}
